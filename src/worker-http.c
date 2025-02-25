/*
 * Copyright (C) 2015-2018 Nikos Mavrogiannopoulos
 * Copyright (C) 2015 Red Hat
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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_COMPRESSION
#ifdef HAVE_LZ4
#include <lz4.h>
#endif
#include "lzs.h"
#endif

#include <nettle/base64.h>
#include <base64-helper.h>
#include <ctype.h>

#include <vpn.h>
#include <worker.h>

#define CS_AES128_GCM "OC-DTLS1_2-AES128-GCM"
#define CS_AES256_GCM "OC-DTLS1_2-AES256-GCM"

struct known_urls_st {
	const char *url;
	unsigned int url_size;
	unsigned int partial_match;
	url_handler_fn get_handler;
	url_handler_fn post_handler;
};

#define LL(x, y, z) { x, sizeof(x) - 1, 0, y, z }
#define LL_DIR(x, y, z) { x, sizeof(x) - 1, 1, y, z }
static const struct known_urls_st known_urls[] = {
	LL("/", get_auth_handler, post_auth_handler),
	LL("/auth", get_auth_handler, post_auth_handler),
	LL("/VPN", get_auth_handler, post_auth_handler),
	LL("/cert.pem", get_cert_handler, NULL),
	LL("/cert.cer", get_cert_der_handler, NULL),
	LL("/ca.pem", get_ca_handler, NULL),
	LL("/ca.cer", get_ca_der_handler, NULL),
#ifdef ANYCONNECT_CLIENT_COMPAT
	LL_DIR("/profiles", get_config_handler, NULL),
	LL("/VPNManifest.xml", get_string_handler, NULL),
	LL("/1/index.html", get_empty_handler, NULL),
	LL("/1/Linux", get_empty_handler, NULL),
	LL("/1/Linux_64", get_empty_handler, NULL),
	LL("/1/Windows", get_empty_handler, NULL),
	LL("/1/Windows_ARM64", get_empty_handler, NULL),
	LL("/1/Darwin_i386", get_empty_handler, NULL),
	LL("/1/binaries/vpndownloader.sh", get_dl_handler, NULL),
	LL("/1/VPNManifest.xml", get_string_handler, NULL),
	LL("/1/binaries/update.txt", get_string_handler, NULL),

	LL("/+CSCOT+/", get_string_handler, NULL),
	LL("/logout", get_empty_handler, NULL),
#endif
	LL("/svc", get_svc_handler, post_svc_handler),
	{ NULL, 0, 0, NULL, NULL }
};

/* In the following we use %NO_SESSION_HASH:%DISABLE_SAFE_RENEGOTIATION because certain
 * versions of openssl send the extended master secret extension in this
 * resumed session. Since the state of this extension is undefined
 * (it's not a real session we are resuming), we explicitly disable this
 * extension to avoid interop issues. Furthermore gnutls does seem to
 * be sending the renegotiation extension which openssl doesn't like (see #193) */

#if GNUTLS_VERSION_NUMBER >= 0x030400
#define WORKAROUND_STR "%NO_SESSION_HASH:%DISABLE_SAFE_RENEGOTIATION"
#else
#define WORKAROUND_STR "%DISABLE_SAFE_RENEGOTIATION"
#endif

/* Consider switching to gperf when this table grows significantly.
 * These tables are used for the custom DTLS cipher negotiation via
 * HTTP headers (WTF), and the compression negotiation.
 */
