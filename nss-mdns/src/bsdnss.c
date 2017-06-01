/* $Id$ */

/***
    This file is part of nss-mdns.
·
    nss-mdns is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.
·
    nss-mdns is distributed in the hope that it will be useful, but1
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.
·
    You should have received a copy of the GNU Lesser General Public License
    along with nss-mdns; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
    USA.
***/

/* Original author: Bruce M. Simpson <bms@FreeBSD.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/ktrace.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <nss.h>

#include <netinet/in.h>
#include <netdb.h>

#include "config.h"

#ifdef MDNS_MINIMAL
/*
 * FreeBSD support prefers Avahi.
 */
#endif

#if defined(NSS_IPV4_ONLY) || defined(NSS_IPV6_ONLY)
/*
 * FreeBSD's libc is always built with IPv4 support.
 * There is no way of telling at compile time with a define if libc
 * was built with -DINET6 or not; a configure test would be required.
 * Therefore, distinguishing between the two makes no sense.
 */
#define NO_BUILD_BSD_NSS
#endif

#ifndef NO_BUILD_BSD_NSS
/*
 * To turn on utrace() records, compile with -DDEBUG_UTRACE.
 */
#ifdef DEBUG_UTRACE
#define _NSS_UTRACE(msg)						\
	do {								\
		static const char __msg[] = msg ;			\
		(void)utrace(__msg, sizeof(__msg));			\
	} while (0)
#else
#define _NSS_UTRACE(msg)
#endif

ns_mtab *nss_module_register(const char *source, unsigned int *mtabsize,
			     nss_module_unregister_fn *unreg);

extern enum nss_status _nss_mdns_gethostbyname_r (const char *name, struct hostent * result,
			   char *buffer, size_t buflen, int *errnop,
			   int *h_errnop);

extern enum nss_status _nss_mdns_gethostbyname2_r (const char *name, int af, struct hostent * result,
			    char *buffer, size_t buflen, int *errnop,
			    int *h_errnop);
extern enum nss_status _nss_mdns_gethostbyaddr_r (struct in_addr * addr, int len, int type,
			   struct hostent * result, char *buffer,
			   size_t buflen, int *errnop, int *h_errnop);
extern enum nss_status _nss_mdns4_gethostbyname_r (const char *name, struct hostent * result,
			   char *buffer, size_t buflen, int *errnop,
			   int *h_errnop);

extern enum nss_status _nss_mdns4_gethostbyname2_r (const char *name, int af, struct hostent * result,
			    char *buffer, size_t buflen, int *errnop,
			    int *h_errnop);
extern enum nss_status _nss_mdns4_gethostbyaddr_r (struct in_addr * addr, int len, int type,
			   struct hostent * result, char *buffer,
			   size_t buflen, int *errnop, int *h_errnop);
extern enum nss_status _nss_mdns6_gethostbyname_r (const char *name, struct hostent * result,
			   char *buffer, size_t buflen, int *errnop,
			   int *h_errnop);

extern enum nss_status _nss_mdns6_gethostbyname2_r (const char *name, int af, struct hostent * result,
			    char *buffer, size_t buflen, int *errnop,
			    int *h_errnop);
extern enum nss_status _nss_mdns6_gethostbyaddr_r (struct in_addr * addr, int len, int type,
			   struct hostent * result, char *buffer,
			   size_t buflen, int *errnop, int *h_errnop);

typedef enum nss_status 	(*_bsd_nsstub_fn_t)(const char *, struct hostent *, char *, size_t, int *, int *);

/* XXX: FreeBSD 5.x is not supported. */
static NSS_METHOD_PROTOTYPE(__nss_bsdcompat_getaddrinfo);
static NSS_METHOD_PROTOTYPE(__nss_bsdcompat_gethostbyaddr_r);
static NSS_METHOD_PROTOTYPE(__nss_bsdcompat_gethostbyname2_r);
static NSS_METHOD_PROTOTYPE(__nss_bsdcompat_ghbyaddr);
static NSS_METHOD_PROTOTYPE(__nss_bsdcompat_ghbyname);

