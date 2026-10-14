/* fdio.c - the descriptor functions on files: open, creat, close, read, write, readv, writev, lseek, stat, lstat, fstat, access, dup, dup2, fsync, fdatasync, ftruncate, truncate, isatty, ioctl, unlink, utime, utimes, fdopen and fileno, and the *64 names.
   A descriptor is a FILE of the stdio code of the kit (fcore.c): 0, 1 and 2 are stdin, stdout and stderr; open () makes a stream that is read and written with no buffer (the OS calls are OS_GBPB and
   OS_Args, as fread and fwrite make them), and fopen / fileno give a descriptor to a stream that has none.  The same object is both: fdopen returns the FILE of the descriptor, and fclose closes
   the descriptor.  Files, the three standard streams and sockets: socket () and accept () (sockets.c) give a descriptor of this table, of kind K_SOCKET (the number of the Internet module's own socket is inside), and close,
   read, write, readv, writev, ioctl, fcntl, fstat, dup and select (sockets.c) work on it as they do in UnixLib.  RISC OS has no permissions, owners or links. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <kernel.h>
#include "fileimpl.h"

#define FDMAX 64                                  /* descriptors 0 to FDMAX - 1 (0, 1 and 2 are the standard streams) */
static FILE *fdtab[FDMAX];

static void release (FILE *f)                     /* (the hook of fcore.c: the stream is closed: every descriptor that dup () made of it goes too) */
{
  int i;
  for (i = 3; i < FDMAX; i++) if (fdtab[i] == f) fdtab[i] = 0;
  f->fd = -1;
}
static int alloc_fd (FILE *f)
{
  int i;
  for (i = 3; i < FDMAX; i++)
    if (!fdtab[i]) { fdtab[i] = f; f->fd = i; __modlib_fd_hook = release; return i; }
  errno = EMFILE;
  return -1;
}
/* the stream of descriptor FD, or 0 with errno EBADF */
static FILE *get (int fd)
{
  FILE *f = fd == 0 ? &__modlib_stdin : fd == 1 ? &__modlib_stdout : fd == 2 ? &__modlib_stderr : fd > 2 && fd < FDMAX ? fdtab[fd] : 0;
  if (!f || f->kind == K_CLOSED) { errno = EBADF; return 0; }
  return f;
}

/* ---- sockets as descriptors (see fileimpl.h) */
int (*__modlib_sock_op) (int op, int handle, void *buf, unsigned long n, void *arg);

int __modlib_socket_new (int handle)
{
  FILE *f = calloc (1, sizeof *f);
  int fd;
  if (!f) { errno = ENOMEM; return -1; }
  f->kind = K_SOCKET; f->handle = (unsigned) handle; f->mode = M_READ | M_WRITE; f->unget = -1; f->fd = -1;
  fd = alloc_fd (f);
  if (fd < 0) free (f);
  return fd;
}
/* the end of a program and __modlib_closeall (): every socket descriptor is closed (the Internet module's socket goes with the last descriptor of it); the number of descriptors */
int __modlib_sockets_closeall (void)
{
  int fd, n = 0;
  for (fd = 3; fd < FDMAX; fd++)
    if (fdtab[fd] && fdtab[fd]->kind == K_SOCKET) { close (fd); n++; }
  return n;
}
int __modlib_socket_handle (int fd)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (f->kind != K_SOCKET) { errno = ENOTSOCK; return -1; }
  return (int) f->handle;
}
int __modlib_fd_info (int fd, int *handle)
{
  FILE *f = get (fd);
  if (!f) return 0;
  if (f->kind != K_SOCKET) return 2;
  *handle = (int) f->handle;
  return 1;
}

int fileno (FILE *f)
{
  if (!f || f->kind == K_CLOSED) { errno = EBADF; return -1; }
  if (f == &__modlib_stdin) return 0;
  if (f == &__modlib_stdout) return 1;
  if (f == &__modlib_stderr) return 2;
  if (f->fd >= 0) return f->fd;
  return alloc_fd (f);
}
FILE *fdopen (int fd, const char *mode)
{
  FILE *f = get (fd);
  int c = mode ? mode[0] : 0, plus = 0;
  const char *p;
  if (!f) return 0;
  if (f->kind == K_SOCKET) { errno = ENOTSUP; return 0; }         /* (stdio on a socket is not there: use recv / send or read / write) */
  if (c != 'r' && c != 'w' && c != 'a') { errno = EINVAL; return 0; }
  for (p = mode + 1; *p; p++) if (*p == '+') plus = 1;
  if (((c == 'r' || plus) && !(f->mode & M_READ)) || ((c != 'r' || plus) && !(f->mode & M_WRITE))) { errno = EINVAL; return 0; }          /* the mode must fit how the descriptor was opened */
  return f;
}

