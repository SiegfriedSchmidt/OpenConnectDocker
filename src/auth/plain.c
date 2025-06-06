/*
 * Copyright (C) 2013-2015 Nikos Mavrogiannopoulos
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This file is part of ocserv.
 *
 * ocserv is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * ocserv is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vpn.h>
#include <ctype.h>
#include "plain.h"
#include "common-config.h"
#include "auth/common.h"
#include <ccan/htable/htable.h>
#include <ccan/hash/hash.h>
#ifdef HAVE_LIBOATH
#include <liboath/oath.h>
#endif
#ifdef HAVE_CRYPT_H
/* libcrypt in Fedora28 does not provide prototype
   * in unistd.h */
#include <crypt.h>
#endif
#include "log.h"

#define MAX_CPASS_SIZE 128
#define HOTP_WINDOW 20

struct plain_ctx_st {
	char username[MAX_USERNAME_SIZE];
	char cpass[MAX_CPASS_SIZE]; /* crypt() passwd */

	char *groupnames[MAX_GROUPS];
	unsigned int groupnames_size;

	const char *pass_msg;
	unsigned int retries;
	unsigned int failed; /* non-zero if the username is wrong */

	const struct plain_cfg_st *config;
};

static void plain_vhost_init(void **vctx, void *pool, void *additional)
{
	struct plain_cfg_st *config = additional;

	/* vctx is plain_cfg_st */

	if (config == NULL) {
		fprintf(stderr, "plain: no configuration passed!\n");
		exit(EXIT_FAILURE);
	}

	*vctx = (void *)config;

#ifdef HAVE_LIBOATH
	oath_init();
#endif
}

/* Breaks a list of "xxx", "yyy", to a character array, of
 * MAX_COMMA_SEP_ELEMENTS size; Note that the given string is modified.
  */
static void break_group_list(void *pool, char *text,
			     char *broken_text[MAX_GROUPS],
			     unsigned int *elements)
{
	char *p = talloc_strdup(pool, text);
	char *p2;
	unsigned int len;

	*elements = 0;

	if (p == NULL)
		return;

	do {
		broken_text[*elements] = p;
		(*elements)++;

		p = strchr(p, ',');
		if (p) {
			*p = 0;
			len = p - broken_text[*elements - 1];

			/* remove any trailing space */
			p2 = p - 1;
			while (isspace(*p2)) {
				*p2 = 0;
				p2--;
			}

			p++; /* move to next entry and skip white
				 * space.
				 */
			while (isspace(*p))
				p++;

			if (len == 1) {
				/* skip the group */
				(*elements)--;
			}
		} else {
			p2 = strrchr(broken_text[(*elements) - 1], ' ');
			if (p2 != NULL) {
				while (isspace(*p2)) {
					*p2 = 0;
					p2--;
				}
			}

			if (strlen(broken_text[(*elements) - 1]) == 1) {
				/* skip the group */
				(*elements)--;
			}
		}
	} while (p != NULL && *elements < MAX_GROUPS);
}

/* Returns 0 if the user is successfully authenticated, and sets the appropriate group name.
 */
static int read_auth_pass(struct plain_ctx_st *pctx)
{
	FILE *fp;
	char line[512];
	ssize_t ll;
	char *p, *sp;
	int ret;

	if (pctx->config->passwd == NULL) {
		/* no password file is set */
		return 0;
	}

	pctx->failed = 1;

	fp = fopen(pctx->config->passwd, "r");
	if (fp == NULL) {
		oc_syslog(LOG_ERR,
			  "error in plain authentication; cannot open: %s",
			  pctx->config->passwd);
		return -1;
	}

	line[sizeof(line) - 1] = 0;
	while ((p = fgets(line, sizeof(line) - 1, fp)) != NULL) {
		ll = strlen(p);

		if (ll <= 4)
			continue;

		if (line[ll - 1] == '\n') {
			ll--;
			line[ll] = 0;
		}
		if (line[ll - 1] == '\r') {
			ll--;
			line[ll] = 0;
		}
#ifdef HAVE_STRSEP
		sp = line;
		p = strsep(&sp, ":");

		if (p != NULL && strcmp(pctx->username, p) == 0) {
			p = strsep(&sp, ":");
			if (p != NULL) {
				break_group_list(pctx, p, pctx->groupnames,
						 &pctx->groupnames_size);

				p = strsep(&sp, ":");
				if (p != NULL) {
					strlcpy(pctx->cpass, p,
						sizeof(pctx->cpass));
					pctx->failed = 0;
					ret = 0;
					goto exit;
				}
			}
		}

#else
		p = strtok_r(line, ":", &sp);

		if (p != NULL && strcmp(pctx->username, p) == 0) {
			p = strtok_r(NULL, ":", &sp);
			if (p != NULL) {
				break_group_list(pctx, p, pctx->groupnames,
						 &pctx->groupnames_size);

				p = strtok_r(NULL, ":", &sp);
				if (p != NULL) {
					strlcpy(pctx->cpass, p,
						sizeof(pctx->cpass));
					pctx->failed = 0;
					ret = 0;
					goto exit;
				}
			}
		}
#endif
	}

	/* always succeed */
	ret = 0;
exit:
	safe_memset(line, 0, sizeof(line));
	fclose(fp);
	return ret;
}

