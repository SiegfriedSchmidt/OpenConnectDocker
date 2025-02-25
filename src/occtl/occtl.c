/*
 * Copyright (C) 2014-2016 Red Hat
 *
 * Author: Nikos Mavrogiannopoulos
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <system.h>
#include <ctype.h>
#include <locale.h>
#include <occtl/occtl.h>

int syslog_open;

static int handle_reset_cmd(CONN_TYPE *conn, const char *arg,
			    cmd_params_st *params);
static int handle_help_cmd(CONN_TYPE *conn, const char *arg,
			   cmd_params_st *params);
static int handle_exit_cmd(CONN_TYPE *conn, const char *arg,
			   cmd_params_st *params);

typedef struct {
	char *name;
	unsigned int name_size;
	char *arg;
	cmd_func func;
	char *doc;
	int always_show;
	int need_preconn;
} commands_st;

#define ENTRY(name, arg, func, doc, show, npc) \
	{ name, sizeof(name) - 1, arg, func, doc, show, npc }

static const commands_st commands[] = {
	ENTRY("disconnect user", "[NAME]", handle_disconnect_user_cmd,
	      "Disconnect the specified user", 1, 1),
	ENTRY("disconnect id", "[ID]", handle_disconnect_id_cmd,
	      "Disconnect the specified ID", 1, 1),
	ENTRY("unban ip", "[IP]", handle_unban_ip_cmd, "Unban the specified IP",
	      1, 1),
	ENTRY("reload", NULL, handle_reload_cmd,
	      "Reloads the server configuration", 1, 1),
	ENTRY("show status", NULL, handle_status_cmd,
	      "Prints the status and statistics of the server", 1, 1),
	ENTRY("show users", NULL, handle_list_users_cmd,
	      "Prints the connected users", 1, 1),
	ENTRY("show ip bans", NULL, handle_list_banned_ips_cmd,
	      "Prints the banned IP addresses", 1, 1),
	ENTRY("show ip ban points", NULL, handle_list_banned_points_cmd,
	      "Prints all the known IP addresses which have points", 1, 1),
	ENTRY("show iroutes", NULL, handle_list_iroutes_cmd,
	      "Prints the routes provided by users of the server", 1, 1),
	ENTRY("show sessions all", NULL, handle_list_all_sessions_cmd,
	      "Prints all the session IDs", 1, 1),
	ENTRY("show sessions valid", NULL, handle_list_valid_sessions_cmd,
	      "Prints all the valid for reconnection sessions", 1, 1),
	ENTRY("show session", "[SID]", handle_show_session_cmd,
	      "Prints information on the specified session", 1, 1),
	ENTRY("show user", "[NAME]", handle_show_user_cmd,
	      "Prints information on the specified user", 1, 1),
	ENTRY("show id", "[ID]", handle_show_id_cmd,
	      "Prints information on the specified ID", 1, 1),
	ENTRY("show events", NULL, handle_events_cmd,
	      "Provides information about connecting users", 1, 1),
	ENTRY("stop", "now", handle_stop_cmd, "Terminates the server", 1, 1),
	ENTRY("reset", NULL, handle_reset_cmd, "Resets the screen and terminal",
	      0, 0),
	ENTRY("help", "or ?", handle_help_cmd, "Prints this help", 0, 0),
	ENTRY("exit", NULL, handle_exit_cmd, "Exits this application", 0, 0),
	/* hidden options */
	ENTRY("?", NULL, handle_help_cmd, "Prints this help", -1, 0),
	ENTRY("quit", NULL, handle_exit_cmd, "Exits this application", -1, 0),
	ENTRY("show cookies all", NULL, handle_list_all_sessions_cmd,
	      "Alias for show sessions all", -1, 1),
	ENTRY("show cookies valid", NULL, handle_list_valid_sessions_cmd,
	      "Alias for show sessions valid", -1, 1),
	{ NULL, 0, NULL, NULL }
};

static void print_commands(unsigned int interactive)
{
	unsigned int i;

	printf("Available Commands\n");
	for (i = 0;; i++) {
		if (commands[i].name == NULL)
			break;

		if (commands[i].always_show == -1)
			continue;

		if (commands[i].always_show == 0 && interactive == 0)
			continue;

		if (commands[i].arg)
			printf(" %12s %s\t%16s\n", commands[i].name,
			       commands[i].arg, commands[i].doc);
		else
			printf(" %16s\t%16s\n", commands[i].name,
			       commands[i].doc);
	}
}

