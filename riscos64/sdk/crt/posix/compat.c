/* compat.c - small POSIX calls that need no operating system support, or are the plain call with a different name: the *at calls (for the current folder and for full names), process groups and sessions
   (one process: the ids are fixed), user and group ids (one user, as in ids.c), file locks (nobody else to lock against), syslog (nothing is kept), inet_addr and its relatives, scandir, mkdtemp, getrandom.
   libriscosstub.a is searched before libc.a.  The SDK of RISC OS 64. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/random.h>
#include <execinfo.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef AT_REMOVEDIR
#define AT_REMOVEDIR 0x200
#endif

/* ---- the *at calls: a descriptor other than AT_FDCWD is not supported unless the name is a full one ---- */

extern const char *__riscos_fd_name (int fd);
/* the name for a call with a directory descriptor: the folder's name and the path; AT_FDCWD and full names are as they are */
static char at_buf[4][1100];
static int at_slot;
static const char *at_name (int fd, const char *path)
{
  const char *dir;
  char *b;
  if (!path) { errno = EFAULT; return 0; }
  if (fd == AT_FDCWD || path[0] == '/' || path[0] == '$') return path;
  dir = __riscos_fd_name (fd);
  if (!dir) { errno = EBADF; return 0; }
  b = at_buf[at_slot++ & 3];
  snprintf (b, 1100, "%s/%s", dir, path);
  return b;
}
int openat (int fd, const char *path, int flags, ...)
{
  va_list ap;
  int mode = 0;
  if (!(path = at_name (fd, path))) return -1;
  va_start (ap, flags);
  mode = va_arg (ap, int);
  va_end (ap);
  return open (path, flags, mode);
}
int fstatat (int fd, const char *path, struct stat *st, int flags) { (void) flags; return (path = at_name (fd, path)) ? stat (path, st) : -1; }
int faccessat (int fd, const char *path, int mode, int flags) { (void) flags; return (path = at_name (fd, path)) ? access (path, mode) : -1; }
int unlinkat (int fd, const char *path, int flags)
{
  if (!(path = at_name (fd, path))) return -1;
  return (flags & AT_REMOVEDIR) ? rmdir (path) : unlink (path);
}
int mkdirat (int fd, const char *path, mode_t mode) { return (path = at_name (fd, path)) ? mkdir (path, mode) : -1; }
int renameat (int ofd, const char *o, int nfd, const char *n) { return (o = at_name (ofd, o)) && (n = at_name (nfd, n)) ? rename (o, n) : -1; }

int fchownat (int fd, const char *path, uid_t u, gid_t g, int flags) { (void) flags; return (path = at_name (fd, path)) ? chown (path, u, g) : -1; }
int fchmodat (int fd, const char *path, mode_t mode, int flags) { (void) flags; return (path = at_name (fd, path)) ? chmod (path, mode) : -1; }
int linkat (int ofd, const char *o, int nfd, const char *n, int flags) { (void) flags; return (o = at_name (ofd, o)) && (n = at_name (nfd, n)) ? link (o, n) : -1; }	/* (a copy: see link ()) */
int symlinkat (const char *target, int fd, const char *path) { (void) target; (void) fd; (void) path; errno = ENOSYS; return -1; }
ssize_t readlinkat (int fd, const char *path, char *buf, size_t n) { (void) fd; (void) path; (void) buf; (void) n; errno = EINVAL; return -1; }
int mkfifoat (int fd, const char *path, mode_t mode) { (void) fd; (void) path; (void) mode; errno = ENOSYS; return -1; }
int mknodat (int fd, const char *path, mode_t mode, dev_t dev) { (void) fd; (void) path; (void) mode; (void) dev; errno = ENOSYS; return -1; }

/* ---- times of files ---- */