static int plain_auth_init(void **ctx, void *pool, void *vctx,
			   const common_auth_init_st *info)
{
	struct plain_ctx_st *pctx;
	int ret;

	if (info->username == NULL || info->username[0] == 0) {
		oc_syslog(LOG_ERR, "plain-auth: no username present");
		return ERR_AUTH_FAIL;
	}

	pctx = talloc_zero(pool, struct plain_ctx_st);
	if (pctx == NULL)
		return ERR_AUTH_FAIL;

	strlcpy(pctx->username, info->username, sizeof(pctx->username));
	pctx->pass_msg = NULL; /* use default */
	pctx->config = vctx;

	/* this doesn't fail on password mismatch but sets p->failed */
	ret = read_auth_pass(pctx);
	if (ret < 0) {
		talloc_free(pctx);
		return ERR_AUTH_FAIL;
	}

	*ctx = pctx;

	if (pctx->cpass[0] == 0 && pctx->failed == 0) {
		/* if there is no password set, nor an OTP file; don't ask for password */
		if (pctx->config->otp_file == NULL)
			return 0;

		/* only OTP is present */
		pctx->pass_msg = pass_msg_otp;
	}

	return ERR_AUTH_CONTINUE;
}

static int plain_auth_group(void *ctx, const char *suggested, char *groupname,
			    int groupname_size)
{
	struct plain_ctx_st *pctx = ctx;
	unsigned int i, found = 0;

	groupname[0] = 0;

	if (suggested != NULL) {
		for (i = 0; i < pctx->groupnames_size; i++) {
			if (strcmp(suggested, pctx->groupnames[i]) == 0) {
				strlcpy(groupname, pctx->groupnames[i],
					groupname_size);
				found = 1;
				break;
			}
		}

		if (found == 0) {
			oc_syslog(
				LOG_NOTICE,
				"user '%s' requested group '%s' but is not a member",
				pctx->username, suggested);
			return -1;
		}
	}

	if (pctx->groupnames_size > 0 && groupname[0] == 0) {
		strlcpy(groupname, pctx->groupnames[0], groupname_size);
	}
	return 0;
}

static int plain_auth_user(void *ctx, char *username, int username_size)
{
	/* do not update username */
	return -1;
}

/* Returns 0 if the user is successfully authenticated, and sets the appropriate group name.
 */
static int plain_auth_pass(void *ctx, const char *pass, unsigned int pass_len)
{
	struct plain_ctx_st *pctx = ctx;
	const char *p;

	if (pctx->cpass[0] != 0) {
		p = crypt(pass, pctx->cpass);
		if (p == NULL) {
			pctx->failed = 1;
		} else if (strcmp(p, pctx->cpass) != 0)
			pctx->failed = 1;
	}

	if (pctx->failed) {
		if (pctx->retries++ < MAX_PASSWORD_TRIES - 1) {
			pctx->pass_msg = pass_msg_failed;
			return ERR_AUTH_CONTINUE;
		} else {
			oc_syslog(LOG_NOTICE,
				  "plain-auth: error authenticating user '%s'",
				  pctx->username);
			return ERR_AUTH_FAIL;
		}
	}

	if (pctx->cpass[0] == 0 && pctx->config->otp_file == NULL) {
		oc_syslog(
			LOG_NOTICE,
			"plain-auth: user '%s' has empty password and no OTP file configured",
			pctx->username);
		return ERR_AUTH_FAIL;
	}

#ifdef HAVE_LIBOATH
	if (pctx->config->otp_file != NULL) {
		int ret;
		time_t last;

		if (pctx->cpass[0] != 0) { /* we just checked the password */
			pctx->cpass[0] = 0;
			pctx->pass_msg = pass_msg_otp;
			return ERR_AUTH_CONTINUE;
		}

		/* no primary password -> check OTP */
		ret = oath_authenticate_usersfile(pctx->config->otp_file,
						  pctx->username, pass,
						  HOTP_WINDOW, NULL, &last);
		if (ret != OATH_OK) {
			oc_syslog(LOG_NOTICE,
				  "plain-auth: OTP auth failed for '%s': %s",
				  pctx->username, oath_strerror(ret));
			return ERR_AUTH_FAIL;
		}
	}
#endif

	if (pctx->failed)
		return ERR_AUTH_FAIL;

	return 0;
}