#ifndef HAVE_ORIG_READLINE
#define whitespace(x) c_isspace(x)
#endif

unsigned int need_help(const char *arg)
{
	while (whitespace(*arg))
		arg++;

	if (arg[0] == 0 || (arg[0] == '?' && arg[1] == 0))
		return 1;

	return 0;
}

unsigned int check_cmd_help(const char *line)
{
	unsigned int i;
	unsigned len = (line != NULL) ? strlen(line) : 0;
	unsigned int status = 0, tlen;

	while (len > 0 && (line[len - 1] == '?' || whitespace(line[len - 1])))
		len--;

	for (i = 0;; i++) {
		if (commands[i].name == NULL)
			break;

		tlen = len;
		if (tlen > commands[i].name_size) {
			tlen = commands[i].name_size;
		}

		if (strncasecmp(commands[i].name, line, tlen) == 0) {
			status = 1;
			if (commands[i].arg)
				printf(" %12s %s\t%16s\n", commands[i].name,
				       commands[i].arg, commands[i].doc);
			else
				printf(" %16s\t%16s\n", commands[i].name,
				       commands[i].doc);
		}
	}

	return status;
}

static void usage(void)
{
	printf("occtl: [OPTIONS...] {COMMAND}\n\n");
	printf("  -s --socket-file       Specify the server's occtl socket file\n");
	printf("  -h --help              Show this help\n");
	printf("     --debug             Enable more verbose information in some commands\n");
	printf("  -v --version           Show the program's version\n");
	printf("  -j --json              Use JSON formatting for output\n");
	printf("\n");
	print_commands(0);
	printf("\n");
}

static void version(void)
{
	fprintf(stderr, "OpenConnect server control (occtl) version %s\n",
		VERSION);
	fprintf(stderr, "Copyright (C) 2014-2017 Red Hat and others.\n");
	fprintf(stderr,
		"ocserv comes with ABSOLUTELY NO WARRANTY. This is free software,\n");
	fprintf(stderr,
		"and you are welcome to redistribute it under the conditions of the\n");
	fprintf(stderr, "GNU General Public License version 2.\n");
	fprintf(stderr, "\nFor help type ? or 'help'\n");
	fprintf(stderr,
		"==================================================================\n");
}

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
static char *rl_gets(char *line_read)
{
	/* If the buffer has already been allocated, return the memory
	   to the free pool. */
	if (line_read) {
		free(line_read); /* this is allocated using readline() not talloc */
	}

	/* Get a line from the user. */
	line_read = readline("> ");

	/* If the line has any text in it, save it on the history. */
	if (line_read && *line_read)
		add_history(line_read);

	return line_read;
}

void bytes2human(unsigned long bytes, char *output, unsigned int output_size,
		 const char *suffix)
{
	double data;

	if (suffix == NULL)
		suffix = "";

	if (bytes > 1000 && bytes < 1000 * 1000) {
		data = ((double)bytes) / 1000;
		snprintf(output, output_size, "%.1f kB%s", data, suffix);
	} else if (bytes >= 1000 * 1000 && bytes < 1000 * 1000 * 1000) {
		data = ((double)bytes) / (1000 * 1000);
		snprintf(output, output_size, "%.1f MB%s", data, suffix);
	} else if (bytes >= 1000 * 1000 * 1000) {
		data = ((double)bytes) / (1000 * 1000 * 1000);
		snprintf(output, output_size, "%.1f GB%s", data, suffix);
	} else {
		snprintf(output, output_size, "%lu bytes%s", bytes, suffix);
	}
}

void time2human(uint64_t microseconds, char *output, unsigned int output_size)
{
	if (microseconds < 1000) {
		snprintf(output, output_size, "<1ms");
	} else if (microseconds < 1000000) {
		snprintf(output, output_size, "%" PRIu64 "ms",
			 microseconds / 1000);
	} else {
		snprintf(output, output_size, "%" PRIu64 "s",
			 microseconds / 1000000);
	}
}

static int handle_help_cmd(CONN_TYPE *conn, const char *arg,
			   cmd_params_st *params)
{
	print_commands(1);
	return 0;
}

static int handle_reset_cmd(CONN_TYPE *conn, const char *arg,
			    cmd_params_st *params)
{
	rl_reset_terminal(NULL);
#ifdef HAVE_ORIG_READLINE
	rl_reset_screen_size();
#endif

	return 0;
}

