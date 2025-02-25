/*
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#include <sec-mod-sup-config.h>
#include <common.h>
#include <vpn.h>
#include "common-config.h"

static void free_expanded_brackets_string(subcfg_val_st out[MAX_SUBOPTIONS],
					  unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; i++) {
		talloc_free(out[i].name);
		talloc_free(out[i].value);
	}
}

/* Returns the number of suboptions processed.
 */
static unsigned int expand_brackets_string(void *pool, const char *str,
					   subcfg_val_st out[MAX_SUBOPTIONS])
{
	char *p, *p2, *p3;
	unsigned int len, len2;
	unsigned int pos = 0, finish = 0;

	if (str == NULL)
		return 0;

	p = strchr(str, '[');
	if (p == NULL) {
		return 0;
	}
	p++;
	while (isspace(*p))
		p++;

	do {
		p2 = strchr(p, '=');
		if (p2 == NULL) {
			fprintf(stderr, "error parsing %s\n", str);
			exit(EXIT_FAILURE);
		}
		len = p2 - p;

		p2++;
		while (isspace(*p2))
			p2++;

		p3 = strchr(p2, ',');
		if (p3 == NULL) {
			p3 = strchr(p2, ']');
			if (p3 == NULL) {
				fprintf(stderr, "error parsing %s\n", str);
				exit(EXIT_FAILURE);
			}
			finish = 1;
		}
		len2 = p3 - p2;

		if (len > 0) {
			while (isspace(p[len - 1]))
				len--;
		}
		if (len2 > 0) {
			while (isspace(p2[len2 - 1]))
				len2--;
		}

		out[pos].name = talloc_strndup(pool, p, len);
		out[pos].value = talloc_strndup(pool, p2, len2);
		pos++;
		p = p2 + len2;
		while (isspace(*p) || *p == ',')
			p++;
	} while (finish == 0 && pos < MAX_SUBOPTIONS);

	return pos;
}

#ifdef HAVE_GSSAPI
void *gssapi_get_brackets_string(void *pool, struct perm_cfg_st *config,
				 const char *str)
{
	subcfg_val_st vals[MAX_SUBOPTIONS];
	unsigned int vals_size, i;
	gssapi_cfg_st *additional;

	additional = talloc_zero(pool, gssapi_cfg_st);
	if (additional == NULL) {
		return NULL;
	}

	vals_size = expand_brackets_string(pool, str, vals);
	for (i = 0; i < vals_size; i++) {
		if (strcasecmp(vals[i].name, "keytab") == 0) {
			additional->keytab = vals[i].value;
			vals[i].value = NULL;
		} else if (strcasecmp(vals[i].name, "require-local-user-map") ==
			   0) {
			additional->no_local_map =
				1 - CHECK_TRUE(vals[i].value);
		} else if (strcasecmp(vals[i].name, "tgt-freshness-time") ==
			   0) {
			additional->ticket_freshness_secs = atoi(vals[i].value);
			if (additional->ticket_freshness_secs == 0) {
				fprintf(stderr, "Invalid value for '%s': %s\n",
					vals[i].name, vals[i].value);
				exit(EXIT_FAILURE);
			}
		} else if (strcasecmp(vals[i].name, "gid-min") == 0) {
			additional->gid_min = atoi(vals[i].value);
			if (additional->gid_min < 0) {
				fprintf(stderr, "error in gid-min value: %d\n",
					additional->gid_min);
				exit(EXIT_FAILURE);
			}
		} else {
			fprintf(stderr, "unknown option '%s'\n", vals[i].name);
			exit(EXIT_FAILURE);
		}
	}
	free_expanded_brackets_string(vals, vals_size);
	return additional;
}
#endif

void *get_brackets_string1(void *pool, const char *str)
{
	char *p, *p2;
	unsigned int len;

	p = strchr(str, '[');
	if (p == NULL) {
		return NULL;
	}
	p++;
	while (isspace(*p))
		p++;

	p2 = strchr(p, ',');
	if (p2 == NULL) {
		p2 = strchr(p, ']');
		if (p2 == NULL) {
			fprintf(stderr, "error parsing %s\n", str);
			exit(EXIT_FAILURE);
		}
	}

	len = p2 - p;

	return talloc_strndup(pool, p, len);
}

#ifdef HAVE_RADIUS
static void *get_brackets_string2(void *pool, const char *str)
{
	char *p, *p2;
	unsigned int len;

	p = strchr(str, '[');
	if (p == NULL) {
		return NULL;
	}
	p++;

	p = strchr(p, ',');
	if (p == NULL) {
		return NULL;
	}
	p++;

	while (isspace(*p))
		p++;

	p2 = strchr(p, ',');
	if (p2 == NULL) {
		p2 = strchr(p, ']');
		if (p2 == NULL) {
			fprintf(stderr, "error parsing %s\n", str);
			exit(EXIT_FAILURE);
		}
	}

	len = p2 - p;

	return talloc_strndup(pool, p, len);
}

