/*
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

#include <vpn.h>
#include <worker.h>
#include "common.h"

#ifdef HAVE_GSSAPI

int der_decode(const uint8_t *der, unsigned int der_size, uint8_t *out,
	       unsigned int *out_size, char *realm, unsigned int realm_size,
	       int *error)
{
	int ret, len;
	asn1_node c2 = NULL;

	ret = asn1_create_element(_kkdcp_pkix1_asn, "KKDCP.KDC-PROXY-MESSAGE",
				  &c2);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		return -1;
	}

	ret = asn1_der_decoding(&c2, der, der_size, NULL);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		ret = -1;
		goto cleanup;
	}

	len = *out_size;
	ret = asn1_read_value(c2, "kerb-message", out, &len);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		ret = -1;
		goto cleanup;
	}
	*out_size = len;

	len = realm_size;
	ret = asn1_read_value(c2, "target-domain", realm, &len);
	if (ret != ASN1_SUCCESS) {
		/* no realm was given */
		realm[0] = 0;
	}

	ret = 0;
cleanup:
	asn1_delete_structure(&c2);
	return ret;
}

int der_encode_inplace(uint8_t *raw, unsigned int *raw_size,
		       unsigned int max_size, int *error)
{
	int ret, len;
	asn1_node c2 = NULL;

	ret = asn1_create_element(_kkdcp_pkix1_asn, "KKDCP.KDC-PROXY-MESSAGE",
				  &c2);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		return -1;
	}

	ret = asn1_write_value(c2, "kerb-message", raw, *raw_size);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		ret = -1;
		goto cleanup;
	}

	asn1_write_value(c2, "target-domain", NULL, 0);
	asn1_write_value(c2, "dclocator-hint", NULL, 0);

	len = max_size;

	ret = asn1_der_coding(c2, "", raw, &len, NULL);
	if (ret != ASN1_SUCCESS) {
		*error = ret;
		ret = -1;
		goto cleanup;
	}
	*raw_size = len;

	ret = 0;
cleanup:
	asn1_delete_structure(&c2);
	return ret;
}