static const dtls_ciphersuite_st ciphersuites[] = {
	{
		.oc_name = CS_AES128_GCM,
		.gnutls_name =
			"NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-128-GCM:+AEAD:+RSA:+SIGN-ALL:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS1_2,
		.gnutls_mac = GNUTLS_MAC_AEAD,
		.gnutls_kx = GNUTLS_KX_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_128_GCM,
		.server_prio = 80,
	},
	{
		.oc_name = CS_AES256_GCM,
		.gnutls_name =
			"NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-256-GCM:+AEAD:+RSA:+SIGN-ALL:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS1_2,
		.gnutls_mac = GNUTLS_MAC_AEAD,
		.gnutls_kx = GNUTLS_KX_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_256_GCM,
		.server_prio = 90,
	},
	{
		.oc_name = "AES256-SHA",
		.gnutls_name =
			"NONE:+VERS-DTLS0.9:+COMP-NULL:+AES-256-CBC:+SHA1:+RSA:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS0_9,
		.gnutls_mac = GNUTLS_MAC_SHA1,
		.gnutls_kx = GNUTLS_KX_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_256_CBC,
		.server_prio = 60,
	},
	{
		.oc_name = "AES128-SHA",
		.gnutls_name =
			"NONE:+VERS-DTLS0.9:+COMP-NULL:+AES-128-CBC:+SHA1:+RSA:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS0_9,
		.gnutls_mac = GNUTLS_MAC_SHA1,
		.gnutls_kx = GNUTLS_KX_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_128_CBC,
		.server_prio = 50,
	},
	{
		.oc_name = "DES-CBC3-SHA",
		.gnutls_name =
			"NONE:+VERS-DTLS0.9:+COMP-NULL:+3DES-CBC:+SHA1:+RSA:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS0_9,
		.gnutls_mac = GNUTLS_MAC_SHA1,
		.gnutls_kx = GNUTLS_KX_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_3DES_CBC,
		.server_prio = 1,
	},
};

static const dtls_ciphersuite_st ciphersuites12[] = {
	{ .oc_name = "AES128-GCM-SHA256",
	  .gnutls_name =
		  "NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-128-GCM:+AEAD:+RSA:+SIGN-ALL:" WORKAROUND_STR,
	  .gnutls_version = GNUTLS_DTLS1_2,
	  .gnutls_mac = GNUTLS_MAC_AEAD,
	  .gnutls_kx = GNUTLS_KX_RSA,
	  .gnutls_cipher = GNUTLS_CIPHER_AES_128_GCM,
	  .dtls12_mode = 1,
	  .server_prio = 50 },
	{ .oc_name = "AES256-GCM-SHA384",
	  .gnutls_name =
		  "NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-256-GCM:+AEAD:+RSA:+SIGN-ALL:" WORKAROUND_STR,
	  .gnutls_version = GNUTLS_DTLS1_2,
	  .gnutls_mac = GNUTLS_MAC_AEAD,
	  .gnutls_kx = GNUTLS_KX_RSA,
	  .gnutls_cipher = GNUTLS_CIPHER_AES_256_GCM,
	  .dtls12_mode = 1,
	  .server_prio = 90 },
	/* these next two are currently only used by cisco-svc-client-compat devices */
	{
		.oc_name = "ECDHE-RSA-AES128-GCM-SHA256",
		.gnutls_name =
			"NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-128-GCM:+AEAD:+SHA256:+ECDHE-RSA:+SIGN-ALL:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS1_2,
		.gnutls_mac = GNUTLS_MAC_AEAD,
		.gnutls_kx = GNUTLS_KX_ECDHE_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_128_GCM,
		.dtls12_mode = 1,
		.server_prio = 70,
	},
	{
		.oc_name = "ECDHE-RSA-AES256-GCM-SHA384",
		.gnutls_name =
			"NONE:+VERS-DTLS1.2:+COMP-NULL:+AES-256-GCM:+AEAD:+SHA384:+ECDHE-RSA:+SIGN-ALL:" WORKAROUND_STR,
		.gnutls_version = GNUTLS_DTLS1_2,
		.gnutls_mac = GNUTLS_MAC_AEAD,
		.gnutls_kx = GNUTLS_KX_ECDHE_RSA,
		.gnutls_cipher = GNUTLS_CIPHER_AES_256_GCM,
		.dtls12_mode = 1,
		.server_prio = 80,
	}
};

#define STR_ST(x) { .data = (uint8_t *)x, .length = sizeof(x) - 1 }
static const str_st sensitve_http_headers[] = { STR_ST("Cookie"),
						STR_ST("X-DTLS-Master-Secret"),
						STR_ST("Authorization"),
						{ NULL, 0 } };

