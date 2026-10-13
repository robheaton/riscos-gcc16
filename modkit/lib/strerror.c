/* strerror.c - the texts of the error numbers of errno.h (the wording of glibc's, where the number means the same), sys_errlist and sys_nerr, and the POSIX strerror_r. */
#include <stddef.h>
#include <string.h>
#include <errno.h>

int sys_nerr = 99;
const char *sys_errlist[99] =
{
  [0] = "No error",
  [EPERM] = "Operation not permitted", [ENOENT] = "No such file or directory", [ESRCH] = "No such process", [EINTR] = "Interrupted", [EIO] = "Input/output error",
  [ENXIO] = "No such device or address", [E2BIG] = "Argument list too long", [ENOEXEC] = "Exec format error", [EBADF] = "Bad file descriptor", [ECHILD] = "No child processes",
  [EDEADLK] = "Resource deadlock avoided", [ENOMEM] = "Cannot allocate memory", [EACCES] = "Permission denied", [EFAULT] = "Bad address", [ENOTBLK] = "Block device required",
  [EBUSY] = "Device or resource busy", [EEXIST] = "File exists", [EXDEV] = "Invalid cross-device link", [ENODEV] = "No such device", [ENOTDIR] = "Not a directory",
  [EISDIR] = "Is a directory", [EINVAL] = "Invalid argument", [ENFILE] = "Too many open files in system", [EMFILE] = "Too many open files", [ENOTTY] = "Inappropriate ioctl for device",
  [ETXTBSY] = "Text file busy", [EFBIG] = "File too large", [ENOSPC] = "No space left on device", [ESPIPE] = "Illegal seek", [EROFS] = "Read-only file system",
  [EMLINK] = "Too many links", [EPIPE] = "Broken pipe", [EDOM] = "Numerical argument out of domain", [ERANGE] = "Numerical result out of range",
  [EAGAIN] = "Resource temporarily unavailable", [EINPROGRESS] = "Operation now in progress", [EALREADY] = "Operation already in progress", [ENOTSOCK] = "Socket operation on non-socket",
  [EDESTADDRREQ] = "Destination address required", [EMSGSIZE] = "Message too long", [EPROTOTYPE] = "Protocol wrong type for socket", [ENOPROTOOPT] = "Protocol not available",
  [EPROTONOSUPPORT] = "Protocol not supported", [ESOCKTNOSUPPORT] = "Socket type not supported", [EOPNOTSUPP] = "Operation not supported", [EPFNOSUPPORT] = "Protocol family not supported",
  [EAFNOSUPPORT] = "Address family not supported by protocol", [EADDRINUSE] = "Address already in use", [EADDRNOTAVAIL] = "Cannot assign requested address", [ENETDOWN] = "Network is down",
  [ENETUNREACH] = "Network is unreachable", [ENETRESET] = "Network dropped connection on reset", [ECONNABORTED] = "Software caused connection abort", [ECONNRESET] = "Connection reset by peer",
  [ENOBUFS] = "No buffer space available", [EISCONN] = "Transport endpoint is already connected", [ENOTCONN] = "Transport endpoint is not connected",
  [ESHUTDOWN] = "Cannot send after transport endpoint shutdown", [ETOOMANYREFS] = "Too many references: cannot splice", [ETIMEDOUT] = "Connection timed out", [ECONNREFUSED] = "Connection refused",
  [ELOOP] = "Too many levels of symbolic links", [ENAMETOOLONG] = "File name too long", [EHOSTDOWN] = "Host is down", [EHOSTUNREACH] = "No route to host", [ENOTEMPTY] = "Directory not empty",
  [EPROCLIM] = "Too many processes", [EUSERS] = "Too many users", [EDQUOT] = "Disk quota exceeded", [ESTALE] = "Stale file handle", [EREMOTE] = "Object is remote",
  [ENOSTR] = "Device not a stream", [ETIME] = "Timer expired", [ENOSR] = "Out of streams resources", [ENOMSG] = "No message of desired type", [EBADMSG] = "Bad message",
  [EIDRM] = "Identifier removed", [ENOTSUP] = "Not supported", [ENOLCK] = "No locks available",
  [80] = "Unknown error 80", [81] = "Unknown error 81",
  [ELIBVER] = "Shared library version mismatch", [ELIBACC] = "Can not access a needed shared library", [ELIBLIM] = "Attempting to link in too many shared libraries",
  [ELIBNOENT] = "Shared library not found", [ELIBNOEXEC] = "Cannot exec a shared library directly", [ENOSYS] = "Function not implemented", [EOPSYS] = "Operating system error",
  [ESIG] = "Signal not supported", [EILSEQ] = "Invalid or incomplete multibyte or wide character", [EOVERFLOW] = "Value too large for defined data type",
  [EPROTO] = "Protocol error", [ENODATA] = "No data available", [ENOLINK] = "Link has been severed", [EMULTIHOP] = "Multihop attempted",
  [ECANCELED] = "Operation canceled", [ENOTRECOVERABLE] = "State not recoverable", [EOWNERDEAD] = "Owner died",
};

static void unknown (char *buf, int e)
{
  char num[12], *p = num + sizeof num;
  unsigned u = e < 0 ? -(unsigned) e : (unsigned) e;
  *--p = 0;
  do *--p = (char) ('0' + u % 10); while (u /= 10);
  if (e < 0) *--p = '-';
  strcpy (buf, "Unknown error ");
  strcat (buf, p);
}
char *strerror (int e)
{
  static char buf[32];
  if (e >= 0 && e < sys_nerr && sys_errlist[e]) return (char *) sys_errlist[e];
  unknown (buf, e);
  return buf;
}
int strerror_r (int e, char *buf, size_t n)
{
  char tmp[32];
  const char *text;
  int known = e >= 0 && e < sys_nerr && sys_errlist[e];
  size_t len;
  if (known) text = sys_errlist[e];
  else { unknown (tmp, e); text = tmp; }
  len = strlen (text);
  if (n) { size_t c = len < n ? len : n - 1; memcpy (buf, text, c); buf[c] = 0; }
  if (!known) return EINVAL;
  return len >= n ? ERANGE : 0;
}