/* set the length of the file of F: shorter with OS_Args 3, longer with zeros written at the end (what OS_Args 3 puts there is not defined) */
static int set_extent (FILE *f, long len)
{
  long ext;
  if (__modlib_sync (f) || __modlib_file_len (f, &ext)) return -1;
  if (len < ext)
    {
      if (_kernel_osargs (3, f->handle, (int) len) == _kernel_ERROR) { __modlib_os_errno (); return -1; }
      f->bits &= ~B_PTR;
    }
  else if (len > ext)
    {
      unsigned char zero[256];
      long save = f->fpos;
      memset (zero, 0, sizeof zero);
      f->fpos = ext; f->bits &= ~B_PTR;
      while (f->fpos < len)
	{
	  long left = len - f->fpos;
	  if (__modlib_rawwrite (f, zero, (unsigned) (left < (long) sizeof zero ? left : (long) sizeof zero))) { f->fpos = save; f->bits &= ~B_PTR; return -1; }
	}
      f->fpos = save; f->bits &= ~B_PTR;
    }
  return 0;
}

int open (const char *path, int flags, ...)
{
  int acc = flags & O_ACCMODE, create = (flags & O_CREAT) != 0, trunc = (flags & O_TRUNC) != 0, fd;
  FILE *f;
  if (!path) { errno = EFAULT; return -1; }
  if (acc == O_ACCMODE) { errno = EINVAL; return -1; }
  f = __modlib_open_gen (0, path, acc != O_WRONLY, acc != O_RDONLY, create, trunc && create, create && (flags & O_EXCL) != 0, (flags & O_APPEND) != 0, 0);
  if (!f) return -1;
  if (trunc && !create && acc != O_RDONLY && set_extent (f, 0)) { fclose (f); return -1; }               /* O_TRUNC of a file that must be there: emptied after it is opened */
  fd = alloc_fd (f);
  if (fd < 0) { fclose (f); return -1; }
  return fd;
}
int open64 (const char *path, int flags, ...) { return open (path, flags | O_LARGEFILE); }
int creat (const char *path, mode_t mode) { (void) mode; return open (path, O_WRONLY | O_CREAT | O_TRUNC); }
int creat64 (const char *path, mode_t mode) { return creat (path, mode); }

static int is_std (FILE *f) { return f == &__modlib_stdin || f == &__modlib_stdout || f == &__modlib_stderr; }
static int refs (FILE *f)                         /* how many descriptors of 3 and up are this stream */
{
  int i, n = 0;
  for (i = 3; i < FDMAX; i++) if (fdtab[i] == f) n++;
  return n;
}
int close (int fd)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (f->kind == K_SOCKET)                                       /* the Internet module's socket is closed with the last descriptor of it (dup () makes more) */
    {
      int last = refs (f) <= 1, r = 0, i;
      if (last) r = __modlib_sock_op (SOP_CLOSE, (int) f->handle, 0, 0, 0);
      fdtab[fd] = 0;
      if (last) free (f);
      else if (f->fd == fd) { f->fd = -1; for (i = 3; i < FDMAX; i++) if (fdtab[i] == f) { f->fd = i; break; } }
      return r;
    }
  if (fd >= 3 && (is_std (f) || refs (f) > 1))                  /* a copy that dup () made: the descriptor goes, the stream stays while it has another (a copy of a standard stream is never closed) */
    {
      int i;
      fdtab[fd] = 0;
      if (f->fd == fd) { f->fd = -1; for (i = 3; i < FDMAX; i++) if (fdtab[i] == f) { f->fd = i; break; } }
      return 0;
    }
  return fclose (f) ? -1 : 0;                                    /* (fclose frees the descriptor through the hook of fcore.c) */
}
int dup (int fd)
{
  FILE *f = get (fd);
  int i;
  if (!f) return -1;
  for (i = 3; i < FDMAX; i++)
    if (!fdtab[i])
      {
	fdtab[i] = f;
	if (f->fd < 0 && !is_std (f)) f->fd = i;
	__modlib_fd_hook = release;
	return i;
      }
  errno = EMFILE;
  return -1;
}
int dup2 (int fd, int fd2)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (fd2 < 0 || fd2 >= FDMAX) { errno = EBADF; return -1; }
  if (fd2 == fd) return fd2;
  if (fd2 < 3) { errno = ENOSYS; return -1; }                    /* the standard streams cannot be pointed somewhere else */
  if (fdtab[fd2]) close (fd2);
  fdtab[fd2] = f;
  if (f->fd < 0 && !is_std (f)) f->fd = fd2;
  __modlib_fd_hook = release;
  return fd2;
}
/* write what is buffered, then OS_Args 255: the filing system writes the file's data to the medium */
int fsync (int fd)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (f->kind == K_FILE)
    {
      if (fflush (f) || __modlib_sync (f)) return -1;
      if (_kernel_osargs (255, f->handle, 0) == _kernel_ERROR) { __modlib_os_errno (); return -1; }
      return 0;
    }
  if (f->kind == K_SCREEN) return fflush (f) ? -1 : 0;
  errno = EINVAL;                                               /* the keyboard: not something that can be synchronised */
  return -1;
}
int fdatasync (int fd) { return fsync (fd); }