static int handle_exit_cmd(CONN_TYPE *conn, const char *arg,
			   cmd_params_st *params)
{
	exit(EXIT_SUCCESS);
}

/* checks whether an input command of type "  list   users" matches
 * the given cmd (e.g., "list users"). If yes it executes func() and returns true.
 */
static unsigned int check_cmd(const char *cmd, const char *input,
			      CONN_TYPE *conn, int need_preconn, cmd_func func,
			      int *status, cmd_params_st *params)
{
	char *t, *p;
	unsigned int len, tlen;
	unsigned int i, ret = 0;
	char prev;

	while (whitespace(*input))
		input++;

	len = strlen(input);

	t = talloc_size(conn, len + 1);
	if (t == NULL)
		return 0;

	prev = 0;
	p = t;
	for (i = 0; i < len; i++) {
		if (!whitespace(prev) || !whitespace(input[i])) {
			*p = input[i];
			prev = input[i];
			p++;
		}
	}
	*p = 0;
	tlen = p - t;
	len = strlen(cmd);

	if (len == 0)
		goto cleanup;

	if (tlen >= len && strncasecmp(cmd, t, len) == 0 &&
	    cmd[len] == 0) { /* match */
		p = t + len;
		while (whitespace(*p))
			p++;

		if (need_preconn != 0) {
			if (conn_prehandle(conn) < 0) {
				*status = 1;
			} else {
				*status = func(conn, p, params);
			}
		} else {
			*status = func(conn, p, params);
		}

		ret = 1;

		if (need_preconn != 0)
			conn_posthandle(conn);
	}

cleanup:
	talloc_free(t);

	return ret;
}

char *stripwhite(char *string)
{
	register char *s, *t;

	for (s = string; whitespace(*s); s++)
		;

	if (*s == 0)
		return (s);

	t = s + strlen(s) - 1;
	while (t > s && whitespace(*t))
		t--;
	*++t = '\0';

	return s;
}

static int handle_cmd(CONN_TYPE *conn, char *line, cmd_params_st *params)
{
	char *cline;
	unsigned int i;
	int status = 0;

	cline = stripwhite(line);

	if (strlen(cline) == 0)
		return 1;

	for (i = 0;; i++) {
		if (commands[i].name == NULL)
			goto error;

		if (check_cmd(commands[i].name, cline, conn,
			      commands[i].need_preconn, commands[i].func,
			      &status, params) != 0)
			break;
	}

	return status;

error:
	if (check_cmd_help(line) == 0) {
		fprintf(stderr, "unknown command: %s\n", line);
		fprintf(stderr,
			"use help or '?' to get a list of the available commands\n");
	}
	return 1;
}

/* returns an allocated string using malloc(), not talloc,
 * to be compatible with readline() return.
 */
static char *merge_args(int argc, char **argv)
{
	unsigned int size = 0;
	char *data, *p;
	unsigned int i, len;

	for (i = 1; i < argc; i++) {
		size += strlen(argv[i]) + 1;
	}
	size++;

	data = malloc(size);
	if (data == NULL) {
		fprintf(stderr, "memory error\n");
		exit(EXIT_FAILURE);
	}

	p = data;
	for (i = 1; i < argc; i++) {
		len = strlen(argv[i]);
		memcpy(p, argv[i], len);
		p += len;
		*p = ' ';
		p++;
	}

	*p = 0;

	return data;
}

static unsigned int cmd_start;
static char *command_generator(const char *text, int state)
{
	static int list_index, len;
	static int entries_idx;
	unsigned int name_size;
	char *name, *arg;
	char *ret;

	/* If this is a new word to complete, initialize now.  This includes
	   saving the length of TEXT for efficiency, and initializing the index
	   variable to 0. */
	if (!state) {
		list_index = 0;
		entries_idx = 0;
		len = strlen(text);
	}

	/* Return the next name which partially matches from the command list. */
	while ((name = commands[list_index].name)) {
		name_size = commands[list_index].name_size;
		arg = commands[list_index].arg;
		list_index++;

		if (cmd_start > name_size) {
			/* check for user or ID options */
			if (rl_line_buffer != NULL &&
			    strncasecmp(rl_line_buffer, name, name_size) == 0 &&
			    /* make sure only one argument is appended */
			    rl_line_buffer[name_size] != 0 &&
			    strchr(&rl_line_buffer[name_size + 1], ' ') ==
				    NULL) {
				if (arg != NULL) {
					ret = NULL;
					if (strcmp(arg, "[NAME]") == 0)
						ret = search_for_user(
							entries_idx, text, len);
					else if (strcmp(arg, "[ID]") == 0)
						ret = search_for_id(entries_idx,
								    text, len);
					else if (strcmp(arg, "[IP]") == 0)
						ret = search_for_ip(entries_idx,
								    text, len);
					else if (strcmp(arg, "[SID]") == 0)
						ret = search_for_session(
							entries_idx, text, len);
					if (ret != NULL) {
						entries_idx++;
					}
					list_index--; /* restart at the same cmd */
					return ret;
				}
			}

			continue;
		}

		if (cmd_start > 0 && name[cmd_start - 1] != ' ')
			continue;

		if (rl_line_buffer != NULL &&
		    strncasecmp(rl_line_buffer, name, cmd_start) != 0)
			continue;

		name += cmd_start;
		if (strncasecmp(name, text, len) == 0) {
			return strdup(name);
		}
	}

	return NULL;
}