#ifdef HAVE_LZ4
/* Wrappers over LZ4 functions */
static int lz4_decompress(void *dst, int dstlen, const void *src, int srclen)
{
	return LZ4_decompress_safe(src, dst, srclen, dstlen);
}

static int lz4_compress(void *dst, int dstlen, const void *src, int srclen)
{
	/* we intentionally restrict output to srclen so that
	 * compression fails early for packets that expand. */
	return LZ4_compress_default(src, dst, srclen, srclen);
}
#endif

#ifdef ENABLE_COMPRESSION
struct compression_method_st comp_methods[] = {
#ifdef HAVE_LZ4
	{
		.id = OC_COMP_LZ4,
		.name = "oc-lz4",
		.decompress = lz4_decompress,
		.compress = lz4_compress,
		.server_prio = 90,
	},
#endif
	{
		.id = OC_COMP_LZS,
		.name = "lzs",
		.decompress = (decompress_fn)lzs_decompress,
		.compress = (compress_fn)lzs_compress,
		.server_prio = 80,
	}
};

unsigned int switch_comp_priority(void *pool, const char *modstring)
{
	unsigned int i, ret;
	char *token, *str;
	const char *algo = NULL;
	long priority = -1;

	str = talloc_strdup(pool, modstring);
	if (!str)
		return 0;

	token = str;
	token = strtok(token, ":");

	algo = token;

	token = strtok(NULL, ":");
	if (token)
		priority = strtol(token, NULL, 10);

	if (algo == NULL || priority <= 0) {
		ret = 0;
		goto finish;
	}
	for (i = 0; i < ARRAY_SIZE(comp_methods); i++) {
		if (strcasecmp(algo, comp_methods[i].name) == 0) {
			comp_methods[i].server_prio = priority;
			ret = 1;
			goto finish;
		}
	}

	ret = 0;

finish:
	talloc_free(str);
	return ret;
}
#endif

static bool header_is_sensitive(str_st *header)
{
	size_t i;

	for (i = 0; sensitve_http_headers[i].length != 0; i++) {
		if ((header->length == sensitve_http_headers[i].length) &&
		    (strncasecmp((char *)header->data,
				 (char *)sensitve_http_headers[i].data,
				 header->length) == 0))
			return true;
	}
	return false;
}