ssize_t read (int fd, void *buf, size_t n)
{
  FILE *f = get (fd);
  int got;
  if (!f) return -1;
  if (n > (size_t) INT_MAX) { errno = EINVAL; return -1; }
  if (f->kind == K_SOCKET) return n ? __modlib_sock_op (SOP_READ, (int) f->handle, buf, n, 0) : 0;
  if (f->kind == K_KEYBOARD)                                      /* the keyboard gives a line at a time */
    {
      char *p = buf;
      size_t k = 0;
      int c;
      while (k < n && (c = fgetc (f)) != EOF) { p[k++] = (char) c; if (c == '\n') break; }
      if (!k && ferror (f)) return -1;
      return (ssize_t) k;
    }
  if (f->kind != K_FILE || !(f->mode & M_READ)) { errno = EBADF; return -1; }
  if (!n) return 0;
  if (__modlib_sync (f)) return -1;
  got = __modlib_rawread (f, buf, (unsigned) n);
  if (got < 0) return -1;
  f->fpos += got;
  return got;
}
ssize_t write (int fd, const void *buf, size_t n)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (n > (size_t) INT_MAX) { errno = EINVAL; return -1; }
  if (f->kind == K_SOCKET) return n ? __modlib_sock_op (SOP_WRITE, (int) f->handle, (void *) buf, n, 0) : 0;
  if (f->kind == K_SCREEN)
    {
      if (fwrite (buf, 1, n, f) != n) return -1;
      return (ssize_t) n;
    }
  if (f->kind != K_FILE || !(f->mode & M_WRITE)) { errno = EBADF; return -1; }
  if (!n) return 0;
  if (__modlib_sync (f)) return -1;
  if (__modlib_rawwrite (f, buf, (unsigned) n)) return -1;
  return (ssize_t) n;
}
ssize_t readv (int fd, const struct iovec *iov, int count)
{
  ssize_t total = 0;
  int i;
  FILE *sf = get (fd);
  if (!sf) return -1;
  if (count <= 0 || count > 1024) { errno = EINVAL; return -1; }
  if (sf->kind == K_SOCKET) return __modlib_sock_op (SOP_READV, (int) sf->handle, (void *) iov, (unsigned long) count, 0);
  for (i = 0; i < count; i++)
    {
      ssize_t r;
      if (iov[i].iov_len > (size_t) (INT_MAX - total)) { errno = EINVAL; return -1; }
      r = read (fd, iov[i].iov_base, iov[i].iov_len);
      if (r < 0) return total ? total : -1;
      total += r;
      if ((size_t) r < iov[i].iov_len) break;
    }
  return total;
}
ssize_t writev (int fd, const struct iovec *iov, int count)
{
  ssize_t total = 0;
  int i;
  FILE *sf = get (fd);
  if (!sf) return -1;
  if (count <= 0 || count > 1024) { errno = EINVAL; return -1; }
  if (sf->kind == K_SOCKET) return __modlib_sock_op (SOP_WRITEV, (int) sf->handle, (void *) iov, (unsigned long) count, 0);
  for (i = 0; i < count; i++)
    if (iov[i].iov_len > (size_t) (INT_MAX - total)) { errno = EINVAL; return -1; }
    else total += (ssize_t) iov[i].iov_len;
  total = 0;
  for (i = 0; i < count; i++)
    {
      ssize_t r = write (fd, iov[i].iov_base, iov[i].iov_len);
      if (r < 0) return total ? total : -1;
      total += r;
    }
  return total;
}

