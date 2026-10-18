/* arpa/inet.h - byte order and the text form of addresses.  The SDK of RISC OS 64. */
#ifndef __ARPA_INET_H__
#define __ARPA_INET_H__
#include <endian.h>
#include <stdint.h>
#include <netinet/in.h>
#ifdef __cplusplus
extern "C" {
#endif
int inet_pton (int, const char *, void *);
const char *inet_ntop (int, const void *, char *, unsigned);
int inet_aton (const char *, struct in_addr *);
in_addr_t inet_addr (const char *);
char *inet_ntoa (struct in_addr);
#ifdef __cplusplus
}
#endif
#endif