static void header_value_check(struct worker_st *ws, struct http_req_st *req)
{
	unsigned int tmplen, i;
	int ret;
	size_t nlen, value_length;
	char *token, *value;
	char *str, *p;
	const dtls_ciphersuite_st *cand = NULL;
	const dtls_ciphersuite_st *saved_ciphersuite;
	const compression_method_st *comp_cand = NULL;
	const compression_method_st **selected_comp;
	int want_cipher;
	int want_mac;

	if (req->value.length <= 0)
		return;

	if (WSPCONFIG(ws)->log_level < OCLOG_SENSITIVE &&
	    header_is_sensitive(&req->header))
		oclog(ws, LOG_HTTP_DEBUG, "HTTP processing: %.*s: (censored)",
		      (int)req->header.length, req->header.data);
	else
		oclog(ws, LOG_HTTP_DEBUG, "HTTP processing: %.*s: %.*s",
		      (int)req->header.length, req->header.data,
		      (int)req->value.length, req->value.data);

	value = talloc_size(ws, req->value.length + 1);
	if (value == NULL)
		return;

	/* make sure the value is null terminated */
	value_length = req->value.length;
	memcpy(value, req->value.data, value_length);
	value[value_length] = 0;

	switch (req->next_header) {
	case HEADER_MASTER_SECRET:
		if (req->use_psk || !WSCONFIG(ws)->dtls_legacy) /* ignored */
			break;

		if (value_length < TLS_MASTER_SIZE * 2) {
			req->master_secret_set = 0;
			goto cleanup;
		}

		tmplen = TLS_MASTER_SIZE * 2;

		nlen = sizeof(req->master_secret);
		gnutls_hex2bin((void *)value, tmplen, req->master_secret,
			       &nlen);

		req->master_secret_set = 1;
		break;
	case HEADER_HOSTNAME:
		if (value_length + 1 > MAX_HOSTNAME_SIZE) {
			req->hostname[0] = 0;
			goto cleanup;
		}
		memcpy(req->hostname, value, value_length);
		req->hostname[value_length] = 0;

		/* check validity */
		if (!valid_hostname(req->hostname)) {
			oclog(ws, LOG_HTTP_DEBUG,
			      "Skipping invalid hostname '%s'", req->hostname);
			req->hostname[0] = 0;
		}

		break;
	case HEADER_DEVICE_TYPE:
		if (value_length + 1 > sizeof(req->devtype)) {
			req->devtype[0] = 0;
			goto cleanup;
		}
		memcpy(req->devtype, value, value_length);
		req->devtype[value_length] = 0;

		oclog(ws, LOG_DEBUG, "Device-type: '%s'", value);
		break;
	case HEADER_PLATFORM:
		if (value_length + 1 > sizeof(req->devplatform)) {
			req->devplatform[0] = 0;
			goto cleanup;
		}
		memcpy(req->devplatform, value, value_length);
		req->devplatform[value_length] = 0;

		if (strncasecmp(value, "apple-ios", 9) == 0 ||
		    strncasecmp(value, "android", 7) == 0) {
			if (strncasecmp(value, "apple-ios", 9) == 0)
				req->is_ios = 1;

			oclog(ws, LOG_DEBUG, "Platform: '%s' (mobile)", value);
			req->is_mobile = 1;
		} else {
			oclog(ws, LOG_DEBUG, "Platform: '%s'", value);
		}
		break;
	case HEADER_SUPPORT_SPNEGO:
		/* Switch to GSSAPI if the client supports it, but only
		 * if we haven't already authenticated with a certificate */
		if (!((ws->selected_auth->type & AUTH_TYPE_CERTIFICATE) &&
		      ws->cert_auth_ok != 0)) {
			ws_switch_auth_to(ws, AUTH_TYPE_GSSAPI);
			req->spnego_set = 1;
		}
		break;
	case HEADER_AUTHORIZATION:
		if (req->authorization != NULL)
			talloc_free(req->authorization);
		req->authorization = value;
		req->authorization_size = value_length;
		value = NULL;
		break;
	case HEADER_USER_AGENT:
		if (value_length + 1 > MAX_AGENT_NAME) {
			memcpy(req->user_agent, value, MAX_AGENT_NAME - 1);
			req->user_agent[MAX_AGENT_NAME - 1] = 0;
		} else {
			memcpy(req->user_agent, value, value_length);
			req->user_agent[value_length] = 0;
		}

		oclog(ws, LOG_DEBUG, "User-agent: '%s'", req->user_agent);

		if (strncasecmp(req->user_agent, "Open AnyConnect VPN Agent v",
				27) == 0) {
			unsigned int version = atoi(&req->user_agent[27]);

			if (version <= 3) {
				oclog(ws, LOG_DEBUG,
				      "Detected OpenConnect v3 or older");
				req->user_agent_type = AGENT_OPENCONNECT_V3;
			} else {
				oclog(ws, LOG_DEBUG,
				      "Detected OpenConnect v4 or newer");
				req->user_agent_type = AGENT_OPENCONNECT;
			}
		} else if (strncasecmp(req->user_agent,
				       "Cisco AnyConnect VPN Agent for Apple",
				       36) == 0) {
			oclog(ws, LOG_DEBUG,
			      "Detected Cisco AnyConnect on iOS");
			req->user_agent_type = AGENT_ANYCONNECT;
			req->is_ios = 1;
		} else if (strncasecmp(req->user_agent, "OpenConnect VPN Agent",
				       21) == 0) {
			oclog(ws, LOG_DEBUG,
			      "Detected OpenConnect v4 or newer");
			req->user_agent_type = AGENT_OPENCONNECT;
		} else if (strncasecmp(req->user_agent, "Cisco AnyConnect",
				       16) == 0) {
			oclog(ws, LOG_DEBUG, "Detected Cisco AnyConnect");
			req->user_agent_type = AGENT_ANYCONNECT;
		} else if (strncasecmp(req->user_agent,
				       "AnyConnect-compatible OpenConnect",
				       33) == 0) {
			oclog(ws, LOG_DEBUG,
			      "Detected OpenConnect v9 or newer");
			req->user_agent_type = AGENT_OPENCONNECT;
		} else if (strncasecmp(req->user_agent, "AnyConnect", 10) ==
			   0) {
			oclog(ws, LOG_DEBUG, "Detected Cisco AnyConnect");
			req->user_agent_type = AGENT_ANYCONNECT;
		} else if (strncasecmp(req->user_agent,
				       "Clavister OneConnect VPN", 24) == 0) {
			oclog(ws, LOG_DEBUG, "Detected Clavister OneConnect");
			req->user_agent_type = AGENT_OPENCONNECT_CLAVISTER;
		} else if (strncasecmp(req->user_agent, "AnyLink Secure Client",
				       21) == 0) {
			oclog(ws, LOG_DEBUG, "Detected AnyLink");
			req->user_agent_type = AGENT_ANYLINK;
		} else if (strncasecmp(req->user_agent,
				       "Cisco SVC IPPhone Client", 24) == 0) {
			oclog(ws, LOG_DEBUG,
			      "Detected Cisco SVC IPPhone Client");
			req->user_agent_type = AGENT_SVC_IPPHONE;
		} else {
			oclog(ws, LOG_DEBUG, "Unknown client (%s)",
			      req->user_agent);
		}
		break;

	case HEADER_DTLS_CIPHERSUITE:
		str = (char *)value;

		p = strstr(str, DTLS_PROTO_INDICATOR);
		if (p != NULL && (p[sizeof(DTLS_PROTO_INDICATOR) - 1] == 0 ||
				  p[sizeof(DTLS_PROTO_INDICATOR) - 1] == ':')) {
			/* OpenConnect DTLS setup was detected. */
			if (WSCONFIG(ws)->dtls_psk) {
				req->use_psk = 1;
				req->master_secret_set =
					1; /* we don't need it */
				req->selected_ciphersuite = NULL;
				break;
			}
		}

		if (req->use_psk || !WSCONFIG(ws)->dtls_legacy)
			break;

		if (req->selected_ciphersuite) /* if set via HEADER_DTLS12_CIPHERSUITE */
			break;

		if (ws->session != NULL) {
			want_mac = gnutls_mac_get(ws->session);
			want_cipher = gnutls_cipher_get(ws->session);
		} else {
			want_mac = -1;
			want_cipher = -1;
		}

		while ((token = strtok(str, ":")) != NULL) {
			for (i = 0; i < ARRAY_SIZE(ciphersuites); i++) {
				if (strcmp(token, ciphersuites[i].oc_name) ==
				    0) {
					if (cand == NULL ||
					    cand->server_prio <
						    ciphersuites[i].server_prio ||
					    (want_cipher != -1 &&
					     want_cipher ==
						     ciphersuites[i]
							     .gnutls_cipher &&
					     want_mac == ciphersuites[i]
								 .gnutls_mac)) {
						cand = &ciphersuites[i];

						/* if our candidate matches the TLS session
						 * ciphersuite, we are finished */
						if (want_cipher != -1) {
							if (want_cipher ==
								    cand->gnutls_cipher &&
							    want_mac ==
								    cand->gnutls_mac)
								goto ciphersuite_finish;
						}
					}
				}
			}
			str = NULL;
		}
ciphersuite_finish:
		req->selected_ciphersuite = cand;

		break;
	case HEADER_DTLS12_CIPHERSUITE:
		if (req->use_psk || !WSCONFIG(ws)->dtls_legacy) {
			break;
		}

		/* in gnutls 3.6.0+ there is a regression which makes
		 * anyconnect's openssl fail: https://gitlab.com/gnutls/gnutls/merge_requests/868
		 */
#ifdef gnutls_check_version_numeric
		if (req->user_agent_type == AGENT_ANYCONNECT &&
		    (!gnutls_check_version_numeric(3, 6, 6) &&
		     (!gnutls_check_version_numeric(3, 3, 0) ||
		      gnutls_check_version_numeric(3, 6, 0)))) {
			break;
		}
#endif

		str = (char *)value;

		p = strstr(str, DTLS_PROTO_INDICATOR);
		if (p != NULL && (p[sizeof(DTLS_PROTO_INDICATOR) - 1] == 0 ||
				  p[sizeof(DTLS_PROTO_INDICATOR) - 1] == ':')) {
			/* OpenConnect DTLS setup was detected. */
			if (WSCONFIG(ws)->dtls_psk) {
				req->use_psk = 1;
				req->master_secret_set =
					1; /* we don't need it */
				req->selected_ciphersuite = NULL;
				break;
			}
		}

		saved_ciphersuite = req->selected_ciphersuite;
		req->selected_ciphersuite = NULL;

		if (ws->session != NULL) {
			want_mac = gnutls_mac_get(ws->session);
			want_cipher = gnutls_cipher_get(ws->session);
		} else {
			want_mac = -1;
			want_cipher = -1;
		}

		while ((token = strtok(str, ":")) != NULL) {
			for (i = 0; i < ARRAY_SIZE(ciphersuites12); i++) {
				if (strcmp(token, ciphersuites12[i].oc_name) ==
				    0) {
					if (cand == NULL ||
					    cand->server_prio <
						    ciphersuites12[i]
							    .server_prio ||
					    (want_cipher != -1 &&
					     want_cipher ==
						     ciphersuites12[i]
							     .gnutls_cipher &&
					     want_mac == ciphersuites12[i]
								 .gnutls_mac)) {
						cand = &ciphersuites12[i];

						/* if our candidate matches the TLS session
						 * ciphersuite, we are finished */
						if (want_cipher != -1) {
							if (want_cipher ==
								    cand->gnutls_cipher &&
							    want_mac ==
								    cand->gnutls_mac)
								goto ciphersuite12_finish;
						}
					}
				}
			}
			str = NULL;
		}
ciphersuite12_finish:
		req->selected_ciphersuite = cand;

		if (req->selected_ciphersuite == NULL && saved_ciphersuite)
			req->selected_ciphersuite = saved_ciphersuite;

		break;
#ifdef ENABLE_COMPRESSION
	case HEADER_DTLS_ENCODING:
	case HEADER_CSTP_ENCODING:
		if (WSCONFIG(ws)->enable_compression == 0)
			break;

		if (req->next_header == HEADER_DTLS_ENCODING)
			selected_comp = &ws->dtls_selected_comp;
		else
			selected_comp = &ws->cstp_selected_comp;
		*selected_comp = NULL;

		str = (char *)value;
		while ((token = strtok(str, ",")) != NULL) {
			for (i = 0; i < ARRAY_SIZE(comp_methods); i++) {
				if (strcasecmp(token, comp_methods[i].name) ==
				    0) {
					if (comp_cand == NULL ||
					    comp_cand->server_prio <
						    comp_methods[i].server_prio) {
						comp_cand = &comp_methods[i];
					}
				}
			}
			str = NULL;
		}
		*selected_comp = comp_cand;
		break;
#endif

	case HEADER_CSTP_BASE_MTU:
		req->link_mtu = atoi((char *)value);
		break;
	case HEADER_CSTP_MTU:
		req->tunnel_mtu = atoi((char *)value);
		break;
	case HEADER_CSTP_ATYPE:
		if (memmem(value, value_length, "IPv4", 4) == NULL)
			req->no_ipv4 = 1;
		if (memmem(value, value_length, "IPv6", 4) == NULL)
			req->no_ipv6 = 1;
		break;
	case HEADER_FULL_IPV6:
		if (memmem(value, value_length, "true", 4) != NULL)
			ws->full_ipv6 = 1;
		break;
	case HEADER_COOKIE:
		/* don't bother parsing cookies if we are already authenticated */
		if (ws->auth_state > S_AUTH_COOKIE)
			break;

		str = (char *)value;
		while ((token = strtok(str, ";")) != NULL) {
			p = token;
			while (isspace(*p)) {
				p++;
			}
			tmplen = strlen(p);

			if (strncmp(p, "webvpn=", 7) == 0) {
				tmplen -= 7;
				p += 7;

				while (tmplen > 1 && isspace(p[tmplen - 1])) {
					tmplen--;
				}

				/* we allow for BASE64_DECODE_LENGTH reporting few bytes more
				 * than the expected */
				nlen = BASE64_DECODE_LENGTH(tmplen);
				if (nlen < sizeof(ws->cookie) ||
				    nlen > sizeof(ws->cookie) + 8)
					return;

				/* we assume that - should be build time optimized */
				if (sizeof(ws->buffer) < sizeof(ws->cookie) + 8)
					abort();

				ret = oc_base64_decode((uint8_t *)p, tmplen,
						       ws->buffer, &nlen);
				if (ret == 0 || nlen != sizeof(ws->cookie)) {
					oclog(ws, LOG_INFO,
					      "could not decode cookie: %.*s",
					      tmplen, p);
					ws->cookie_set = 0;
				} else {
					memcpy(ws->cookie, ws->buffer,
					       sizeof(ws->cookie));
					ws->auth_state = S_AUTH_COOKIE;
					ws->cookie_set = 1;
				}
			} else if (strncmp(p, "webvpncontext=", 14) == 0) {
				p += 14;
				tmplen -= 14;

				while (tmplen > 1 && isspace(p[tmplen - 1])) {
					tmplen--;
				}

				nlen = BASE64_DECODE_LENGTH(tmplen);
				ret = oc_base64_decode((uint8_t *)p, tmplen,
						       ws->sid, &nlen);
				if (ret == 0 || nlen != sizeof(ws->sid)) {
					oclog(ws, LOG_SENSITIVE,
					      "could not decode sid: %.*s",
					      tmplen, p);
					ws->sid_set = 0;
				} else {
					ws->sid_set = 1;
					oclog(ws, LOG_SENSITIVE,
					      "received sid: %.*s", tmplen, p);
				}
			}

			str = NULL;
		}
		break;
	}

cleanup:
	talloc_free(value);
}