off_t lseek (int fd, off_t offset, int whence)
{
  FILE *f = get (fd);
  long p;
  if (!f) return -1;
  if (f->kind != K_FILE) { errno = ESPIPE; return -1; }
  if (fseek (f, (long) offset, whence)) return -1;
  p = ftell (f);
  return p;
}
off64_t lseek64 (int fd, off64_t offset, int whence)
{
  if (offset > 0x7FFFFFFFLL || offset < -0x7FFFFFFFLL - 1) { errno = EOVERFLOW; return -1; }
  return lseek (fd, (off_t) offset, whence);
}

/* the RISC OS date stamp of a file as seconds since 1970 (0 when it has none): the load address is 0xFFFtttdd (type ttt, the top byte of the time dd), the execution address is the rest of the time,
   in centiseconds since 1900; divided in 16 bit steps, there is no 64 bit division in a module */
static long stamp_to_time (unsigned load, unsigned exec)
{
  unsigned hi, qh, acc, q1, q0;
  long long secs;
  if ((load & 0xFFF00000u) != 0xFFF00000u) return 0;
  hi = load & 0xFF;
  qh = hi / 100; acc = (hi % 100) * 65536 + (exec >> 16);
  q1 = acc / 100; acc = (acc % 100) * 65536 + (exec & 0xFFFF);
  q0 = acc / 100;
  secs = ((long long) qh << 32) + ((long long) q1 << 16) + q0 - 2208988800LL;
  return secs < LONG_MIN ? LONG_MIN : secs > LONG_MAX ? LONG_MAX : (long) secs;
}
/* the permission bits of RISC OS attributes (bit 0 read, bit 1 write) as UnixLib gives them, for a file of the load address LOAD: a read bit also makes an execute bit, except for Text (&FFF) and Data (&FFD) */
static mode_t file_mode (unsigned load, unsigned attr)
{
  mode_t m = ((attr & 1) ? 0444 : 0) | ((attr & 2) ? 0222 : 0);
  if (((load & 0xFFF00000u) != 0xFFF00000u) || (((load >> 8) & 0xFFF) != 0xFFF && ((load >> 8) & 0xFFF) != 0xFFD)) m |= (m & 0444) >> 2;
  return m;
}
/* stat of a path: OS_File 17 gives the object type (1 file, 2 directory, 3 image file, which is a directory here as it is in UnixLib), the date stamp, the length and the attributes */
static int stat_path (const char *path, struct stat64 *st)
{
  _kernel_osfile_block b = { 0, 0, 0, 0 };
  int t;
  if (!path) { errno = EFAULT; return -1; }
  t = _kernel_osfile (17, path, &b);
  if (t == _kernel_ERROR) { __modlib_os_errno (); return -1; }
  if (t == 0) { errno = ENOENT; return -1; }
  memset (st, 0, sizeof *st);
  st->st_blksize = 512;
  st->st_atime = st->st_mtime = st->st_ctime = stamp_to_time ((unsigned) b.load, (unsigned) b.exec);
  if (t == 1)
    {
      st->st_mode = S_IFREG | file_mode ((unsigned) b.load, (unsigned) b.end);
      st->st_nlink = 1;
      st->st_size = (unsigned) b.start;
      { long ext; if (__modlib_open_length (path, &ext)) st->st_size = ext; }                       /* (a file that this program has open: its real length) */
      st->st_blocks = (st->st_size + 511) / 512;
    }
  else
    {
      st->st_mode = S_IFDIR | S_IRWXU;                            /* (the attributes of a directory are not reliable: UnixLib gives the owner everything too) */
      st->st_nlink = 2;
      st->st_size = (unsigned) b.start > 32768 ? 0 : (unsigned) b.start;      /* (the size of a directory in an image file system is not either) */
      st->st_blocks = (st->st_size + 511) / 512;
    }
  return 0;
}
static int narrow_stat (const struct stat64 *s, struct stat *st)
{
  if (s->st_size > 0x7FFFFFFFLL) { errno = EOVERFLOW; return -1; }
  st->st_dev = s->st_dev; st->st_ino = s->st_ino; st->st_mode = s->st_mode; st->st_nlink = s->st_nlink; st->st_uid = s->st_uid; st->st_gid = s->st_gid; st->st_rdev = s->st_rdev;
  st->st_size = (off_t) s->st_size; st->st_atime = s->st_atime; st->st_mtime = s->st_mtime; st->st_ctime = s->st_ctime; st->st_blksize = s->st_blksize; st->st_blocks = s->st_blocks;
  return 0;
}
int stat64 (const char *path, struct stat64 *st) { return stat_path (path, st); }
int lstat64 (const char *path, struct stat64 *st) { return stat_path (path, st); }          /* (there are no links) */
int stat (const char *path, struct stat *st)
{
  struct stat64 s;
  if (stat_path (path, &s)) return -1;
  return narrow_stat (&s, st);
}
int lstat (const char *path, struct stat *st) { return stat (path, st); }
/* F_OK: it is there; R_OK, W_OK, X_OK: the bits of the mode that stat gives (a directory has all of them) */
int access (const char *path, int mode)
{
  struct stat64 s;
  if (mode & ~(R_OK | W_OK | X_OK)) { errno = EINVAL; return -1; }
  if (stat_path (path, &s)) return -1;
  if (((mode & R_OK) && !(s.st_mode & 0444)) || ((mode & W_OK) && !(s.st_mode & 0222)) || ((mode & X_OK) && !(s.st_mode & 0111))) { errno = EACCES; return -1; }
  return 0;
}

