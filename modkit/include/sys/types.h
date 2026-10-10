/* sys/types.h - the BSD types that the socket headers use (the names of the RISC OS TCP/IP libraries: u_char ... fd_set). */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef long long quad_t;
typedef unsigned long long u_quad_t;
typedef char *caddr_t;
typedef int ssize_t;
typedef long off_t;
typedef int pid_t;
typedef unsigned int mode_t;
#define NBBY 8
/* select () sets: bit n is socket n; as in the Internet module, 256 sockets */
#ifndef FD_SETSIZE
#define FD_SETSIZE 256
#endif
typedef long fd_mask;
#define NFDBITS (sizeof (fd_mask) * NBBY)
typedef struct fd_set { fd_mask fds_bits[(FD_SETSIZE + NFDBITS - 1) / NFDBITS]; } fd_set;
#define FD_SET(n, p) ((p)->fds_bits[(n) / NFDBITS] |= (1UL << ((n) % NFDBITS)))
#define FD_CLR(n, p) ((p)->fds_bits[(n) / NFDBITS] &= ~(1UL << ((n) % NFDBITS)))
#define FD_ISSET(n, p) (((p)->fds_bits[(n) / NFDBITS] & (1UL << ((n) % NFDBITS))) != 0)
#define FD_COPY(f, t) (*(t) = *(f))
#define FD_ZERO(p) memset ((p), 0, sizeof (*(p)))
#ifdef __cplusplus
}
#endif
#endif
