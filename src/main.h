/*
 * Copyright (C) 2013-2018 Nikos Mavrogiannopoulos
 * Copyright (C) 2015 Red Hat, Inc.
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
#ifndef OC_MAIN_H
#define OC_MAIN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <unistd.h>
#include <net/if.h>
#include <vpn.h>
#include <tlslib.h>
#include "ipc.pb-c.h"
#include <common.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <signal.h>
#include <ev.h>
#include <hmac.h>
#include "vhost.h"
#include <namespace.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <limits.h>
#define SOL_IP IPPROTO_IP
#endif

#define COOKIE_KEY_SIZE 16

extern int saved_argc;
extern char **saved_argv;

extern struct ev_loop *main_loop;
extern ev_timer maintainance_watcher;

#include "log.h"

#define MAIN_MAINTENANCE_TIME (900)

int cmd_parser(void *pool, int argc, char **argv, struct list_head *head,
	       bool worker);

#if defined(CAPTURE_LATENCY_SUPPORT)
#define LATENCY_AGGREGATION_TIME (60)
#endif

#define MINIMUM_USERS_PER_SEC_MOD 500

struct listener_st {
	ev_io io;
	struct list_node list;
	int fd;
	sock_type_t sock_type;

	struct sockaddr_storage addr; /* local socket address */
	socklen_t addr_len;
	int family;
	int protocol;
	ev_timer resume_accept;
};

struct listen_list_st {
	struct list_head head;
	unsigned int total;
};

struct script_wait_st {
	/* must be first so that this structure can behave as ev_child */
	struct ev_child ev_child;

	struct list_node list;

	pid_t pid;
	struct proc_st *proc;
};

/* Each worker process maps to a unique proc_st structure.
 */
typedef struct proc_st {
	/* This is first so this structure can behave as an ev_io */
	struct ev_io io;
	struct ev_child ev_child;

	struct list_node list;
	int fd; /* the command file descriptor */
	pid_t pid;
	unsigned int pid_killed; /* if explicitly disconnected */

	time_t udp_fd_receive_time; /* when the corresponding process has received a UDP fd */

	time_t conn_time; /* the time the user connected */

	/* the tun lease this process has */
	struct tun_lease_st tun_lease;
	struct ip_lease_st *ipv4;
	struct ip_lease_st *ipv6;
	unsigned int leases_in_use; /* someone else got our IP leases */

	struct sockaddr_storage remote_addr; /* peer address (CSTP) */
	socklen_t remote_addr_len;
	/* It can happen that the peer's DTLS stream comes through a different
	 * address. Most likely that's due to interception of the initial TLS/CSTP session */
	struct sockaddr_storage dtls_remote_addr; /* peer address (DTLS) */
	socklen_t dtls_remote_addr_len;
	struct sockaddr_storage our_addr; /* our address */
	socklen_t our_addr_len;

	/* The SID which acts as a cookie */
	uint8_t sid[SID_SIZE];
	unsigned int active_sid;

	/* non zero if the sid has been invalidated and must not be allowed
	 * to reconnect. */
	unsigned int invalidated;

	/* whether the host-update script has already been called */
	unsigned int host_updated;

	/* The DTLS session ID associated with the TLS session
	 * it is either generated or restored from a cookie.
	 */
	uint8_t dtls_session_id[GNUTLS_MAX_SESSION_ID];
	unsigned int
		dtls_session_id_size; /* would act as a flag if session_id is set */

	/* The following are set by the worker process (or by a stored cookie) */
	char username[MAX_USERNAME_SIZE]; /* the owner */
	char groupname[MAX_GROUPNAME_SIZE]; /* the owner's group */
	char hostname[MAX_HOSTNAME_SIZE]; /* the requested hostname */

	/* the following are copied here from the worker process for reporting
	 * purposes (from main-ctl-handler). */
	char user_agent[MAX_AGENT_NAME];
	char device_type[MAX_DEVICE_TYPE];
	char device_platform[MAX_DEVICE_PLATFORM];
	char tls_ciphersuite[MAX_CIPHERSUITE_NAME];
	char dtls_ciphersuite[MAX_CIPHERSUITE_NAME];
	char cstp_compr[8];
	char dtls_compr[8];
	unsigned int mtu;
	unsigned int sec_mod_instance_index;

	/* if the session is initiated by a cookie the following two are set
	 * and are considered when generating an IP address. That is used to
	 * generate the same address as previously allocated.
	 */
	uint8_t ipv4_seed[4];

	unsigned int status; /* PS_AUTH_ */

	/* these are filled in after the worker process dies, using the
	 * Cli stats message. */
	uint64_t bytes_in;
	uint64_t bytes_out;
	uint32_t discon_reason; /* filled on session close */

	unsigned int
		applied_iroutes; /* whether the iroutes in the config have been successfully applied */

	/* The following we rely on talloc for deallocation */
	GroupCfgSt *config; /* custom user/group config */
	int *config_usage_count; /* points to s->config->usage_count */
	/* pointer to perm_cfg - set after we know the virtual host. As
	 * vhosts never get deleted, this pointer is always valid */
	vhost_cfg_st *vhost;
} proc_st;

