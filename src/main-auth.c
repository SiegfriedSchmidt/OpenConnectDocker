/*
 * Copyright (C) 2013, 2014 Nikos Mavrogiannopoulos
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
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <tlslib.h>
#include <script-list.h>
#include <ip-lease.h>
#include "str.h"

#include <vpn.h>
#include <cookies.h>
#include <tun.h>
#include <main.h>
#include <ccan/list/list.h>
#include <main-auth.h>
#include <plain.h>
#include <common.h>
#include <pam.h>

static const struct auth_mod_st *module = NULL;

void main_auth_init(main_server_st *s)
{
#ifdef HAVE_PAM
	if ((s->config->auth_types & pam_auth_funcs.type) == pam_auth_funcs.type)
		module = &pam_auth_funcs;
	else
#endif
	if ((s->config->auth_types & plain_auth_funcs.type) == plain_auth_funcs.type) {
		module = &plain_auth_funcs;
		s->auth_extra = s->config->plain_passwd;
	}
}

int send_auth_reply(main_server_st* s, struct proc_st* proc,
			AuthReplyMsg__AUTHREP r, unsigned need_sid)
{
	AuthReplyMsg msg = AUTH_REPLY_MSG__INIT;
	unsigned i;
	int ret;

	if (proc->config.routes_size > MAX_ROUTES) {
		mslog(s, proc, LOG_INFO, "note that the routes sent to the client (%d) exceed the maximum allowed (%d). Truncating.", (int)proc->config.routes_size, (int)MAX_ROUTES);
		proc->config.routes_size = MAX_ROUTES;
	}

	if (r == AUTH_REPLY_MSG__AUTH__REP__OK && proc->tun_lease.name[0] != 0) {
		char ipv6[MAX_IP_STR];
		char ipv4[MAX_IP_STR];
		char ipv6_local[MAX_IP_STR];
		char ipv4_local[MAX_IP_STR];

		/* fill message */
		msg.reply = AUTH_REPLY_MSG__AUTH__REP__OK;
		msg.has_cookie = 1;
		msg.cookie.data = proc->cookie;
		msg.cookie.len = COOKIE_SIZE;

		msg.has_session_id = 1;
		msg.session_id.data = proc->dtls_session_id;
		msg.session_id.len = sizeof(proc->dtls_session_id);

		msg.vname = proc->tun_lease.name;
		msg.user_name = proc->username;

		if (proc->ipv4 && proc->ipv4->rip_len > 0) {
			msg.ipv4 = human_addr2((struct sockaddr*)&proc->ipv4->rip, proc->ipv4->rip_len,
					ipv4, sizeof(ipv4), 0);
			msg.ipv4_local = human_addr2((struct sockaddr*)&proc->ipv4->lip, proc->ipv4->lip_len,
					ipv4_local, sizeof(ipv4_local), 0);
		}

		if (proc->ipv6 && proc->ipv6->rip_len > 0) {
			msg.ipv6 = human_addr2((struct sockaddr*)&proc->ipv6->rip, proc->ipv6->rip_len,
					ipv6, sizeof(ipv6), 0);
			msg.ipv6_local = human_addr2((struct sockaddr*)&proc->ipv6->lip, proc->ipv6->lip_len,
					ipv6_local, sizeof(ipv6_local), 0);
		}

		msg.ipv4_netmask = proc->config.ipv4_netmask;
		msg.ipv6_netmask = proc->config.ipv6_netmask;
		msg.ipv6_prefix = proc->config.ipv6_prefix;
		if (proc->config.rx_per_sec != 0) {
			msg.has_rx_per_sec = 1;
			msg.rx_per_sec = proc->config.rx_per_sec;
		}

		if (proc->config.tx_per_sec != 0) {
			msg.has_tx_per_sec = 1;
			msg.tx_per_sec = proc->config.tx_per_sec;
		}

		if (proc->config.net_priority != 0) {
			msg.has_net_priority = 1;
			msg.net_priority = proc->config.net_priority;
		}

		msg.n_dns = proc->config.dns_size;
		for (i=0;i<proc->config.dns_size;i++) {
			mslog(s, proc, LOG_DEBUG, "sending dns '%s'", proc->config.dns[i]);
			msg.dns = proc->config.dns;
		}

		msg.n_nbns = proc->config.nbns_size;
		for (i=0;i<proc->config.nbns_size;i++) {
			mslog(s, proc, LOG_DEBUG, "sending nbns '%s'", proc->config.nbns[i]);
			msg.nbns = proc->config.nbns;
		}

		msg.n_routes = proc->config.routes_size;
		for (i=0;i<proc->config.routes_size;i++) {
			mslog(s, proc, LOG_DEBUG, "sending route '%s'", proc->config.routes[i]);
			msg.routes = proc->config.routes;
		}

		if (need_sid) {
			msg.has_sid = 1;
			msg.sid.data = proc->sid;
			msg.sid.len = sizeof(proc->sid);
		}

		ret = send_socket_msg_to_worker(s, proc, AUTH_REP, proc->tun_lease.fd,
			 &msg,
			 (pack_size_func)auth_reply_msg__get_packed_size,
			 (pack_func)auth_reply_msg__pack);
	} else {
		msg.reply = AUTH_REPLY_MSG__AUTH__REP__FAILED;

		ret = send_msg_to_worker(s, proc, AUTH_REP,
			 &msg,
			 (pack_size_func)auth_reply_msg__get_packed_size,
			 (pack_func)auth_reply_msg__pack);
	}

	if (ret < 0) {
		int e = errno;
		mslog(s, proc, LOG_ERR, "send_msg: %s", strerror(e));
		return ret;
	}

	return 0;
}

