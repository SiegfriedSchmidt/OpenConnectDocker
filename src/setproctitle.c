/*
 * Copyright (C) 2013 Nikos Mavrogiannopoulos
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
#include <stdarg.h>
#include <stdio.h>
#include "main.h"

#if !defined(HAVE_SETPROCTITLE)

#if defined(__linux__)
#include <sys/prctl.h>

/* This sets the process title as shown in top, but not in ps (*@#%@).
 * To change the ps name in Linux, one needs to do master black magic
 * trickery (see util-linux setproctitle).
 */
void setproctitle(const char *fmt, ...)
{
	char name[16];
	va_list args;

	va_start(args, fmt);
	vsnprintf(name, sizeof(name) - 1, fmt, args);
	va_end(args);

#ifdef PR_SET_NAME
	prctl(PR_SET_NAME, name);
#endif
	/* Copied systemd's implementation under LGPL by Lennart Poettering */
	if (saved_argc > 0) {
		int i;

		if (saved_argv[0])
			strncpy(saved_argv[0], name, strlen(saved_argv[0]));
		for (i = 1; i < saved_argc; i++) {
			if (!saved_argv[i])
				break;
			memset(saved_argv[i], 0, strlen(saved_argv[i]));
		}
	}
}
#else /* not linux */

void setproctitle(const char *fmt, ...)
{
}

#endif /* __linux__ */

#endif /* HAVE_SETPROCTITLE */