static int fill_stat (int fd, struct stat64 *st)
{
  FILE *f = get (fd);
  memset (st, 0, sizeof *st);
  if (!f) return -1;
  st->st_nlink = 1;
  st->st_blksize = 512;
  if (f->kind == K_FILE)
    {
      long ext;
      _kernel_osfile_block b = { 0, 0, 0, 0 };
      unsigned attr = 3;                                          /* owner read and write */
      if (__modlib_sync (f) || __modlib_file_len (f, &ext)) return -1;
      st->st_size = ext;
      st->st_blocks = (ext + 511) / 512;
      if (!(f->mode & M_WRITE)) attr &= ~2u;
      if (f->name && _kernel_osfile (17, f->name, &b) == 1)
	{
	  attr = (unsigned) b.end & 3;
	  st->st_atime = st->st_mtime = st->st_ctime = stamp_to_time ((unsigned) b.load, (unsigned) b.exec);
	}
      st->st_mode = S_IFREG | file_mode ((unsigned) b.load, attr);
    }
  else if (f->kind == K_SOCKET) st->st_mode = S_IFSOCK | 0666;
  else st->st_mode = S_IFCHR | 0620;                              /* the keyboard and the screen */
  return 0;
}
int fstat64 (int fd, struct stat64 *st) { return fill_stat (fd, st); }
int fstat (int fd, struct stat *st)
{
  struct stat64 s;
  if (fill_stat (fd, &s)) return -1;
  return narrow_stat (&s, st);
}

int ftruncate (int fd, off_t length)
{
  FILE *f = get (fd);
  if (!f) return -1;
  if (length < 0 || f->kind != K_FILE || !(f->mode & M_WRITE)) { errno = EINVAL; return -1; }
  return set_extent (f, (long) length);
}
int truncate (const char *path, off_t length)
{
  FILE *f;
  int r;
  if (length < 0) { errno = EINVAL; return -1; }
  f = __modlib_open_gen (0, path, 0, 1, 0, 0, 0, 0, 0);
  if (!f) return -1;
  r = set_extent (f, (long) length);
  fclose (f);
  return r;
}

int isatty (int fd)
{
  FILE *f = get (fd);
  if (!f) return 0;
  if (f->kind == K_KEYBOARD || f->kind == K_SCREEN) return 1;
  errno = ENOTTY;
  return 0;
}
int ioctl (int fd, unsigned long request, ...)
{
  FILE *f = get (fd);
  va_list ap;
  if (!f) return -1;
  if (f->kind == K_SOCKET)                                        /* the requests of the Internet module: FIONBIO, FIONREAD, FIOASYNC, SIOCGIFCONF ... (the argument is a pointer) */
    {
      void *arg;
      int r;
      va_start (ap, request);
      arg = va_arg (ap, void *);
      va_end (ap);
      r = __modlib_sock_op (SOP_IOCTL, (int) f->handle, 0, request, arg);
      if (r == 0 && request == FIONBIO && arg) { if (*(int *) arg) f->mode |= M_NONBLOCK; else f->mode &= ~M_NONBLOCK; }
      return r;
    }
  if (request == FIONREAD && f->kind == K_FILE)
    {
      long ext, pos;
      int *out;
      if (__modlib_sync (f) || __modlib_file_len (f, &ext)) return -1;
      pos = f->fpos;
      va_start (ap, request);
      out = va_arg (ap, int *);
      va_end (ap);
      *out = pos < ext ? (int) (ext - pos) : 0;
      return 0;
    }
  errno = ENOTTY;
  return -1;
}