static ns_mtab methods[] = {
    /* database, name, method, mdata */
    { NSDB_HOSTS, "getaddrinfo", __nss_bsdcompat_getaddrinfo, NULL },
    { NSDB_HOSTS, "gethostbyaddr_r", __nss_bsdcompat_gethostbyaddr_r, NULL },
    { NSDB_HOSTS, "gethostbyname2_r", __nss_bsdcompat_gethostbyname2_r, NULL },
    { NSDB_HOSTS, "ghbyaddr", __nss_bsdcompat_ghbyaddr, NULL },
    { NSDB_HOSTS, "ghbyname", __nss_bsdcompat_ghbyname, NULL },
};

ns_mtab *
nss_module_register(const char *source, unsigned int *mtabsize,
    nss_module_unregister_fn *unreg)
{

	*mtabsize = sizeof(methods)/sizeof(methods[0]);
	*unreg = NULL;
	return (methods);
}

/*
 * Calling convention:
 * ap: const char *name (optional), struct addrinfo *pai (hints, optional)
 * retval: struct addrinfo **
 *
 * TODO: Map all returned hostents, not just the first match.
 *
 * name must always be specified by libc; pai is allocated
 * by libc and must always be specified.
 *
 * We can malloc() addrinfo instances and hang them off ai->next;
 * canonnames may also be malloc()'d.
 * libc is responsible for mapping our ns error return to gai_strerror().
 *
 * libc calls us only to look up qualified hostnames. We don't need to
 * worry about port numbers; libc will call getservbyname() and explore
 * the appropriate maps configured in nsswitch.conf(5).
 *
 * _errno and _h_errno are unused by getaddrinfo(), as it is
 * [mostly] OS independent interface implemented by Win32.
 */
static int
__nss_bsdcompat_getaddrinfo(void *retval, void *mdata __unused, va_list ap)
{
	struct addrinfo		 sentinel;
	struct addrinfo		*ai;
	char			*buffer;
	void 			*cbufp;	/* buffer handed to libc */
	char			*hap;
	struct hostent		*hp;
	void 			*mbufp;	/* buffer handed to mdns */
	const char 		*name;
	const struct addrinfo	*pai;
	struct sockaddr		*psa;	/* actually *sockaddr_storage */
	struct addrinfo		**resultp;
	int			 _errno;
	int			 _h_errno;
	size_t			 mbuflen = 1024;
	enum nss_status		 status;

	_NSS_UTRACE("__nss_bsdcompat_getaddrinfo: called");

	_h_errno = _errno = 0;
	status = NSS_STATUS_UNAVAIL;

	name = va_arg(ap, const char *);
	pai = va_arg(ap, struct addrinfo *);
	resultp = (struct addrinfo **)retval;

	/* XXX: Will be used to hang off multiple matches later. */
	memset(&sentinel, 0, sizeof(sentinel));

	if (name == NULL || pai == NULL) {
		*resultp = sentinel.ai_next;
		return (NS_UNAVAIL);
	}

	mbufp = malloc((sizeof(struct hostent) + mbuflen));
	if (mbufp == NULL) {
		*resultp = sentinel.ai_next;
		return (NS_UNAVAIL);
	}
	hp = (struct hostent *)mbufp;
	buffer = (char *)(hp + 1);

	cbufp = malloc(sizeof(struct addrinfo) +
	    sizeof(struct sockaddr_storage));
	if (cbufp == NULL) {
		free(mbufp);
		*resultp = sentinel.ai_next;
		return (NS_UNAVAIL);
	}
	ai = (struct addrinfo *)cbufp;
	psa = (struct sockaddr *)(ai + 1);

	/*
	 * 1. Select which function to call based on the address family.
	 * 2. Map hostent to addrinfo.
	 * 3. Hand-off buffer to libc.
	 */
	switch (pai->ai_family) {
	case AF_UNSPEC:
		status = _nss_mdns_gethostbyname_r(name, hp, buffer, mbuflen,
						   &_errno, &_h_errno);
		break;
	case AF_INET:
		status = _nss_mdns4_gethostbyname_r(name, hp, buffer, mbuflen,
						    &_errno, &_h_errno);
		break;
	case AF_INET6:
		status = _nss_mdns6_gethostbyname_r(name, hp, buffer, mbuflen,
						    &_errno, &_h_errno);
		break;
	default:
		break;
	}
	status = __nss_compat_result(status, _errno);

	if (status == NS_SUCCESS) {
		memset(ai, 0, sizeof(struct addrinfo));
		ai->ai_flags = pai->ai_flags;
		ai->ai_socktype = pai->ai_socktype;
		ai->ai_protocol = pai->ai_protocol;
		ai->ai_family = hp->h_addrtype;
		memset(psa, 0, sizeof(struct sockaddr_storage));
		psa->sa_len = ai->ai_addrlen;
		psa->sa_family = ai->ai_family;
		ai->ai_addr = psa;
		hap = hp->h_addr_list[0];
		switch (ai->ai_family) {
		case AF_INET:
			ai->ai_addrlen = sizeof(struct sockaddr_in);
			memcpy(&((struct sockaddr_in *)psa)->sin_addr, hap,
			    ai->ai_addrlen);
			break;
		case AF_INET6:
			ai->ai_addrlen = sizeof(struct sockaddr_in6);
			memcpy(&((struct sockaddr_in6 *)psa)->sin6_addr, hap,
			    ai->ai_addrlen);
			break;
		default:
			ai->ai_addrlen = sizeof(struct sockaddr_storage);
			memcpy(psa->sa_data, hap, ai->ai_addrlen);
		}
		sentinel.ai_next = ai;
		free(mbufp);
	}

	if (sentinel.ai_next == NULL) {
		free(cbufp);
		free(mbufp);
	}

	*resultp = sentinel.ai_next;
	return (status);
}

