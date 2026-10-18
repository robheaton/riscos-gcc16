/* fileops.c - the calls on files that RISC OS has an equivalent for: fcntl, ioctl, fsync, ftruncate, truncate, chmod, chown, utime, utimes, realpath; and the ones it has not (dup, pipe, link ...), which fail.
   Names go through __riscos_path (the translation of UnixLib 64 if it is linked in).  The SDK of RISC OS 64. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/time.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern int __riscos_handle (int fd);
extern void __ul64_nosys (const char *);
extern const char *__riscos_path (const char *name, char *buf, int create_dir);
extern char *__unixify_std (const char *name, char *buffer, size_t buf_len, int filetype) __attribute__ ((weak));
#define OS_File 0x08
#define OS_Args 0x09
#define OS_FSControl 0x29
#define PATHMAX 1024

static int os_err (unsigned long *r)
{
  switch (((const unsigned *) r[0])[0] & 0xFF)
    {
    case 0xD6: return ENOENT;
    case 0xBD: case 0xC3: return EACCES;
    case 0xC2: return EBUSY;
    default: return EIO;
    }
}

extern int __ul64_is_sock (int) __attribute__ ((weak));		/* sockets.c */
extern int __ul64_sock_nonblock (int, int) __attribute__ ((weak));
extern int __ul64_sock_getfl (int) __attribute__ ((weak));
extern int __ul64_sock_ioctl (int, unsigned long, void *) __attribute__ ((weak));

int fcntl (int fd, int cmd, ...)
{
  if (__ul64_is_sock && __ul64_is_sock (fd))
    {
      va_list ap;
      int arg;
      switch (cmd)
	{
	case F_GETFD: return 0;
	case F_SETFD: return 0;
	case F_GETFL: return __ul64_sock_getfl (fd);
	case F_SETFL: va_start (ap, cmd); arg = va_arg (ap, int); va_end (ap); return __ul64_sock_nonblock (fd, (arg & O_NONBLOCK) != 0);
	default: errno = EINVAL; return -1;
	}
    }
  switch (cmd)
    {
    case F_GETFD: case F_SETFD: case F_SETFL: return 0;
    case F_GETFL: return O_RDWR;
    case F_SETLK: case F_SETLKW: return 0;			/* record locks: there is no other process to lock against */
    case F_GETLK: { va_list ap; struct flock *fl; va_start (ap, cmd); fl = va_arg (ap, struct flock *); va_end (ap); if (fl) fl->l_type = F_UNLCK; return 0; }
    case F_DUPFD: return dup (fd);
    default: errno = EINVAL; return -1;
    }
}

int ioctl (int fd, unsigned long request, ...)
{
  if (__ul64_is_sock && __ul64_is_sock (fd))
    {
      va_list ap;
      void *arg;
      va_start (ap, request); arg = va_arg (ap, void *); va_end (ap);
      return __ul64_sock_ioctl (fd, request, arg);
    }
  errno = ENOTTY;
  return -1;
}

int fsync (int fd)
{
  int h = __riscos_handle (fd);
  unsigned long r[10] = { 255, (unsigned long) h };			/* OS_Args 255: ensure the file is written out */
  if (h < 0) return 0;
  if (swi_x (OS_Args, r)) { errno = EIO; return -1; }
  return 0;
}

int ftruncate (int fd, off_t len)
{
  int h = __riscos_handle (fd);
  unsigned long r[10] = { 3, (unsigned long) h, (unsigned long) len };	/* OS_Args 3: set the extent */
  if (h < 0 || len < 0) { errno = h < 0 ? EBADF : EINVAL; return -1; }
  if (swi_x (OS_Args, r)) { errno = os_err (r); return -1; }
  {
    off_t end = lseek (fd, 0, SEEK_END);			/* a filing system that does not grow a file with OS_Args 3: write the last byte */
    if (end >= 0 && end < len)
      {
	off_t was = lseek (fd, 0, SEEK_CUR);
	char z = 0;
	if (lseek (fd, len - 1, SEEK_SET) < 0 || write (fd, &z, 1) != 1) return -1;
	lseek (fd, was, SEEK_SET);
      }
  }
  return 0;
}

int truncate (const char *path, off_t len)
{
  int fd = open (path, O_WRONLY), r;
  if (fd < 0) return -1;
  r = ftruncate (fd, len);
  close (fd);
  return r;
}