int futimes (int fd, const struct timeval tv[2]) { (void) fd; (void) tv; errno = ENOSYS; return -1; }
int lutimes (const char *path, const struct timeval tv[2]) { return utimes (path, tv); }
int utimensat (int fd, const char *path, const struct timespec ts[2], int flags)
{
  struct timeval tv[2];
  (void) flags;
  if (!(path = at_name (fd, path))) return -1;
  if (!ts) return utimes (path, 0);
  tv[0].tv_sec = ts[0].tv_sec; tv[0].tv_usec = ts[0].tv_nsec / 1000;
  tv[1].tv_sec = ts[1].tv_sec; tv[1].tv_usec = ts[1].tv_nsec / 1000;
  return utimes (path, tv);
}
int futimens (int fd, const struct timespec ts[2]) { (void) fd; (void) ts; errno = ENOSYS; return -1; }

/* ---- files ---- */

ssize_t pread (int fd, void *buf, size_t n, off_t off)
{
  off_t was = lseek (fd, 0, SEEK_CUR);
  ssize_t r;
  if (was < 0 || lseek (fd, off, SEEK_SET) < 0) return -1;
  r = read (fd, buf, n);
  lseek (fd, was, SEEK_SET);					/* (one process, one thread at a time: the position can be put back) */
  return r;
}
ssize_t pwrite (int fd, const void *buf, size_t n, off_t off)
{
  off_t was = lseek (fd, 0, SEEK_CUR);
  ssize_t r;
  if (was < 0 || lseek (fd, off, SEEK_SET) < 0) return -1;
  r = write (fd, buf, n);
  lseek (fd, was, SEEK_SET);
  return r;
}

int fdatasync (int fd) { return fsync (fd); }
void sync (void) { }
int lchown (const char *p, uid_t u, gid_t g) { return chown (p, u, g); }
int posix_fadvise (int fd, off_t off, off_t len, int advice) { (void) fd; (void) off; (void) len; (void) advice; return 0; }
int posix_fallocate (int fd, off_t off, off_t len)
{
  struct stat st;
  if (fstat (fd, &st)) return errno;
  if (st.st_size < off + len && ftruncate (fd, off + len)) return errno;
  return 0;
}
int flock (int fd, int op) { (void) fd; (void) op; return 0; }
int lockf (int fd, int cmd, off_t len) { (void) fd; (void) cmd; (void) len; return 0; }
int mlock (const void *a, size_t n) { (void) a; (void) n; return 0; }
int pipe2 (int fd[2], int flags) { (void) flags; return pipe (fd); }
int dup3 (int a, int b, int flags)
{
  (void) flags;
  if (a == b) { errno = EINVAL; return -1; }
  return dup2 (a, b);
}
int getdtablesize (void) { return 256; }
int ttyname_r (int fd, char *buf, size_t len)
{
  const char *n = ttyname (fd);
  if (!n) return errno ? errno : ENOTTY;
  if (strlen (n) + 1 > len) return ERANGE;
  strcpy (buf, n);
  return 0;
}

char *mkdtemp (char *t)
{
  size_t n = strlen (t);
  static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  int tries;
  if (n < 6 || strcmp (t + n - 6, "XXXXXX")) { errno = EINVAL; return 0; }
  for (tries = 0; tries < 100; tries++)
    {
      int i;
      for (i = 0; i < 6; i++) t[n - 6 + i] = chars[(unsigned) rand () % (sizeof chars - 1)];
      if (mkdir (t, 0700) == 0) return t;
      if (errno != EEXIST) return 0;
    }
  errno = EEXIST;
  return 0;
}