/* fcntl: F_DUPFD (the lowest free descriptor of ARG or more), F_GETFD / F_SETFD (there is no close on exec), F_GETFL / F_SETFL (the access mode, O_APPEND; O_NONBLOCK of a socket, which is FIONBIO) */
int fcntl (int fd, int cmd, ...)
{
  FILE *f = get (fd);
  va_list ap;
  int arg = 0, i;
  if (!f) return -1;
  if (cmd == F_DUPFD || cmd == F_SETFL || cmd == F_SETFD)         /* (the others have no third argument) */
    {
      va_start (ap, cmd);
      arg = va_arg (ap, int);
      va_end (ap);
    }
  switch (cmd)
    {
    case F_DUPFD:
      if (arg < 3) arg = 3;
      for (i = arg; i < FDMAX; i++)
	if (!fdtab[i])
	  {
	    fdtab[i] = f;
	    if (f->fd < 0 && !is_std (f)) f->fd = i;
	    __modlib_fd_hook = release;
	    return i;
	  }
      errno = EMFILE;
      return -1;
    case F_GETFD:
    case F_SETFD:
      return 0;
    case F_GETFL:
      i = (f->mode & M_READ) && (f->mode & M_WRITE) ? O_RDWR : (f->mode & M_WRITE) ? O_WRONLY : O_RDONLY;
      if (f->mode & M_APPEND) i |= O_APPEND;
      if (f->mode & M_NONBLOCK) i |= O_NONBLOCK;
      return i;
    case F_SETFL:
      if (f->kind == K_SOCKET)
	{
	  int on = (arg & O_NONBLOCK) != 0;
	  if (__modlib_sock_op (SOP_IOCTL, (int) f->handle, 0, FIONBIO, &on)) return -1;
	  if (on) f->mode |= M_NONBLOCK; else f->mode &= ~M_NONBLOCK;
	}
      return 0;
    default:
      errno = EINVAL;
      return -1;
    }
}

int unlink (const char *path)
{
  _kernel_osfile_block b = { 0, 0, 0, 0 };
  int t = _kernel_osfile (17, path, &b);
  if (t == _kernel_ERROR) { __modlib_os_errno (); return -1; }
  if (t == 0) { errno = ENOENT; return -1; }
  if (t != 1) { errno = EISDIR; return -1; }                      /* a directory (or an image file) is not for unlink */
  return remove (path);
}

/* the date stamp of PATH to T seconds since 1970: the type stays, the time is put into the load and execution addresses */
static int set_time (const char *path, long t)
{
  _kernel_osfile_block b = { 0, 0, 0, 0 };
  int ty = _kernel_osfile (17, path, &b);
  unsigned long long cs;
  if (ty == _kernel_ERROR) { __modlib_os_errno (); return -1; }
  if (ty == 0) { errno = ENOENT; return -1; }
  if (((unsigned) b.load & 0xFFF00000u) != 0xFFF00000u) { errno = EINVAL; return -1; }          /* a file with a load and execution address of its own has no date stamp to set */
  cs = ((unsigned long long) ((long long) t + 2208988800LL)) * 100;
  b.load = (int) (((unsigned) b.load & 0xFFFFFF00u) | (unsigned) ((cs >> 32) & 0xFF));
  b.exec = (int) (unsigned) cs;
  if (_kernel_osfile (1, path, &b) == _kernel_ERROR) { __modlib_os_errno (); return -1; }          /* OS_File 1: write the catalogue information (the attributes in b.end stay as they were) */
  return 0;
}
int utime (const char *path, const struct utimbuf *times) { return set_time (path, times ? times->modtime : time (0)); }
int utimes (const char *path, const struct timeval tv[2]) { return set_time (path, tv ? tv[1].tv_sec : time (0)); }
