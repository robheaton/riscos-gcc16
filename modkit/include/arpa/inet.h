/* arpa/inet.h - conversions of Internet addresses (IPv4). */
#ifndef _ARPA_INET_H
#define _ARPA_INET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef __cplusplus
extern "C" {
#endif
/* a.b.c.d (also a.b.c, a.b, a; decimal, octal 0n and hexadecimal 0xn) -> the address in network order; inet_addr gives INADDR_NONE (0xffffffff) for a bad text, inet_aton 0 */
extern in_addr_t inet_addr (const char *cp);
extern int inet_aton (const char *cp, struct in_addr *addr);
extern char *inet_ntoa (struct in_addr in);                              /* the text is in a buffer that the next call overwrites */
extern const char *inet_ntop (int af, const void *src, char *dst, socklen_t size);
extern int inet_pton (int af, const char *src, void *dst);               /* AF_INET only; 1: done, 0: bad text, -1: bad family */
extern in_addr_t inet_network (const char *cp);
extern struct in_addr inet_makeaddr (in_addr_t net, in_addr_t host);
extern in_addr_t inet_lnaof (struct in_addr in);
extern in_addr_t inet_netof (struct in_addr in);
#ifdef __cplusplus
}
#endif
#endif