int alphasort (const struct dirent **a, const struct dirent **b) { return strcmp ((*a)->d_name, (*b)->d_name); }
int scandir (const char *dir, struct dirent ***list, int (*sel) (const struct dirent *), int (*cmp) (const struct dirent **, const struct dirent **))
{
  DIR *d = opendir (dir);
  struct dirent *e, **v = 0;
  size_t n = 0, cap = 0;
  if (!d) return -1;
  while ((e = readdir (d)) != 0)
    {
      struct dirent *c;
      if (sel && !sel (e)) continue;
      if (n == cap)
	{
	  struct dirent **nv = realloc (v, (cap ? cap * 2 : 16) * sizeof *v);
	  if (!nv) goto fail;
	  v = nv; cap = cap ? cap * 2 : 16;
	}
      c = malloc (sizeof *c);
      if (!c) goto fail;
      memcpy (c, e, sizeof *c);
      v[n++] = c;
    }
  closedir (d);
  if (cmp && n > 1) qsort (v, n, sizeof *v, (int (*) (const void *, const void *)) cmp);
  *list = v;
  return (int) n;
fail:
  while (n) free (v[--n]);
  free (v);
  closedir (d);
  errno = ENOMEM;
  return -1;
}

/* ---- time and polling ---- */

int clock_getres (clockid_t id, struct timespec *res)
{
  (void) id;
  if (res) { res->tv_sec = 0; res->tv_nsec = 10000000; }	/* the centisecond clock of RISC OS */
  return 0;
}
int clock_nanosleep (clockid_t id, int flags, const struct timespec *req, struct timespec *rem)
{
  struct timespec now, d;
  if (flags & 1)					/* TIMER_ABSTIME */
    {
      clock_gettime (id, &now);
      d.tv_sec = req->tv_sec - now.tv_sec; d.tv_nsec = req->tv_nsec - now.tv_nsec;
      if (d.tv_nsec < 0) { d.tv_nsec += 1000000000L; d.tv_sec--; }
      if (d.tv_sec < 0) return 0;
      req = &d;
    }
  return nanosleep (req, rem) ? errno : 0;
}
int ppoll (struct pollfd *fds, nfds_t n, const struct timespec *to, const sigset_t *mask)
{
  (void) mask;
  return poll (fds, n, to ? (int) (to->tv_sec * 1000 + to->tv_nsec / 1000000) : -1);
}

/* ---- processes, users and groups: one of each ---- */

pid_t getpgrp (void) { return getpid (); }
pid_t getpgid (pid_t p) { return p == 0 || p == getpid () ? getpid () : (errno = ESRCH, -1); }
pid_t getsid (pid_t p) { return p == 0 || p == getpid () ? getpid () : (errno = ESRCH, -1); }
pid_t setsid (void) { errno = EPERM; return -1; }				/* already a session leader */
int setpgid (pid_t p, pid_t g) { return (p == 0 || p == getpid ()) && (g == 0 || g == getpid ()) ? 0 : (errno = EPERM, -1); }
int tcsetpgrp (int fd, pid_t g) { (void) fd; return g == getpid () ? 0 : (errno = EPERM, -1); }
int killpg (pid_t g, int sig) { return kill (-g, sig); }
int nice (int inc) { (void) inc; return 0; }
int setuid (uid_t u) { return u == getuid () ? 0 : (errno = EPERM, -1); }
int seteuid (uid_t u) { return setuid (u); }
int setgid (gid_t g) { return g == getgid () ? 0 : (errno = EPERM, -1); }
int setegid (gid_t g) { return setgid (g); }
int initgroups (const char *user, gid_t g) { (void) user; (void) g; return 0; }
int getgroups (int n, gid_t *list)
{
  if (n > 0) list[0] = getgid ();
  return 1;
}
int getlogin_r (char *buf, size_t len)
{
  const char *n = getenv ("Unix$User");
  if (!n || !*n) n = getenv ("USER");
  if (!n || !*n) n = "user";
  if (strlen (n) + 1 > len) return ERANGE;
  strcpy (buf, n);
  return 0;
}

/* ---- syslog: nothing is kept ---- */

void openlog (const char *ident, int opt, int fac) { (void) ident; (void) opt; (void) fac; }
void closelog (void) { }
void syslog (int pri, const char *fmt, ...) { (void) pri; (void) fmt; }
void vsyslog (int pri, const char *fmt, va_list ap) { (void) pri; (void) fmt; (void) ap; }