int send_auth_reply_msg(main_server_st* s, struct proc_st* proc, unsigned need_sid)
{
	AuthReplyMsg msg = AUTH_REPLY_MSG__INIT;
	char tmp[MAX_MSG_SIZE] = "";

	int ret;

	if (proc->auth_ctx == NULL)
		return -1;

	ret = module->auth_msg(proc->auth_ctx, tmp, sizeof(tmp));
	if (ret < 0)
		return ret;

	msg.msg = tmp;
	msg.reply = AUTH_REPLY_MSG__AUTH__REP__MSG;

	if (need_sid) {
		msg.has_sid = 1;
		msg.sid.data = proc->sid;
		msg.sid.len = sizeof(proc->sid);
	}

	ret = send_msg_to_worker(s, proc, AUTH_REP, &msg, 
		(pack_size_func)auth_reply_msg__get_packed_size,
		(pack_func)auth_reply_msg__pack);
	if (ret < 0) {
		mslog(s, proc, LOG_ERR, "send_msg error");
	}

	return ret;
}

static int check_user_group_status(main_server_st *s, struct proc_st* proc,
		     int tls_auth_ok, const char* cert_user, const char* cert_group)
{
	if (s->config->auth_types & AUTH_TYPE_CERTIFICATE) {
		if (tls_auth_ok == 0 && s->config->cisco_client_compat == 0) {
			mslog(s, proc, LOG_INFO, "user '%s' presented no certificate", proc->username);
			return -1;
		}

		if (tls_auth_ok != 0) {
			if (proc->username[0] == 0) {
				snprintf(proc->username, sizeof(proc->username), "%s", cert_user);
				snprintf(proc->groupname, sizeof(proc->groupname), "%s", cert_group);
			} else {
				if (strcmp(proc->username, cert_user) != 0) {
					mslog(s, proc, LOG_INFO, "user '%s' presented a certificate from user '%s'", proc->username, cert_user);
					return -1;
				}

				if (s->config->cert_group_oid != NULL && strcmp(proc->groupname, cert_group) != 0) {
					mslog(s, proc, LOG_INFO, "user '%s' presented a certificate from group '%s' but he is member of '%s'", proc->username, cert_group, proc->groupname);
					return -1;
				}
			}
		}
	}

	return 0;
}

int handle_auth_cookie_req(main_server_st* s, struct proc_st* proc,
 			   const AuthCookieRequestMsg * req)
{
int ret;
struct stored_cookie_st sc;
time_t now = time(0);

	if (req->cookie.len == 0 || req->cookie.len != sizeof(proc->cookie))
		return -1;

	ret = decrypt_cookie(s, req->cookie.data, req->cookie.len, &sc);
	if (ret < 0)
		return -1;

	if (sc.expiration < now)
		return -1;

	memcpy(proc->cookie, req->cookie.data, req->cookie.len);
	memcpy(proc->username, sc.username, sizeof(proc->username));
	memcpy(proc->groupname, sc.groupname, sizeof(proc->groupname));
	memcpy(proc->hostname, sc.hostname, sizeof(proc->hostname));
	memcpy(proc->dtls_session_id, sc.session_id, sizeof(proc->dtls_session_id));
	proc->dtls_session_id_size = sizeof(proc->dtls_session_id);

	proc->username[sizeof(proc->username)-1] = 0;
	proc->groupname[sizeof(proc->groupname)-1] = 0;
	proc->hostname[sizeof(proc->hostname)-1] = 0;

	memcpy(proc->ipv4_seed, sc.ipv4_seed, sizeof(proc->ipv4_seed));
	proc->seeds_are_set = 1;

	ret = check_user_group_status(s, proc, req->tls_auth_ok, req->cert_user_name, req->cert_group_name);
	if (ret < 0)
		return ret;

	return 0;
}