url_handler_fn http_get_url_handler(const char *url)
{
	const struct known_urls_st *p;
	unsigned int len = strlen(url);

	p = known_urls;
	do {
		if (p->url != NULL) {
			if ((len == p->url_size && strcmp(p->url, url) == 0) ||
			    (len >= p->url_size &&
			     strncmp(p->url, url, p->url_size) == 0 &&
			     (p->partial_match != 0 ||
			      url[p->url_size] == '/' ||
			      url[p->url_size] == '?')))
				return p->get_handler;
		}
		p++;
	} while (p->url != NULL);

	return NULL;
}

url_handler_fn http_post_known_service_check(struct worker_st *ws,
					     const char *url)
{
	const struct known_urls_st *p;
	unsigned int len = strlen(url);
	unsigned int i;

	p = known_urls;
	do {
		if (p->url != NULL) {
			if ((len == p->url_size && strcmp(p->url, url) == 0) ||
			    (len > p->url_size &&
			     strncmp(p->url, url, p->url_size) == 0 &&
			     p->partial_match == 0 && url[p->url_size] == '?'))
				return p->post_handler;
		}
		p++;
	} while (p->url != NULL);

	for (i = 0; i < WSCONFIG(ws)->kkdcp_size; i++) {
		if (WSCONFIG(ws)->kkdcp[i].url &&
		    strcmp(WSCONFIG(ws)->kkdcp[i].url, url) == 0)
			return post_kkdcp_handler;
	}

	return NULL;
}

