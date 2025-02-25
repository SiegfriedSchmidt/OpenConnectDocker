/*
 * Copyright (C) 2013-2023 Nikos Mavrogiannopoulos
 * Copyright (C) 2014 Red Hat, Inc.
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
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <limits.h>
#include <common.h>
#include <ip-util.h>

#ifdef HAVE_RADIUS

#include <vpn.h>
#include <main.h>
#include <sec-mod-sup-config.h>
#include <auth/radius.h>
#include "log.h"

static int get_sup_config(struct cfg_st *cfg, client_entry_st *entry,
			  SecmSessionReplyMsg *msg, void *pool)
{
	struct radius_ctx_st *pctx = entry->auth_ctx;
	unsigned int dns = 0, i;

	if (pctx == NULL)
		return 0;

	msg->config->interim_update_secs = pctx->interim_interval_secs;
	if (msg->config->interim_update_secs > 0)
		msg->config->has_interim_update_secs = 1;

	msg->config->session_timeout_secs = pctx->session_timeout_secs;
	if (msg->config->session_timeout_secs > 0)
		msg->config->has_session_timeout_secs = 1;

	if (pctx->ipv4[0] != 0) {
		msg->config->explicit_ipv4 = talloc_strdup(pool, pctx->ipv4);
	}

	if (pctx->ipv4_mask[0] != 0) {
		msg->config->ipv4_netmask =
			talloc_strdup(pool, pctx->ipv4_mask);
	}

	if (pctx->routes_size > 0) {
		msg->config->routes =
			talloc_size(pool, pctx->routes_size * sizeof(char *));
		if (msg->config->routes != NULL) {
			for (i = 0; i < pctx->routes_size; i++) {
				msg->config->routes[i] =
					talloc_strdup(pool, pctx->routes[i]);
				if (msg->config->routes[i] == NULL) {
					oc_syslog(
						LOG_ERR,
						"Error allocating memory for routes");
					return -1;
				}
				if (ip_route_sanity_check(
					    msg->config->routes,
					    &msg->config->routes[i]) < 0) {
					oc_syslog(LOG_ERR,
						  "Route '%s' is malformed",
						  msg->config->routes[i]);
					return -1;
				}
			}
			msg->config->n_routes = pctx->routes_size;
		}
	}

	if (pctx->ipv4_dns1[0] != 0)
		dns++;
	if (pctx->ipv4_dns2[0] != 0)
		dns++;
	if (pctx->ipv6_dns1[0] != 0)
		dns++;
	if (pctx->ipv6_dns2[0] != 0)
		dns++;

	if (dns > 0) {
		msg->config->dns = talloc_size(pool, dns * sizeof(char *));
		if (msg->config->dns != NULL) {
			unsigned int pos = 0;

			if (pctx->ipv4_dns1[0] != 0)
				msg->config->dns[pos++] =
					talloc_strdup(pool, pctx->ipv4_dns1);
			if (pctx->ipv4_dns2[0] != 0)
				msg->config->dns[pos++] =
					talloc_strdup(pool, pctx->ipv4_dns2);
			if (pctx->ipv6_dns1[0] != 0)
				msg->config->dns[pos++] =
					talloc_strdup(pool, pctx->ipv6_dns1);
			if (pctx->ipv6_dns2[0] != 0)
				msg->config->dns[pos++] =
					talloc_strdup(pool, pctx->ipv6_dns2);

			msg->config->n_dns = dns;
		}
	}

	if (pctx->ipv6[0] != 0) {
		msg->config->explicit_ipv6 = talloc_strdup(pool, pctx->ipv6);
	}

	if (pctx->ipv6_net[0] != 0) {
		msg->config->ipv6_net = talloc_strdup(pool, pctx->ipv6_net);
	}

	if (pctx->ipv6_subnet_prefix != 0) {
		msg->config->ipv6_subnet_prefix = pctx->ipv6_subnet_prefix;
		msg->config->has_ipv6_subnet_prefix = 1;
	}

	if (pctx->rx_per_sec) {
		msg->config->has_rx_per_sec = 1;
		msg->config->rx_per_sec = pctx->rx_per_sec;
	}
	if (pctx->tx_per_sec) {
		msg->config->has_tx_per_sec = 1;
		msg->config->tx_per_sec = pctx->tx_per_sec;
	}

	return 0;
}

struct config_mod_st radius_sup_config = {
	.get_sup_config = get_sup_config,
};

#endif
