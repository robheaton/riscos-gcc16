/* netinet/in.h - Internet addresses (BSD layout: the Internet module's). */
#ifndef _NETINET_IN_H
#define _NETINET_IN_H
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned short in_port_t;
typedef unsigned int in_addr_t;
#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_IGMP 2
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_RAW 255
#define IPPORT_RESERVED 1024
#define IPPORT_USERRESERVED 5000
#define INADDR_ANY ((in_addr_t) 0x00000000)
#define INADDR_LOOPBACK ((in_addr_t) 0x7f000001)
#define INADDR_BROADCAST ((in_addr_t) 0xffffffff)
#define INADDR_NONE 0xffffffff
struct in_addr { in_addr_t s_addr; };
struct sockaddr_in { unsigned char sin_len; unsigned char sin_family; in_port_t sin_port; struct in_addr sin_addr; char sin_zero[8]; };
#define IN_CLASSA(i) (((unsigned int) (i) & 0x80000000) == 0)
#define IN_CLASSB(i) (((unsigned int) (i) & 0xc0000000) == 0x80000000)
#define IN_CLASSC(i) (((unsigned int) (i) & 0xe0000000) == 0xc0000000)
#define IN_MULTICAST(i) (((unsigned int) (i) & 0xf0000000) == 0xe0000000)
/* the host is little endian, the network big endian */
#define ntohl(x) ((unsigned int) __builtin_bswap32 ((unsigned int) (x)))
#define htonl(x) ((unsigned int) __builtin_bswap32 ((unsigned int) (x)))
#define ntohs(x) ((unsigned short) __builtin_bswap16 ((unsigned short) (x)))
#define htons(x) ((unsigned short) __builtin_bswap16 ((unsigned short) (x)))
#ifdef __cplusplus
}
#endif
#endif