url_handler_fn http_post_url_handler(struct worker_st *ws, const char *url)
{
	url_handler_fn h;

	h = http_post_known_service_check(ws, url);
	if (h == NULL && ws->auth_state == S_AUTH_INACTIVE) {
		return post_auth_handler;
	}

	return h;
}

int http_url_cb(llhttp_t *parser, const char *at, size_t length)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;

	if (length >= sizeof(req->url)) {
		req->url[0] = 0;
		return 1;
	}

	memcpy(req->url, at, length);
	req->url[length] = 0;

	return 0;
}

int http_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;
	int ret;

	if (req->header_state != HTTP_HEADER_RECV) {
		/* handle value */
		if (req->header_state == HTTP_HEADER_VALUE_RECV)
			header_value_check(ws, req);
		req->header_state = HTTP_HEADER_RECV;
		str_reset(&req->header);
	}

	ret = str_append_data(&req->header, at, length);
	if (ret < 0)
		return ret;

	return 0;
}

/* include hash table of headers */
#include "http-heads.h"

static void header_check(struct http_req_st *req)
{
	const struct http_headers_st *p;

	p = in_word_set((char *)req->header.data, req->header.length);
	if (p != NULL) {
		req->next_header = p->id;
		return;
	}
	req->next_header = 0;
}