int handle_auth_init(main_server_st *s, struct proc_st* proc,
		     const AuthInitMsg * req)
{
int ret = -1;
char ipbuf[128];
const char* ip;

	ip = human_addr((void*)&proc->remote_addr, proc->remote_addr_len,
			ipbuf, sizeof(ipbuf));

	if (req->user_name == NULL && (s->config->auth_types & AUTH_TYPE_USERNAME_PASS)) {
		mslog(s, proc, LOG_DEBUG, "auth init from '%s' with no username present", ip);
		return -1;
        }

	if (req->hostname != NULL) {
		snprintf(proc->hostname, sizeof(proc->hostname), "%s", req->hostname);
	}

	if (s->config->auth_types & AUTH_TYPE_USERNAME_PASS) {
		/* req->username is non-null at this point */
		ret = module->auth_init(&proc->auth_ctx, proc, req->user_name, ip, s->auth_extra);
		if (ret < 0)
			return ret;

		ret = module->auth_group(proc->auth_ctx, proc->groupname, sizeof(proc->groupname));
		if (ret != 0)
			return -1;
		proc->groupname[sizeof(proc->groupname)-1] = 0;

		/* a module is allowed to change the name of the user */
		ret = module->auth_user(proc->auth_ctx, proc->username, sizeof(proc->username));
		if (ret != 0 && req->user_name != NULL) {
			snprintf(proc->username, MAX_USERNAME_SIZE, "%s", req->user_name);
		}
	}

	ret = check_user_group_status(s, proc, req->tls_auth_ok, req->cert_user_name, req->cert_group_name);
	if (ret < 0)
		return ret;

	mslog(s, proc, LOG_DEBUG, "auth init for user '%s' from '%s'", proc->username, ip);

	if (s->config->auth_types & AUTH_TYPE_USERNAME_PASS) {
                return ERR_AUTH_CONTINUE;
	}

	return 0;
}

int handle_auth_reinit(main_server_st *s, struct proc_st** _proc,
		     const AuthReinitMsg * req)
{
char ipbuf[128];
const char* ip;
struct proc_st *ctmp = NULL;
struct proc_st *proc = *_proc;
unsigned found = 0;

	ip = human_addr((void*)&proc->remote_addr, proc->remote_addr_len,
			ipbuf, sizeof(ipbuf));

	if (req->sid.len != SID_SIZE) {
		mslog(s, proc, LOG_DEBUG, "auth reinit from '%s' with no SID present", ip);
	        return -1;
        }

	if (req->password == NULL) {
		mslog(s, proc, LOG_DEBUG, "auth reinit from '%s' with no password present", ip);
	        return -1;
        }

	/* search all procs for a matching SID */
	list_for_each(&s->proc_list.head, ctmp, list) {
		if (ctmp->status == PS_AUTH_ZOMBIE) {
			if (memcmp(req->sid.data, ctmp->sid, SID_SIZE) == 0) {
				/* replace sessions */
				ctmp->pid = proc->pid;
				ctmp->fd = proc->fd;
				memcpy(&ctmp->remote_addr, &proc->remote_addr, proc->remote_addr_len);

				proc->pid = -1;
				proc->fd = -1;
				proc->status = PS_AUTH_DEAD;
				*_proc = proc = ctmp;
				found = 1;
				break;
			}
		}
	}

	if (found == 0) {
		mslog_hex(s, proc, LOG_DEBUG, "auth reinit received but does not match any session with SID", req->sid.data, req->sid.len, 1);
		return -1;
	}

	if (proc->auth_ctx == NULL) {
		mslog(s, proc, LOG_ERR, "auth req but with no context!");
		return -1;
        }

	mslog(s, proc, LOG_DEBUG, "auth reinit for user '%s' from '%s'", proc->username, ip);

	return module->auth_pass(proc->auth_ctx, req->password, strlen(req->password));
}

int handle_auth_req(main_server_st *s, struct proc_st* proc,
		    const AuthRequestMsg * req)
{
	if (proc->auth_ctx == NULL) {
		mslog(s, proc, LOG_ERR, "auth req but with no context!");
		return -1;
        }
	mslog(s, proc, LOG_DEBUG, "auth req for user '%s'", proc->username);

	if (req->password == NULL)
	        return -1;
	        
	return module->auth_pass(proc->auth_ctx, req->password, strlen(req->password));
}

/* Checks for multiple users. 
 * 
 * It returns a negative error code if more than the maximum allowed
 * users are found.
 * 
 * In addition this function will also check whether the cookie
 * used had been re-used before, and then disconnect the old session
 * (cookies are unique). 
 */
int check_multiple_users(main_server_st *s, struct proc_st* proc)
{
struct proc_st *ctmp = NULL, *cpos;
unsigned int entries = 1; /* that one */

	list_for_each_safe(&s->proc_list.head, ctmp, cpos, list) {
		if (ctmp != proc && ctmp->pid != -1) {
			if (memcmp(proc->cookie, ctmp->cookie, sizeof(proc->cookie)) == 0) {
				mslog(s, ctmp, LOG_DEBUG, "disconnecting '%s' due to new cookie connection", ctmp->username);

				/* steal its leases */
				steal_ip_leases(ctmp, proc);

				kill(ctmp->pid, SIGTERM);
			} else if (strcmp(proc->username, ctmp->username) == 0) {
				entries++;
			}
		}
	}

	if (s->config->max_same_clients && entries > s->config->max_same_clients)
		return -1;

	return 0;
}

void proc_auth_deinit(main_server_st* s, struct proc_st* proc)
{
	mslog(s, proc, LOG_DEBUG, "auth deinit for user '%s'", proc->username);
	if (proc->auth_ctx != NULL) {
		module->auth_deinit(proc->auth_ctx);
		proc->auth_ctx = NULL;
	}
}
