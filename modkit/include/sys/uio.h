/* sys/uio.h - the scatter/gather vector of socketreadv, socketwritev, sendmsg and recvmsg. */
#ifndef _SYS_UIO_H
#define _SYS_UIO_H
#include <sys/types.h>
struct iovec { void *iov_base; size_t iov_len; };
#ifdef __cplusplus
extern "C" {
#endif
extern ssize_t readv (int fd, const struct iovec *iov, int count);         /* on a descriptor of open () or a socket */
extern ssize_t writev (int fd, const struct iovec *iov, int count);
#ifdef __cplusplus
}
#endif
#endif