int http_header_value_cb(llhttp_t *parser, const char *at, size_t length)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;
	int ret;

	if (req->header_state != HTTP_HEADER_VALUE_RECV) {
		/* handle header */
		header_check(req);
		req->header_state = HTTP_HEADER_VALUE_RECV;
		str_reset(&req->value);
	}

	ret = str_append_data(&req->value, at, length);
	if (ret < 0)
		return ret;

	return 0;
}

int http_header_complete_cb(llhttp_t *parser)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;

	/* handle header value */
	header_value_check(ws, req);

	if ((ws->selected_auth->type & AUTH_TYPE_GSSAPI) &&
	    ws->auth_state == S_AUTH_INACTIVE && req->spnego_set == 0) {
		/* client retried getting the form without the SPNEGO header, probably
		 * wants a fallback authentication method */
		if (ws_switch_auth_to_next(ws) == 0)
			oclog(ws, LOG_INFO,
			      "no fallback from gssapi authentication");
	}

	req->headers_complete = 1;
	return 0;
}

int http_message_complete_cb(llhttp_t *parser)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;

	req->message_complete = 1;
	return 0;
}

int http_body_cb(llhttp_t *parser, const char *at, size_t length)
{
	struct worker_st *ws = parser->data;
	struct http_req_st *req = &ws->req;
	char *tmp;

	tmp = talloc_realloc_size(ws, req->body, req->body_length + length + 1);
	if (tmp == NULL)
		return 1;

	memcpy(&tmp[req->body_length], at, length);
	req->body_length += length;
	tmp[req->body_length] = 0;

	req->body = tmp;
	return 0;
}