static char **occtl_completion(const char *text, int start, int end)
{
	cmd_start = start;
	return rl_completion_matches(text, command_generator);
}

void handle_sigint(int signo)
{
#ifdef HAVE_ORIG_READLINE
	rl_reset_line_state();
	rl_replace_line("", 0);
	rl_crlf();
#endif
	rl_redisplay();
}

void initialize_readline(void)
{
	rl_readline_name = "occtl";
	rl_attempted_completion_function = occtl_completion;
	rl_completion_entry_function = command_generator;
	rl_completion_query_items = 20;
#ifdef HAVE_ORIG_READLINE
	rl_clear_signals();
#endif
	ocsignal(SIGINT, handle_sigint);
}

static int single_cmd(int argc, char **argv, void *pool, const char *file,
		      cmd_params_st *params)
{
	CONN_TYPE *conn;
	char *line;
	int ret;

	conn = conn_init(pool, file);

	line = merge_args(argc, argv);
	ret = handle_cmd(conn, line, params);

	free(line);
	return ret;
}

int main(int argc, char **argv)
{
	char *line = NULL;
	CONN_TYPE *conn;
	const char *file = NULL;
	void *gl_pool;
	cmd_params_st params;
	int ret;

	/* ensure our string comparisons do not take into account any system
	 * locale. We compare strings that come through our configuration or
	 * network. */
	setlocale(LC_CTYPE, "C");
	setlocale(LC_COLLATE, "C");

	memset(&params, 0, sizeof(params));

	gl_pool = talloc_init("occtl");
	if (gl_pool == NULL) {
		fprintf(stderr, "talloc init error\n");
		exit(EXIT_FAILURE);
	}

	ocsignal(SIGPIPE, SIG_IGN);

	if (argc > 1) {
		while (argc > 1 && argv[1][0] == '-') {
			if (argv[1][1] == 'j' ||
			    (argv[1][1] == '-' && argv[1][2] == 'j')) {
				params.json = 1;

				argv += 1;
				argc -= 1;
			} else if (argv[1][1] == 'n' ||
				   (argv[1][1] == '-' && argv[1][2] == 'n')) {
				params.no_pager = 1;

				argv += 1;
				argc -= 1;
			} else if (argv[1][1] == '-' && argv[1][2] == 'd') {
				params.debug = 1;

				argv += 1;
				argc -= 1;

				if (argc == 1) {
					params.json = 0;
					goto interactive;
				}
			} else if (argv[1][1] == 'v' ||
				   (argv[1][1] == '-' && argv[1][2] == 'v')) {
				version();
				exit(EXIT_SUCCESS);
			} else if (argc > 2 &&
				   (argv[1][1] == 's' ||
				    (argv[1][1] == '-' && argv[1][2] == 's'))) {
				file = talloc_strdup(gl_pool, argv[2]);

				if (argc == 3) {
					params.json = 0;
					goto interactive;
				}

				argv += 2;
				argc -= 2;
			} else {
				usage();
				exit(EXIT_SUCCESS);
			}
		}

		/* handle all arguments as a command */
		ret = single_cmd(argc, argv, gl_pool, file, &params);
		talloc_free(gl_pool);
		exit(ret);
	}

interactive:
	conn = conn_init(gl_pool, file);

	initialize_readline();

	version();
	for (;;) {
		line = rl_gets(line);
		if (line == NULL)
			return 0;

		handle_cmd(conn, line, &params);
	}

	conn_close(conn);

	talloc_free(gl_pool);
	return 0;
}