/* backtrace: the return addresses on the chain of frame records (x29), innermost first.  Code compiled without -fno-omit-frame-pointer has no records, and the chain skips it.  The addresses are in the
   program's image (the map file of the link, -Wl,-Map=NAME.map, or tools/crashsym.py names them). */
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
int backtrace (void **buf, int n)
{
  unsigned long fp = (unsigned long) __builtin_frame_address (0), limit = 0xFFFFFFFFUL, e[10] = { 0 };
  int k = 0;
  if (!swi_x (0x10, e) && e[1] > 0x8000) limit = e[1];				/* OS_GetEnv: the end of the memory of the program */
  while (k < n && fp >= 0x8000 && !(fp & 15) && fp + 16 <= limit)
    {
      unsigned long *f = (unsigned long *) fp;
      if (!f[1]) break;
      buf[k++] = (void *) f[1];
      if (f[0] <= fp) break;
      fp = f[0];
    }
  return k;
}
char **backtrace_symbols (void *const *buf, int n)
{
  int i;
  size_t total = (size_t) n * sizeof (char *);
  char **r;
  char *p;
  for (i = 0; i < n; i++) total += 24;
  r = (char **) malloc (total + 1);
  if (!r) return 0;
  p = (char *) (r + n);
  for (i = 0; i < n; i++)
    {
      unsigned long v = (unsigned long) buf[i];
      int j;
      r[i] = p;
      p[0] = '['; p[1] = '0'; p[2] = 'x';
      for (j = 0; j < 16; j++) p[3 + j] = "0123456789abcdef"[(v >> (60 - 4 * j)) & 15];
      p[19] = ']'; p[20] = 0;
      p += 24;
    }
  return r;
}
void backtrace_symbols_fd (void *const *buf, int n, int fd)
{
  char **s = backtrace_symbols (buf, n);
  int i;
  if (!s) return;
  for (i = 0; i < n; i++) { write (fd, s[i], strlen (s[i])); write (fd, "\n", 1); }
  free (s);
}

ssize_t getrandom (void *buf, size_t n, unsigned flags)
{
  size_t i;
  (void) flags;
  for (i = 0; i < n; i++) ((unsigned char *) buf)[i] = (unsigned char) (arc4random () >> 8);
  return (ssize_t) n;
}

/* ---- inet_addr and friends (IPv4 and IPv6 in text form) ---- */

int inet_pton (int af, const char *s, void *dst)
{
  if (af == 2)						/* AF_INET */
    {
      unsigned char *o = dst;
      int i;
      for (i = 0; i < 4; i++)
	{
	  unsigned v = 0, digits = 0;
	  while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned) (*s++ - '0'); if (++digits > 3 || v > 255) return 0; }
	  if (!digits || (i < 3 && *s++ != '.')) return 0;
	  o[i] = (unsigned char) v;
	}
      return *s == 0;
    }
  if (af == 10)						/* AF_INET6 */
    {
      unsigned short w[8];
      int n = 0, gap = -1;
      unsigned char *o = dst;
      if (*s == ':') { if (s[1] != ':') return 0; s++; }
      while (*s)
	{
	  unsigned v = 0, digits = 0;
	  if (*s == ':') { if (gap >= 0) return 0; gap = n; s++; continue; }
	  while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))
	    {
	      v = v * 16 + (unsigned) (*s <= '9' ? *s - '0' : (*s | 32) - 'a' + 10); s++;
	      if (++digits > 4) return 0;
	    }
	  if (!digits || n >= 8) return 0;
	  w[n++] = (unsigned short) v;
	  if (*s == ':') { s++; if (!*s) return 0; if (*s == ':') continue; }
	  else if (*s) return 0;
	}
      if (gap < 0 ? n != 8 : n > 7) return 0;
      if (gap >= 0) { int k = 8 - n, i; for (i = n - 1; i >= gap; i--) w[i + k] = w[i]; for (i = gap; i < gap + k; i++) w[i] = 0; }
      for (n = 0; n < 8; n++) { o[2 * n] = (unsigned char) (w[n] >> 8); o[2 * n + 1] = (unsigned char) w[n]; }
      return 1;
    }
  errno = EAFNOSUPPORT;
  return -1;
}