/*
 * Calling convention:
 * ap: const u_char *uaddr, socklen_t len, int af, struct hostent *hp,
 *     char *buf, size_t buflen, int ret_errno, int *h_errnop
 * retval: should be set to NULL or hp passed in
 */
static int
__nss_bsdcompat_gethostbyaddr_r(void *retval, void *mdata __unused, va_list ap)
{
	void		*addr;
	char		*buf;
	int		*h_errnop;
	struct hostent	*hp;
	struct hostent	**resultp;
	int		 af;
	size_t		 buflen;
	int		 len;
	int		 ret_errno;
	enum nss_status	 status;

	addr = va_arg(ap, void *);
	len = va_arg(ap, socklen_t);
	af = va_arg(ap, int);
	hp = va_arg(ap, struct hostent *);
	buf = va_arg(ap, char *);
	buflen = va_arg(ap, size_t);
	ret_errno = va_arg(ap, int);
	h_errnop = va_arg(ap, int *);
	resultp = (struct hostent **)retval;

	*resultp = NULL;
	status = _nss_mdns_gethostbyaddr_r(addr, len, af, hp, buf, buflen,
	    &ret_errno, h_errnop);

	status = __nss_compat_result(status, *h_errnop);
	if (status == NS_SUCCESS)
		*resultp = hp;
	return (status);
}

/*
 * Calling convention:
 * ap: const char *name, int af, struct hostent *hp, char *buf,
 *     size_t buflen, int ret_errno, int *h_errnop
 * retval is a struct hostent **result passed in by the libc client,
 * which is responsible for allocating storage.
 */