static int plain_auth_msg(void *ctx, void *pool, passwd_msg_st *pst)
{
	struct plain_ctx_st *pctx = ctx;

	if (pctx->pass_msg)
		pst->msg_str = talloc_strdup(pool, pctx->pass_msg);
	pst->counter = 0; /* we support a single password */

	/* use the default prompt */
	return 0;
}

static void plain_auth_deinit(void *ctx)
{
	talloc_free(ctx);
}

static size_t rehash(const void *_e, void *unused)
{
	const char *e = _e;

	return hash_any(e, strlen(e), 0);
}

static bool str_cmp(const void *_c1, void *_c2)
{
	const char *c1 = _c1, *c2 = _c2;

	if (strcmp(c1, c2) == 0)
		return 1;
	return 0;
}

static void plain_group_list(void *pool, void *additional, char ***groupname,
			     unsigned int *groupname_size)
{
	FILE *fp;
	char line[512];
	ssize_t ll;
	char *p, *sp;
	unsigned int i;
	size_t hval;
	struct htable_iter iter;
	char *tgroup[MAX_GROUPS];
	unsigned int tgroup_size;
	struct htable hash;
	struct plain_cfg_st *config = additional;

	htable_init(&hash, rehash, NULL);

	pool = talloc_init("plain");
	fp = fopen(config->passwd, "r");
	if (fp == NULL) {
		oc_syslog(LOG_NOTICE,
			  "error in plain authentication; cannot open: %s",
			  (char *)config->passwd);
		return;
	}

	line[sizeof(line) - 1] = 0;
	while ((p = fgets(line, sizeof(line) - 1, fp)) != NULL) {
		ll = strlen(p);

		if (ll <= 4)
			continue;

		if (line[ll - 1] == '\n') {
			ll--;
			line[ll] = 0;
		}
		if (line[ll - 1] == '\r') {
			ll--;
			line[ll] = 0;
		}

#ifdef HAVE_STRSEP
		sp = line;
		p = strsep(&sp, ":");

		if (p != NULL) {
			p = strsep(&sp, ":");
#else
		p = strtok_r(line, ":", &sp);

		if (p != NULL) {
			p = strtok_r(NULL, ":", &sp);
#endif
			if (p != NULL) {
				break_group_list(pool, p, tgroup, &tgroup_size);

				for (i = 0; i < tgroup_size; i++) {
					hval = rehash(tgroup[i], NULL);

					if (htable_get(&hash, hval, str_cmp,
						       tgroup[i]) == NULL) {
						if (strlen(tgroup[i]) > 1)
							(void)htable_add(
								&hash, hval,
								tgroup[i]);
					}
				}
			}
		}
	}

	*groupname_size = 0;
	*groupname = talloc_size(pool, sizeof(char *) * MAX_GROUPS);
	if (*groupname == NULL) {
		goto exit;
	}

	p = htable_first(&hash, &iter);
	while (p != NULL && (*groupname_size) < MAX_GROUPS) {
		(*groupname)[(*groupname_size)] = talloc_strdup(*groupname, p);
		p = htable_next(&hash, &iter);
		(*groupname_size)++;
	}

	/* always succeed */
exit:
	htable_clear(&hash);
	safe_memset(line, 0, sizeof(line));
	fclose(fp);
}

const struct auth_mod_st plain_auth_funcs = { .type = AUTH_TYPE_PLAIN |
						      AUTH_TYPE_USERNAME_PASS,
					      .allows_retries = 1,
					      .vhost_init = plain_vhost_init,
					      .auth_init = plain_auth_init,
					      .auth_deinit = plain_auth_deinit,
					      .auth_msg = plain_auth_msg,
					      .auth_pass = plain_auth_pass,
					      .auth_user = plain_auth_user,
					      .auth_group = plain_auth_group,
					      .group_list = plain_group_list };
