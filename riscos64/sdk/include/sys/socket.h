/* sys/socket.h - BSD sockets on the Internet module of RISC OS (crt/posix/sockets.c): a socket is a descriptor (2000 and up) for read, write, close, fcntl, ioctl, select and poll.  AF_INET only (stream and datagram
   sockets); AF_UNIX and AF_INET6 fail with EAFNOSUPPORT.  The SDK of RISC OS 64. */
#ifndef _RISCOS64_SYS_SOCKET_H
#define _RISCOS64_SYS_SOCKET_H
#include <sys/types.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;
#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define AF_INET 2
#define AF_INET6 10
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_LOCAL AF_UNIX
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOL_SOCKET 0xffff
#define SO_DEBUG 0x0001
#define SO_ACCEPTCONN 0x0002
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_BROADCAST 0x0020
#define SO_LINGER 0x0080
#define SO_OOBINLINE 0x0100
#define SO_SNDBUF 0x1001
#define SO_RCVBUF 0x1002
#define SO_SNDTIMEO 0x1005
#define SO_RCVTIMEO 0x1006
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define SOMAXCONN 128
#define MSG_OOB 0x1
#define MSG_PEEK 0x2
#define MSG_DONTROUTE 0x4
#define MSG_NOSIGNAL 0
#define MSG_WAITALL 0x40
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; char __ss_pad[126]; } __attribute__ ((aligned (8)));
struct linger { int l_onoff; int l_linger; };
#define SOCK_NONBLOCK 0x4000
#define SOCK_CLOEXEC 0x2000
int socket (int, int, int);
int socketpair (int, int, int, int[2]);
int bind (int, const struct sockaddr *, socklen_t);
int listen (int, int);
int accept (int, struct sockaddr *, socklen_t *);
int connect (int, const struct sockaddr *, socklen_t);
ssize_t send (int, const void *, size_t, int);
ssize_t recv (int, void *, size_t, int);
ssize_t sendto (int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t recvfrom (int, void *, size_t, int, struct sockaddr *, socklen_t *);
int shutdown (int, int);
int setsockopt (int, int, int, const void *, socklen_t);
int getsockopt (int, int, int, void *, socklen_t *);
int getsockname (int, struct sockaddr *, socklen_t *);
int getpeername (int, struct sockaddr *, socklen_t *);
#ifdef __cplusplus
}
#endif
#endif