void http_req_init(worker_st *ws)
{
	str_init(&ws->req.header, ws);
	str_init(&ws->req.value, ws);
}

void http_req_reset(worker_st *ws)
{
	ws->req.headers_complete = 0;
	ws->req.message_complete = 0;
	ws->req.body_length = 0;
	ws->req.spnego_set = 0;
	ws->req.url[0] = 0;

	ws->req.header_state = HTTP_HEADER_INIT;
	str_reset(&ws->req.header);
	str_reset(&ws->req.value);
}

void http_req_deinit(worker_st *ws)
{
	http_req_reset(ws);
	str_clear(&ws->req.header);
	str_clear(&ws->req.value);
	talloc_free(ws->req.body);
	ws->req.body = NULL;
}

/* add_owasp_headers:
 * @ws: an initialized worker structure
 *
 * This function adds the OWASP default headers
 * There are security tools that flag the server as a security risk.
 * These are added to help users comply with security best practices.
 */
int add_owasp_headers(worker_st *ws)
{
	unsigned int i;

	for (i = 0; i < GETCONFIG(ws)->included_http_headers_size; i++) {
		if (cstp_printf(ws, "%s",
				GETCONFIG(ws)->included_http_headers[i]) < 0 ||
		    cstp_puts(ws, "\r\n") < 0) {
			return -1;
		}
	}
	return 0;
}