int chmod (const char *path, mode_t mode)
{
  char b[PATHMAX];
  unsigned long r[10] = { 17 };
  const char *p = __riscos_path (path, b, 0);
  unsigned attr;
  if (!p) return -1;
  r[1] = (unsigned long) p;
  if (swi_x (OS_File, r)) { errno = os_err (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  /* the RISC OS attributes: 1 owner read, 2 owner write, 8 locked, 16 public read, 32 public write */
  attr = ((mode & 0400) ? 1 : 0) | ((mode & 0200) ? 2 : 0) | ((mode & 0004) ? 16 : 0) | ((mode & 0002) ? 32 : 0);
  { unsigned long q[10] = { 4, (unsigned long) p, 0, 0, 0, attr };	/* OS_File 4: write the attributes */
    if (swi_x (OS_File, q)) { errno = os_err (q); return -1; } }
  return 0;
}
int fchmod (int fd, mode_t mode) { (void) fd; (void) mode; return 0; }			/* (like chown: there is nothing to change; objcopy and cp call it on the file they have made) */
int chown (const char *path, uid_t u, gid_t g) { (void) u; (void) g; return access (path, F_OK); }
int fchown (int fd, uid_t u, gid_t g) { (void) fd; (void) u; (void) g; return 0; }

/* the date stamp of a file: load address top 12 bits &FFF, file type, then 40 bits of centiseconds since 1900 */
static int set_time (const char *path, time_t t)
{
  char b[PATHMAX];
  unsigned long r[10] = { 17 };
  const char *p = __riscos_path (path, b, 0);
  unsigned long long cs;
  unsigned long load, exec, type;
  if (!p) return -1;
  r[1] = (unsigned long) p;
  if (swi_x (OS_File, r)) { errno = os_err (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  type = ((r[2] & 0xFFF00000UL) == 0xFFF00000UL) ? ((r[2] >> 8) & 0xFFF) : 0xFFF;
  cs = ((unsigned long long) t + 2208988800ULL) * 100;
  load = 0xFFF00000UL | (type << 8) | (unsigned long) ((cs >> 32) & 0xFF);
  exec = (unsigned long) (cs & 0xFFFFFFFFUL);
  { unsigned long q[10] = { 1, (unsigned long) p, load, exec, 0, r[5] };	/* OS_File 1: write the catalogue information */
    if (swi_x (OS_File, q)) { errno = os_err (q); return -1; } }
  return 0;
}
int utime (const char *path, const struct utimbuf *times) { return set_time (path, times ? times->modtime : time (0)); }
int utimes (const char *path, const struct timeval tv[2]) { return set_time (path, tv ? tv[1].tv_sec : time (0)); }

char *realpath (const char *path, char *resolved)
{
  char b[PATHMAX], ros[PATHMAX], tmp[PATHMAX];
  unsigned long r[10] = { 17 };
  const char *p;
  if (!path) { errno = EINVAL; return 0; }
  p = __riscos_path (path, b, 0);
  if (!p) return 0;
  r[1] = (unsigned long) p;
  if (swi_x (OS_File, r)) { errno = os_err (r); return 0; }
  if (r[0] == 0) { errno = ENOENT; return 0; }
  { unsigned long q[10] = { 37, (unsigned long) p, (unsigned long) ros, 0, 0, sizeof ros };	/* OS_FSControl 37: canonicalise */
    if (swi_x (OS_FSControl, q)) { errno = os_err (q); return 0; } }
  if (__unixify_std) { if (!__unixify_std (ros, tmp, sizeof tmp, -1)) { errno = ENAMETOOLONG; return 0; } }
  else { strncpy (tmp, ros, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0; }
  if (!resolved) return strdup (tmp);
  memcpy (resolved, tmp, strlen (tmp) + 1);				/* (not strncpy: it pads with zeroes to the given size, and the caller's buffer may be smaller than PATHMAX: pathconf says what it is) */
  return resolved;
}

extern int __riscos_dup (int fd, int want);
int dup (int fd) { return __riscos_dup (fd, -1); }
extern int __ul64_vfork_depth_get (void);
int dup2 (int fd, int fd2)
{
  if (fd2 < 0) { errno = EBADF; return -1; }
  /* in a vfork child, before the exec: the standard descriptors of the program that is started are the system's, so a redirection of them has no meaning here and is accepted (the descriptor must be good) */
  if (fd2 < 3 && __ul64_vfork_depth_get () > 0) { struct stat st; if (fd != fd2 && fstat (fd, &st) != 0) return -1; return fd2; }
  return __riscos_dup (fd, fd2);
}
int link (const char *a, const char *b) { (void) a; (void) b; __ul64_nosys ("link"); return -1; }
int symlink (const char *a, const char *b) { (void) a; (void) b; __ul64_nosys ("symlink"); return -1; }
ssize_t readlink (const char *p, char *buf, size_t n) { (void) p; (void) buf; (void) n; errno = EINVAL; return -1; }
char *ttyname (int fd) { (void) fd; errno = ENOTTY; return 0; }
