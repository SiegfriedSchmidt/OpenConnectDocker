#ifndef OC_AUTH_UNIX_H
#define OC_AUTH_UNIX_H

#include <config.h>

#if defined(HAVE_GSSAPI) || defined(HAVE_PAM)
#define HAVE_GET_USER_AUTH_GROUP
#endif

#ifdef HAVE_GET_USER_AUTH_GROUP
int get_user_auth_group(const char *username, const char *suggested,
			char *groupname, int groupname_size);
void unix_group_list(void *pool, unsigned int gid_min, char ***groupname,
		     unsigned int *groupname_size);
#endif

#endif
