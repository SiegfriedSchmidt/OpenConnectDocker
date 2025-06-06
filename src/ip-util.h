/*
 * Copyright (C) 2013, 2014 Nikos Mavrogiannopoulos
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
#ifndef OC_IP_UTIL_H
#define OC_IP_UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>

// Lower MTU bound is the value defined in RFC 791
#define RFC_791_MTU (68)
// Upper bound is the maximum DTLS frame size
#define MAX_DTLS_MTU (1 << 14)

void set_mtu_disc(int fd, int family, int val);
int ip_route_sanity_check(void *pool, char **_route);

int ip_cmp(const struct sockaddr_storage *s1,
	   const struct sockaddr_storage *s2);
char *ipv4_prefix_to_strmask(void *pool, unsigned int prefix);
unsigned int ipv6_prefix_to_mask(struct in6_addr *in6, unsigned int prefix);
inline static int valid_ipv6_prefix(unsigned int prefix)
{
	if (prefix > 10 && prefix <= 128)
		return 1;
	else
		return 0;
}

char *ipv4_route_to_cidr(void *pool, const char *route);

/* Helper casts */
#define SA_IN_P(p) (&((struct sockaddr_in *)(p))->sin_addr)
#define SA_IN_U8_P(p) ((uint8_t *)(&((struct sockaddr_in *)(p))->sin_addr))
#define SA_IN6_P(p) (&((struct sockaddr_in6 *)(p))->sin6_addr)
#define SA_IN6_U8_P(p) ((uint8_t *)(&((struct sockaddr_in6 *)(p))->sin6_addr))

#define SA_IN_PORT(p) (((struct sockaddr_in *)(p))->sin_port)
#define SA_IN6_PORT(p) (((struct sockaddr_in6 *)(p))->sin6_port)

#define SA_IN_P_GENERIC(addr, size)                                \
	((size == sizeof(struct sockaddr_in)) ? SA_IN_U8_P(addr) : \
						SA_IN6_U8_P(addr))
#define SA_IN_P_TYPE(addr, type) \
	((type == AF_INET) ? SA_IN_U8_P(addr) : SA_IN6_U8_P(addr))
#define SA_IN_SIZE(size)                                                 \
	((size == sizeof(struct sockaddr_in)) ? sizeof(struct in_addr) : \
						sizeof(struct in6_addr))

char *human_addr2(const struct sockaddr *sa, socklen_t salen, void *buf,
		  size_t buflen, unsigned int full);

#define human_addr(x, y, z, w) human_addr2(x, y, z, w, 1)

#endif