const char *inet_ntop (int af, const void *src, char *dst, unsigned size)
{
  const unsigned char *o = src;
  char tmp[64];
  if (af == 2) snprintf (tmp, sizeof tmp, "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
  else if (af == 10)
    {
      unsigned w[8];
      int i, best = -1, bestlen = 0, cur = -1, curlen = 0;
      char *p = tmp;
      for (i = 0; i < 8; i++) w[i] = (unsigned) o[2 * i] << 8 | o[2 * i + 1];
      for (i = 0; i < 8; i++)
	{
	  if (w[i] == 0) { if (cur < 0) { cur = i; curlen = 0; } curlen++; if (curlen > bestlen) { best = cur; bestlen = curlen; } }
	  else cur = -1;
	}
      if (bestlen < 2) best = -1;
      for (i = 0; i < 8; i++)
	{
	  if (i == best) { *p++ = ':'; i += bestlen - 1; if (i == 7) *p++ = ':'; continue; }
	  if (i) *p++ = ':';
	  p += sprintf (p, "%x", w[i]);
	}
      *p = 0;
    }
  else { errno = EAFNOSUPPORT; return 0; }
  if (strlen (tmp) + 1 > size) { errno = ENOSPC; return 0; }
  strcpy (dst, tmp);
  return dst;
}

/* the old calls, IPv4 only; the address is in network order */
int inet_aton (const char *s, struct in_addr *dst)
{
  unsigned char b[4];
  if (inet_pton (2, s, b) != 1) return 0;
  memcpy (dst, b, 4);
  return 1;
}
in_addr_t inet_addr (const char *s)
{
  struct in_addr a;
  return inet_aton (s, &a) ? a.s_addr : 0xFFFFFFFFu;
}
char *inet_ntoa (struct in_addr a)
{
  static char buf[16];
  inet_ntop (2, &a, buf, sizeof buf);
  return buf;
}

/* ---- the host name and the groups cannot be changed; statfs is statvfs ---- */
int sethostname (const char *name, size_t len) { (void) name; (void) len; errno = EPERM; return -1; }
int setgroups (size_t n, const gid_t *list) { (void) n; (void) list; errno = EPERM; return -1; }
int statfs (const char *path, struct statfs *s)
{
  struct statvfs v;
  if (statvfs (path, &v) != 0) return -1;
  memset (s, 0, sizeof *s);
  s->f_bsize = (long) v.f_bsize; s->f_blocks = v.f_blocks; s->f_bfree = v.f_bfree; s->f_bavail = v.f_bavail;
  s->f_files = v.f_files; s->f_ffree = v.f_ffree; s->f_namelen = (long) v.f_namemax; s->f_frsize = (long) v.f_frsize;
  return 0;
}
int fstatfs (int fd, struct statfs *s) { (void) fd; (void) s; errno = ENOSYS; return -1; }

/* sysconf: a few answers that suit RISC OS 64; the rest is picolibc's (renamed __pico_sysconf when the tool chain is built, like setlocale) */
extern long __pico_sysconf (int name);
long sysconf (int name)
{
  switch (name)
    {
    case _SC_ARG_MAX: return 32768;			/* xargs and make split long command lines to fit */
    case _SC_OPEN_MAX: return 240;			/* RISC OS file handles 1 to 255 */
    case _SC_CLK_TCK: return 100;			/* centiseconds */
    case _SC_PAGESIZE: return 4096;
    default: return __pico_sysconf (name);
    }
}
