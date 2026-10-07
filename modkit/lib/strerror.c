/* strerror.c - the texts of the error numbers of errno.h. */
#include <stddef.h>
#include <string.h>
#include <errno.h>
char *strerror (int e)
{
  static char buf[32];
  switch (e)
    {
    case 0: return "No error";
    case EPERM: return "Operation not permitted";
    case ENOENT: return "No such file or directory";
    case EINTR: return "Interrupted";
    case EIO: return "Input/output error";
    case E2BIG: return "Argument list too long";
    case EBADF: return "Bad file descriptor";
    case ENOMEM: return "Cannot allocate memory";
    case EACCES: return "Permission denied";
    case EBUSY: return "Device or resource busy";
    case EEXIST: return "File exists";
    case ENOTDIR: return "Not a directory";
    case EISDIR: return "Is a directory";
    case EINVAL: return "Invalid argument";
    case EMFILE: return "Too many open files";
    case ENOSPC: return "No space left on device";
    case ESPIPE: return "Illegal seek";
    case EROFS: return "Read-only file system";
    case EPIPE: return "Broken pipe";
    case EDOM: return "Numerical argument out of domain";
    case ERANGE: return "Numerical result out of range";
    case EAGAIN: return "Resource temporarily unavailable";
    case EINPROGRESS: return "Operation now in progress";
    case ECONNRESET: return "Connection reset by peer";
    case ENOTCONN: return "Socket is not connected";
    }
  char num[12], *p = num + sizeof num;
  unsigned u = e < 0 ? -(unsigned) e : (unsigned) e;
  *--p = 0;
  do *--p = (char) ('0' + u % 10); while (u /= 10);
  if (e < 0) *--p = '-';
  strcpy (buf, "Unknown error ");
  strcat (buf, p);
  return buf;
}

