/* sys/socket.h - the types and calls of BSD sockets, so that programs that mention them compile (GCC's diagnostics, make, ...); every call fails with ENOSYS until the 64-bit Internet module has an interface the SDK
   can use.  The SDK of RISC OS 64. */
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
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_BROADCAST 0x0020
#define SO_ERROR 0x1007
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define SOMAXCONN 128
#define MSG_PEEK 0x2
#define MSG_WAITALL 0x40
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage { sa_family_t ss_family; char __ss_pad[126]; };
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
