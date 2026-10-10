/* netdb.h - host names (the Resolver module's SWI Resolver_GetHostByName), services (the file InetDBase:Services, then a small built-in list) and protocols (a built-in list). */
#ifndef _NETDB_H
#define _NETDB_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef __cplusplus
extern "C" {
#endif
struct hostent { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
#define h_addr h_addr_list[0]
struct servent { char *s_name; char **s_aliases; int s_port; char *s_proto; };       /* s_port is in network order */
struct protoent { char *p_name; char **p_aliases; int p_proto; };
extern int h_errno;
#define HOST_NOT_FOUND 1
#define TRY_AGAIN 2
#define NO_RECOVERY 3
#define NO_DATA 4
#define NO_ADDRESS NO_DATA
/* These return pointers to static data that the next call overwrites. */
extern struct hostent *gethostbyname (const char *name);                  /* a dotted address is converted, any other name is looked up by the Resolver; NULL with h_errno set */
extern struct hostent *gethostbyaddr (const void *addr, socklen_t len, int type);       /* not implemented: NULL with h_errno NO_RECOVERY */
extern struct servent *getservbyname (const char *name, const char *proto);
extern struct servent *getservbyport (int port, const char *proto);       /* PORT in network order */
extern struct protoent *getprotobyname (const char *name);
extern struct protoent *getprotobynumber (int proto);
extern const char *hstrerror (int err);
extern void herror (const char *s);
#ifdef __cplusplus
}
#endif
#endif
