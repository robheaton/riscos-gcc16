/* sys/uio.h - scatter/gather I/O (SDK of RISC OS 64: readv and writev are loops over read and write) */
#ifndef _SYS_UIO_H
#define _SYS_UIO_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct iovec { void *iov_base; size_t iov_len; };
#define UIO_MAXIOV 1024
ssize_t readv (int, const struct iovec *, int);
ssize_t writev (int, const struct iovec *, int);
#ifdef __cplusplus
}
#endif
#endif