void *radius_get_brackets_string(void *pool, struct perm_cfg_st *config,
				 const char *str)
{
	char *p;
	subcfg_val_st vals[MAX_SUBOPTIONS];
	unsigned int vals_size, i;
	radius_cfg_st *additional;

	additional = talloc_zero(pool, radius_cfg_st);
	if (additional == NULL) {
		return NULL;
	}

	if (str && str[0] == '[' &&
	    (str[1] == '/' || str[1] == '.')) { /* legacy format */
		additional->config = get_brackets_string1(pool, str);

		p = get_brackets_string2(config, str);
		if (p != NULL) {
			if (strcasecmp(p, "groupconfig") != 0) {
				fprintf(stderr,
					"No known configuration option: %s\n",
					p);
				exit(EXIT_FAILURE);
			}
			config->sup_config_type = SUP_CONFIG_RADIUS;
		}
	} else {
		/* new format */
		vals_size = expand_brackets_string(pool, str, vals);
		for (i = 0; i < vals_size; i++) {
			if (strcasecmp(vals[i].name, "config") == 0) {
				additional->config = vals[i].value;
				vals[i].value = NULL;
			} else if (strcasecmp(vals[i].name, "nas-identifier") ==
				   0) {
				additional->nas_identifier = vals[i].value;
				vals[i].value = NULL;
			} else if (strcasecmp(vals[i].name, "groupconfig") ==
				   0) {
				if (CHECK_TRUE(vals[i].value))
					config->sup_config_type =
						SUP_CONFIG_RADIUS;
			} else {
				fprintf(stderr, "unknown option '%s'\n",
					vals[i].name);
				exit(EXIT_FAILURE);
			}
		}
		free_expanded_brackets_string(vals, vals_size);
	}

	if (additional->config == NULL) {
		fprintf(stderr, "No radius configuration specified: %s\n", str);
		exit(EXIT_FAILURE);
	}

	return additional;
}
#endif

#ifdef HAVE_PAM
void *pam_get_brackets_string(void *pool, struct perm_cfg_st *config,
			      const char *str)
{
	subcfg_val_st vals[MAX_SUBOPTIONS];
	unsigned int vals_size, i;
	pam_cfg_st *additional;

	additional = talloc_zero(pool, pam_cfg_st);
	if (additional == NULL) {
		return NULL;
	}

	/* new format */
	vals_size = expand_brackets_string(pool, str, vals);
	for (i = 0; i < vals_size; i++) {
		if (strcasecmp(vals[i].name, "gid-min") == 0) {
			additional->gid_min = atoi(vals[i].value);
			if (additional->gid_min < 0) {
				fprintf(stderr, "error in gid-min value: %d\n",
					additional->gid_min);
				exit(EXIT_FAILURE);
			}
		} else {
			fprintf(stderr, "unknown option '%s'\n", vals[i].name);
			exit(EXIT_FAILURE);
		}
	}

	free_expanded_brackets_string(vals, vals_size);
	return additional;
}
#endif

void *plain_get_brackets_string(void *pool, struct perm_cfg_st *config,
				const char *str)
{
	subcfg_val_st vals[MAX_SUBOPTIONS];
	unsigned int vals_size, i;
	plain_cfg_st *additional;

	additional = talloc_zero(pool, plain_cfg_st);
	if (additional == NULL) {
		return NULL;
	}

	if (str && str[0] == '[' &&
	    (str[1] == '/' || str[1] == '.')) { /* legacy format */
		additional->passwd = get_brackets_string1(pool, str);
	} else {
		vals_size = expand_brackets_string(pool, str, vals);
		for (i = 0; i < vals_size; i++) {
			if (strcasecmp(vals[i].name, "passwd") == 0) {
				additional->passwd = vals[i].value;
				vals[i].value = NULL;
#ifdef HAVE_LIBOATH
			} else if (strcasecmp(vals[i].name, "otp") == 0) {
				additional->otp_file = vals[i].value;
				vals[i].value = NULL;
#endif
			} else {
				fprintf(stderr, "unknown option '%s'\n",
					vals[i].name);
				exit(EXIT_FAILURE);
			}
		}
		free_expanded_brackets_string(vals, vals_size);
	}

	if (additional->passwd == NULL && additional->otp_file == NULL) {
		fprintf(stderr, "plain: no password or OTP file specified\n");
		exit(EXIT_FAILURE);
	}

	return additional;
}

void *oidc_get_brackets_string(void *pool, struct perm_cfg_st *config,
			       const char *str)
{
	subcfg_val_st vals[MAX_SUBOPTIONS];
	char *additional = NULL;

	unsigned int vals_size, i;

	vals_size = expand_brackets_string(pool, str, vals);

	for (i = 0; i < vals_size; i++) {
		if (strcasecmp(vals[i].name, "config") == 0) {
			additional = talloc_strdup(pool, vals[i].value);
		}
	}

	return additional;
}