static int
__nss_bsdcompat_gethostbyname2_r(void *retval, void *mdata __unused,
    va_list ap)
{
	char		*buf;
	const char 	*name;
	int		*h_errnop;
	struct hostent	*hp;
	struct hostent	**resultp;
	int		 af;
	size_t		 buflen;
	int		 ret_errno;
	enum nss_status	 status;

	name = va_arg(ap, char *);
	af = va_arg(ap, int);
	hp = va_arg(ap, struct hostent *);
	buf = va_arg(ap, char *);
	buflen = va_arg(ap, size_t);
	ret_errno = va_arg(ap, int);
	h_errnop = va_arg(ap, int *);
	resultp = (struct hostent **)retval;

	*resultp = NULL;
	if (hp == NULL)
		return (NS_UNAVAIL);

	status = _nss_mdns_gethostbyname2_r(name, af, hp, buf, buflen,
	    &ret_errno, h_errnop);

	status = __nss_compat_result(status, *h_errnop);
	if (status == NS_SUCCESS)
		*resultp = hp;
	return (status);
}

/*
 * Used by getipnodebyaddr(3).
 *
 * Calling convention:
 * ap: struct in[6]_addr *src, size_t len, int af, int *errp
 * retval: pointer to a pointer to an uninitialized struct hostent,
 * in which should be returned a single pointer to on-heap storage.
 *
 * This function is responsible for allocating on-heap storage.
 * The caller is responsible for calling freehostent() on the returned
 * storage.
 */
static int
__nss_bsdcompat_ghbyaddr(void *retval, void *mdata __unused, va_list ap)
{
	char		*buffer;
	void 		*bufp;
	int		*errp;
	struct hostent	*hp;
	struct hostent	**resultp;
	void		*src;
	int		 af;
	size_t		 buflen = 1024;
	size_t		 len;
	int		 h_errnop;
	enum nss_status	 status;

	src = va_arg(ap, void *);
	len = va_arg(ap, size_t);
	af = va_arg(ap, int);
	errp = va_arg(ap, int *);
	resultp = (struct hostent **)retval;

	_NSS_UTRACE("__nss_bsdcompat_ghbyaddr: called");

	bufp = malloc((sizeof(struct hostent) + buflen));
	if (bufp == NULL) {
		*resultp = NULL;
		return (NS_UNAVAIL);
	}
	hp = (struct hostent *)bufp;
	buffer = (char *)(hp + 1);

	status = _nss_mdns_gethostbyaddr_r(src, len, af, hp, buffer,
	    buflen, errp, &h_errnop);

	status = __nss_compat_result(status, *errp);
	if (status != NS_SUCCESS) {
		free(bufp);
		hp = NULL;
	}
	*resultp = hp;
	return (status);
}

/*
 * Used by getipnodebyname(3).
 *
 * Calling convention:
 * ap: const char *name, int af, int *errp
 * retval: pointer to a pointer to an uninitialized struct hostent.
 *
 * This function is responsible for allocating on-heap storage.
 * The caller is responsible for calling freehostent() on the returned
 * storage.
 */
static int
__nss_bsdcompat_ghbyname(void *retval, void *mdata __unused, va_list ap)
{
	char		*buffer;
	void 		*bufp;
	int		*errp;
	struct hostent	*hp;
	struct hostent	**resultp;
	char		*name;
	int		 af;
	size_t		 buflen = 1024;
	int		 h_errnop;
	enum nss_status	 status;

	name = va_arg(ap, char *);
	af = va_arg(ap, int);
	errp = va_arg(ap, int *);
	resultp = (struct hostent **)retval;

	bufp = malloc((sizeof(struct hostent) + buflen));
	if (bufp == NULL) {
		*resultp = NULL;
		return (NS_UNAVAIL);
	}
	hp = (struct hostent *)bufp;
	buffer = (char *)(hp + 1);

	status = _nss_mdns_gethostbyname_r(name, hp, buffer, buflen, errp,
	    &h_errnop);

	status = __nss_compat_result(status, *errp);
	if (status != NS_SUCCESS) {
		free(bufp);
		hp = NULL;
	}
	*resultp = hp;
	return (status);
}

#endif /* !NO_BUILD_BSD_NSS */