inline static void kill_proc(proc_st *proc)
{
	kill(proc->pid, SIGTERM);
	proc->pid_killed = 1;
}

struct ip_lease_db_st {
	struct htable ht;
};

struct proc_list_st {
	struct list_head head;
	unsigned int total;
};

struct script_list_st {
	struct list_head head;
};

struct proc_hash_db_st {
	struct htable *db_ip;
	struct htable *db_dtls_ip;
	struct htable *db_dtls_id;
	struct htable *db_sid;
	unsigned int total;
};

#if defined(CAPTURE_LATENCY_SUPPORT)
struct latency_stats_st {
	uint64_t median_total;
	uint64_t rms_total;
	uint64_t sample_count;
};
#endif

struct main_stats_st {
	uint64_t session_timeouts; /* sessions with timeout */
	uint64_t session_idle_timeouts; /* sessions with idle timeout */
	uint64_t session_errors; /* sessions closed with error */
	uint64_t sessions_closed; /* sessions closed since last reset */
	uint64_t kbytes_in;
	uint64_t kbytes_out;
	unsigned int min_mtu;
	unsigned int max_mtu;

	unsigned int active_clients;
	time_t start_time;
	time_t last_reset;

	uint32_t avg_session_mins; /* in minutes */
	uint32_t max_session_mins;
	uint64_t auth_failures; /* authentication failures */

	/* These are counted since start time */
	uint64_t total_auth_failures; /* authentication failures since start_time */
	uint64_t total_sessions_closed; /* sessions closed since start_time */

#if defined(CAPTURE_LATENCY_SUPPORT)
	struct latency_stats_st current_latency_stats;
	struct latency_stats_st delta_latency_stats;
#endif
};

typedef struct sec_mod_instance_st {
	struct main_server_st *server;
	char socket_file[_POSIX_PATH_MAX];
	char full_socket_file[_POSIX_PATH_MAX];
	pid_t sec_mod_pid;

	struct sockaddr_un secmod_addr;
	unsigned int secmod_addr_len;

	int sec_mod_fd; /* messages are sent and received async */
	int sec_mod_fd_sync; /* messages are send in a sync order (ping-pong). Only main sends. */
	/* updated on the cli_stats_msg from sec-mod.
	 * Holds the number of entries in secmod list of users */
	unsigned int secmod_client_entries;
	unsigned int tlsdb_entries;
	uint32_t avg_auth_time; /* in seconds */
	uint32_t max_auth_time; /* in seconds */

} sec_mod_instance_st;

typedef struct if_address_st {
	struct sockaddr if_addr;
	struct sockaddr if_netmask;
} if_address_st;

typedef struct main_server_st {
	/* virtual hosts are only being added to that list, never removed */
	struct list_head *vconfig;

	struct ip_lease_db_st ip_leases;

	struct htable *ban_db;

	struct listen_list_st listen_list;
	struct proc_list_st proc_list;
	struct script_list_st script_list;
	/* maps DTLS session IDs to proc entries */
	struct proc_hash_db_st proc_table;

	struct main_stats_st stats;

	void *auth_extra;

	/* This one is on worker pool */
	struct worker_st *ws;

	unsigned int sec_mod_instance_count;
	sec_mod_instance_st *sec_mod_instances;

	int top_fd;
	int ctl_fd;

	void *main_pool; /* talloc main pool */
	void *config_pool; /* talloc config pool */

	const uint8_t hmac_key[HMAC_DIGEST_SIZE];

	/* used as temporary buffer (currently by forward_udp_to_owner) */
	uint8_t msg_buffer[MAX_MSG_SIZE];

	struct netns_fds netns;

#ifdef RLIMIT_NOFILE
	struct rlimit fd_limits_default_set;
#endif

	struct if_address_st *if_addresses;
	unsigned int if_addresses_count;
} main_server_st;

