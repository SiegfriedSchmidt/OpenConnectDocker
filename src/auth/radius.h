/*
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of ocserv.
 *
 * ocserv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#ifndef RADIUS_H
#define RADIUS_H

#include <sec-mod-auth.h>
#include "common/common.h"

#ifdef HAVE_RADIUS

#ifdef LEGACY_RADIUS
#include <freeradius-client.h>
#else
#include <radcli/radcli.h>
#endif

struct radius_vhost_ctx {
	rc_handle *rh;
	char nas_identifier[64];
};

struct radius_ctx_st {
	char username[MAX_USERNAME_SIZE * 2];
	char user_agent[MAX_AGENT_NAME];

	char *groupnames[MAX_GROUPS];
	unsigned int groupnames_size;

	char remote_ip[MAX_IP_STR];
	char our_ip[MAX_IP_STR];
	unsigned int interim_interval_secs;
	unsigned int session_timeout_secs;

	/* variables for configuration */
	char ipv4[MAX_IP_STR];
	char ipv4_mask[MAX_IP_STR];
	char ipv4_dns1[MAX_IP_STR];
	char ipv4_dns2[MAX_IP_STR];

	char ipv6[MAX_IP_STR];
	char ipv6_net[MAX_IP_STR];
	uint16_t ipv6_subnet_prefix;
	char ipv6_dns1[MAX_IP_STR];
	char ipv6_dns2[MAX_IP_STR];

	uint32_t rx_per_sec;
	uint32_t tx_per_sec;

	char **routes;
	unsigned int routes_size;

	char pass_msg[PW_MAX_MSG_SIZE];
	unsigned int retries;
	unsigned int id;

	struct radius_vhost_ctx *vctx;
	char *state;
	unsigned int passwd_counter;
	size_t prev_prompt_hash;
};

extern const struct auth_mod_st radius_auth_funcs;

#endif
#endif
