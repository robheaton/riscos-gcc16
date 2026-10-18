/* netdb.h - names of hosts, services and protocols (crt/posix/netdb.c): numeric addresses, "localhost", a small table of services and protocols, and the Resolver module for other names.  The SDK of RISC OS 64. */
#ifndef _NETDB_H
#define _NETDB_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct hostent { char *h_name; char **h_aliases; int h_addrtype; int h_length; char **h_addr_list; };
#define h_addr h_addr_list[0]
struct servent { char *s_name; char **s_aliases; int s_port; char *s_proto; };
struct protoent { char *p_name; char **p_aliases; int p_proto; };
struct addrinfo { int ai_flags; int ai_family; int ai_socktype; int ai_protocol; socklen_t ai_addrlen; struct sockaddr *ai_addr; char *ai_canonname; struct addrinfo *ai_next; };
#define AI_PASSIVE 1
#define AI_CANONNAME 2
#define AI_NUMERICHOST 4
#define AI_NUMERICSERV 8
#define AI_ADDRCONFIG 0x20
#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NAMEREQD 4
#define NI_DGRAM 16
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define EAI_BADFLAGS (-1)
#define EAI_NONAME (-2)
#define EAI_AGAIN (-3)
#define EAI_FAIL (-4)
#define EAI_FAMILY (-6)
#define EAI_SOCKTYPE (-7)
#define EAI_SERVICE (-8)
#define EAI_MEMORY (-10)
#define EAI_SYSTEM (-11)
#define EAI_OVERFLOW (-12)
#define HOST_NOT_FOUND 1
#define TRY_AGAIN 2
#define NO_RECOVERY 3
#define NO_DATA 4
extern int h_errno;
struct hostent *gethostbyname (const char *);
struct hostent *gethostbyaddr (const void *, socklen_t, int);
struct servent *getservbyname (const char *, const char *);
struct servent *getservbyport (int, const char *);
struct protoent *getprotobyname (const char *);
struct protoent *getprotobynumber (int);
int getaddrinfo (const char *, const char *, const struct addrinfo *, struct addrinfo **);
void freeaddrinfo (struct addrinfo *);
const char *gai_strerror (int);
int getnameinfo (const struct sockaddr *, socklen_t, char *, socklen_t, char *, socklen_t, int);
const char *hstrerror (int);
#ifdef __cplusplus
}
#endif
#endif
