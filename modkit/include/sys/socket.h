/* sys/socket.h - the BSD socket calls on the RISC OS Internet module, for a module (no UnixLib, no Shared C Library): each call is a Socket_* SWI (the XSocket_* SWIs: an error is the
   return value -1 with errno set, as in the socklib of the RISC OS sources).  A socket is a small integer that is not a file handle: close it with socketclose (), read and write it with socketread ()
   and socketwrite () (or recv and send): the names of the Shared C Library's socket calls, which GCCSDK 4.7.4's modules used.  The numbers are the BSD ones that the Internet module expects: the
   sockaddr has a length byte first (sa_len). */
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include <sys/types.h>
#include <sys/uio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int socklen_t;
typedef unsigned char sa_family_t;
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5
#define SO_DEBUG 0x0001
#define SO_ACCEPTCONN 0x0002
#define SO_REUSEADDR 0x0004
#define SO_KEEPALIVE 0x0008
#define SO_DONTROUTE 0x0010
#define SO_BROADCAST 0x0020
#define SO_USELOOPBACK 0x0040
#define SO_LINGER 0x0080
#define SO_OOBINLINE 0x0100
#define SO_REUSEPORT 0x0200
#define SO_SNDBUF 0x1001
#define SO_RCVBUF 0x1002
#define SO_SNDLOWAT 0x1003
#define SO_RCVLOWAT 0x1004
#define SO_SNDTIMEO 0x1005
#define SO_RCVTIMEO 0x1006
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008
#define SOL_SOCKET 0xffff
struct linger { int l_onoff; int l_linger; };
#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define AF_INET 2
#define AF_INET6 28
#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX AF_UNIX
#define PF_LOCAL AF_LOCAL
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define SOMAXCONN 128
#define MSG_OOB 0x1
#define MSG_PEEK 0x2
#define MSG_DONTROUTE 0x4
#define MSG_EOR 0x8
#define MSG_TRUNC 0x10
#define MSG_CTRUNC 0x20
#define MSG_WAITALL 0x40
#define MSG_DONTWAIT 0x80
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
struct sockaddr { unsigned char sa_len; unsigned char sa_family; char sa_data[14]; };
struct sockaddr_storage { unsigned char ss_len; unsigned char ss_family; char __ss_pad[126]; };
struct msghdr { void *msg_name; socklen_t msg_namelen; struct iovec *msg_iov; int msg_iovlen; void *msg_control; socklen_t msg_controllen; int msg_flags; };

extern int socket (int domain, int type, int protocol);
extern int bind (int s, const struct sockaddr *addr, socklen_t len);
extern int listen (int s, int backlog);
extern int accept (int s, struct sockaddr *addr, socklen_t *len);
extern int connect (int s, const struct sockaddr *addr, socklen_t len);
extern ssize_t send (int s, const void *buf, size_t len, int flags);
extern ssize_t sendto (int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
extern ssize_t sendmsg (int s, const struct msghdr *msg, int flags);
extern ssize_t recv (int s, void *buf, size_t len, int flags);
extern ssize_t recvfrom (int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
extern ssize_t recvmsg (int s, struct msghdr *msg, int flags);
extern int shutdown (int s, int how);
extern int getsockname (int s, struct sockaddr *addr, socklen_t *len);
extern int getpeername (int s, struct sockaddr *addr, socklen_t *len);
extern int setsockopt (int s, int level, int name, const void *value, socklen_t len);
extern int getsockopt (int s, int level, int name, void *value, socklen_t *len);
/* the Shared C Library's names (the sockets are not files) */
extern int socketclose (int s);
extern int socketioctl (int s, unsigned long cmd, void *arg);
extern ssize_t socketread (int s, void *buf, size_t len);
extern ssize_t socketwrite (int s, const void *buf, size_t len);
extern ssize_t socketreadv (int s, const struct iovec *iov, int iovcnt);
extern ssize_t socketwritev (int s, const struct iovec *iov, int iovcnt);
extern int socketstat (int s, void *stat);
extern int getstablesize (void);
/* the text of the last error of the Internet module (the error block itself is _inet_error) */
extern char *_inet_err (void);
#ifdef __cplusplus
}
#endif
#endif