void clear_lists(main_server_st *s);

int handle_worker_commands(main_server_st *s, struct proc_st *cur);
int handle_sec_mod_commands(sec_mod_instance_st *sec_mod_instances);

int user_connected(main_server_st *s, struct proc_st *cur);
void user_hostname_update(main_server_st *s, struct proc_st *cur);
void user_disconnected(main_server_st *s, struct proc_st *cur);

int send_udp_fd(main_server_st *s, struct proc_st *proc, int fd);

int session_open(sec_mod_instance_st *sec_mod_instance, struct proc_st *proc,
		 const uint8_t *cookie, unsigned int cookie_size);
int session_close(sec_mod_instance_st *sec_mod_instance, struct proc_st *proc);

int open_tun(main_server_st *s, struct proc_st *proc);
void close_tun(main_server_st *s, struct proc_st *proc);
void reset_tun(struct proc_st *proc);
int set_tun_mtu(main_server_st *s, struct proc_st *proc, unsigned int mtu);

int send_cookie_auth_reply(main_server_st *s, struct proc_st *proc, AUTHREP r);

int handle_auth_cookie_req(sec_mod_instance_st *sec_mod_instance,
			   struct proc_st *proc,
			   const AuthCookieRequestMsg *req);

int check_multiple_users(main_server_st *s, struct proc_st *proc);
int handle_script_exit(main_server_st *s, struct proc_st *proc, int code);

void run_sec_mod(sec_mod_instance_st *sec_mod_instance,
		 unsigned int instance_index);

struct proc_st *new_proc(main_server_st *s, pid_t pid, int cmd_fd,
			 struct sockaddr_storage *remote_addr,
			 socklen_t remote_addr_len,
			 struct sockaddr_storage *our_addr,
			 socklen_t our_addr_len, uint8_t *sid, size_t sid_size);

/* kill the pid */
#define RPROC_KILL 1
/* we are on shutdown, don't wait for anything */
#define RPROC_QUIT (1 << 1)

void remove_proc(main_server_st *s, struct proc_st *proc, unsigned int flags);
void proc_to_zombie(main_server_st *s, struct proc_st *proc);

inline static void disconnect_proc(main_server_st *s, proc_st *proc)
{
	/* make sure that the SID cannot be reused */
	proc->invalidated = 1;

	/* if it has a PID, send a signal so that we cleanup
	 * and sec-mod gets stats orderly */
	if (proc->pid != -1 && proc->pid != 0) {
		kill(proc->pid, SIGTERM);
	} else {
		remove_proc(s, proc, RPROC_KILL);
	}
}

void put_into_cgroup(main_server_st *s, const char *cgroup, pid_t pid);

inline static int send_msg_to_worker(main_server_st *s, struct proc_st *proc,
				     uint8_t cmd, const void *msg,
				     pack_size_func get_size, pack_func pack)
{
	mslog(s, proc, LOG_DEBUG, "sending message '%s' to worker",
	      cmd_request_to_str(cmd));
	return send_msg(proc, proc->fd, cmd, msg, get_size, pack);
}

inline static int send_socket_msg_to_worker(main_server_st *s,
					    struct proc_st *proc, uint8_t cmd,
					    int socketfd, const void *msg,
					    pack_size_func get_size,
					    pack_func pack)
{
	mslog(s, proc, LOG_DEBUG, "sending (socket) message %u to worker",
	      (unsigned int)cmd);
	return send_socket_msg(proc, proc->fd, cmd, socketfd, msg, get_size,
			       pack);
}

int secmod_reload(sec_mod_instance_st *sec_mod_instance);

const char *secmod_socket_file_name(struct perm_cfg_st *perm_config);
void restore_secmod_socket_file_name(const char *save_path);
void clear_vhosts(struct list_head *head);

void request_reload(int signo);
void request_stop(int signo);

const struct auth_mod_st *get_auth_mod(void);
const struct auth_mod_st *get_backup_auth_mod(void);

#endif