/* max UDP size */
#define KKDCP_READ_TIMEOUT 20
#define BUF_SIZE 64 * 1024
int post_kkdcp_handler(worker_st *ws, unsigned int http_ver)
{
	int ret, e, fd = -1;
	struct http_req_st *req = &ws->req;
	unsigned int i, length;
	kkdcp_st *kkdcp = NULL;
	uint8_t *buf;
	uint32_t mlength;
	char realm[128] = "";
	const char *reason = "Unknown";
	kkdcp_realm_st *kr;

	oclog(ws, LOG_INFO, "Processing KKDCP request");

	for (i = 0; i < WSCONFIG(ws)->kkdcp_size; i++) {
		if (WSCONFIG(ws)->kkdcp[i].url &&
		    strcmp(WSCONFIG(ws)->kkdcp[i].url, req->url) == 0) {
			kkdcp = &WSCONFIG(ws)->kkdcp[i];
			break;
		}
	}

	if (kkdcp == NULL) {
		oclog(ws, LOG_HTTP_DEBUG,
		      "could not figure kkdcp handler for %s", req->url);
		return -1;
	}

	if (req->body_length == 0) {
		oclog(ws, LOG_HTTP_DEBUG,
		      "empty body length for kkdcp handler %s", req->url);
		return -1;
	}

	ws_add_score_to_ip(ws, WSCONFIG(ws)->ban_points_kkdcp, 0, 0);
	oclog(ws, LOG_HTTP_DEBUG,
	      "HTTP processing kkdcp framed request: %u bytes",
	      (unsigned int)req->body_length);

	length = BUF_SIZE;
	buf = talloc_size(ws, length);
	if (buf == NULL) {
		oclog(ws, LOG_ERR, "kkdcp: memory error");
		return -1;
	}

	ret = der_decode((uint8_t *)req->body, req->body_length, buf, &length,
			 realm, sizeof(realm), &e);
	if (ret < 0) {
		oclog(ws, LOG_ERR, "kkdcp: DER decoding error: %s",
		      asn1_strerror(e));
		reason = "kkdcp: DER decoding error";
		goto fail;
	}

	kr = &kkdcp->realms[0];
	if (realm[0] != 0 && kkdcp->realms_size > 1) {
		oclog(ws, LOG_DEBUG, "kkdcp: client asked for '%s'", realm);

		for (i = 0; i < kkdcp->realms_size; i++) {
			if (strcmp(kkdcp->realms[i].realm, realm) == 0) {
				kr = &kkdcp->realms[i];
				break;
			}
		}
	}

	fd = socket(kr->ai_family, kr->ai_socktype, kr->ai_protocol);
	if (fd == -1) {
		e = errno;
		oclog(ws, LOG_ERR, "kkdcp: socket error: %s", strerror(e));
		reason = "kkdcp: socket error";
		goto fail;
	}

	ret = connect(fd, (struct sockaddr *)&kr->addr, kr->addr_len);
	if (ret == -1) {
		e = errno;
		oclog(ws, LOG_ERR, "kkdcp: connect error: %s", strerror(e));
		reason = "kkdcp: error connecting to server";
		goto fail;
	}

	oclog(ws, LOG_HTTP_DEBUG, "HTTP sending kkdcp request: %u bytes",
	      (unsigned int)length);
	ret = send(fd, buf, length, 0);
	if (ret != length) {
		if (ret == -1) {
			e = errno;
			oclog(ws, LOG_ERR, "kkdcp: send error: %s",
			      strerror(e));
		} else {
			oclog(ws, LOG_ERR,
			      "kkdcp: send error: only %d were sent", ret);
		}
		reason = "kkdcp: error sending to server";
		goto fail;
	}

	if (kr->ai_socktype == SOCK_DGRAM) {
		ret = recv(fd, buf, BUF_SIZE, 0);
		if (ret == -1) {
			e = errno;
			oclog(ws, LOG_ERR, "kkdcp: recv error: %s",
			      strerror(e));
			reason = "kkdcp: error receiving from server";
			goto fail;
		}

		length = ret;
	} else {
		ret = recv(fd, buf, 4, 0);
		if (ret < 4) {
			e = errno;
			oclog(ws, LOG_ERR, "kkdcp: recv error: %s",
			      strerror(e));
			reason = "kkdcp: error receiving from server";
			goto fail;
		}

		memcpy(&mlength, buf, 4);
		mlength = ntohl(mlength);
		if (mlength >= BUF_SIZE - 4) {
			oclog(ws, LOG_ERR, "kkdcp: too long message (%d bytes)",
			      (int)mlength);
			reason = "kkdcp: error receiving from server";
			goto fail;
		}

		ret = force_read_timeout(fd, buf + 4, mlength,
					 KKDCP_READ_TIMEOUT);
		if (ret == -1) {
			e = errno;
			oclog(ws, LOG_ERR, "kkdcp: recv error: %s",
			      strerror(e));
			reason = "kkdcp: error receiving from server";
			goto fail;
		}
		length = ret + 4;
	}

	oclog(ws, LOG_HTTP_DEBUG, "HTTP processing kkdcp reply: %u bytes",
	      (unsigned int)length);

	cstp_cork(ws);
	ret = cstp_printf(ws, "HTTP/1.%u 200 OK\r\n", http_ver);
	if (ret < 0) {
		goto fail;
	}

	ret = cstp_puts(ws, "Content-Type: application/kerberos\r\n");
	if (ret < 0) {
		goto fail;
	}

	ret = der_encode_inplace(buf, &length, BUF_SIZE, &e);
	if (ret < 0) {
		oclog(ws, LOG_ERR, "kkdcp: DER encoding error: %s",
		      asn1_strerror(e));
		reason = "kkdcp: DER encoding error";
		goto fail;
	}

	oclog(ws, LOG_HTTP_DEBUG, "HTTP sending kkdcp framed reply: %u bytes",
	      (unsigned int)length);
	ret = cstp_printf(ws, "Content-Length: %u\r\n", (unsigned int)length);
	if (ret < 0) {
		goto fail;
	}

	ret = cstp_puts(ws, "Connection: Keep-Alive\r\n");
	if (ret < 0) {
		goto fail;
	}

	ret = add_owasp_headers(ws);
	if (ret < 0) {
		goto fail;
	}

	ret = cstp_puts(ws, "\r\n");
	if (ret < 0) {
		goto fail;
	}

	ret = cstp_send(ws, buf, length);
	if (ret < 0) {
		goto fail;
	}

	ret = cstp_uncork(ws);
	if (ret < 0) {
		goto fail;
	}

	ret = 0;
	goto cleanup;
fail:
	(void)cstp_printf(ws, "HTTP/1.%u 502 %s\r\n\r\n", http_ver, reason);
	ret = -1;

cleanup:
	talloc_free(buf);
	if (fd != -1)
		close(fd);
	return ret;
}

#else

int post_kkdcp_handler(worker_st *ws, unsigned int http_ver)
{
	return -1;
}
#endif
