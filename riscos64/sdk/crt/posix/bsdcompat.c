/* bsdcompat.c - small BSD, SysV and UnixLib functions that programs call and picolibc does not have (SDK of RISC OS 64): err and warn, readv and writev, readdir_r,
   tempnam and tmpnam_r, lsearch and lfind, insque and remque, fpurge, getw and putw, ftime, timelocal, ulimit, getwd, getpriority and setpriority, wait3 and wait4, vsyslog and
   setlogmask, stricmp and strnicmp and the other UnixLib string extras, getgrouplist, __fpu_control. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <err.h>
#include <fpu_control.h>
#include <grp.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char *program_invocation_short_name;
fpu_control_t __fpu_control = _FPU_DEFAULT;

/* ---- err.h ---- */
static void prefix (void) { fprintf (stderr, "%s: ", program_invocation_short_name ? program_invocation_short_name : "?"); }
void vwarnc (int code, const char *fmt, va_list ap)
{
  prefix ();
  if (fmt) { vfprintf (stderr, fmt, ap); fputs (": ", stderr); }
  fprintf (stderr, "%s\n", strerror (code));
}
void vwarn (const char *fmt, va_list ap) { vwarnc (errno, fmt, ap); }
void vwarnx (const char *fmt, va_list ap) { prefix (); if (fmt) vfprintf (stderr, fmt, ap); fputc ('\n', stderr); }
void warn (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (errno, fmt, ap); va_end (ap); }
void warnx (const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnx (fmt, ap); va_end (ap); }
void warnc (int code, const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (code, fmt, ap); va_end (ap); }
void verrc (int status, int code, const char *fmt, va_list ap) { vwarnc (code, fmt, ap); exit (status); }
void verr (int status, const char *fmt, va_list ap) { vwarnc (errno, fmt, ap); exit (status); }
void verrx (int status, const char *fmt, va_list ap) { vwarnx (fmt, ap); exit (status); }
void err (int status, const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (errno, fmt, ap); va_end (ap); exit (status); }
void errx (int status, const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnx (fmt, ap); va_end (ap); exit (status); }
void errc (int status, int code, const char *fmt, ...) { va_list ap; va_start (ap, fmt); vwarnc (code, fmt, ap); va_end (ap); exit (status); }

/* ---- sys/uio.h ---- */
ssize_t readv (int fd, const struct iovec *v, int n)
{
  ssize_t total = 0; int i;
  for (i = 0; i < n; i++)
  {
    ssize_t r;
    if (!v[i].iov_len) continue;
    r = read (fd, v[i].iov_base, v[i].iov_len);
    if (r < 0) return total ? total : -1;
    total += r;
    if ((size_t) r < v[i].iov_len) break;
  }
  return total;
}
ssize_t writev (int fd, const struct iovec *v, int n)
{
  ssize_t total = 0; int i;
  for (i = 0; i < n; i++)
  {
    ssize_t r;
    if (!v[i].iov_len) continue;
    r = write (fd, v[i].iov_base, v[i].iov_len);
    if (r < 0) return total ? total : -1;
    total += r;
    if ((size_t) r < v[i].iov_len) break;
  }
  return total;
}

/* ---- dirent.h: the reentrant readdir (the entry is copied into the caller's) ---- */
int readdir_r (DIR *d, struct dirent *entry, struct dirent **result)
{
  struct dirent *e;
  errno = 0;
  e = readdir (d);
  if (!e) { *result = NULL; return errno; }
  memcpy (entry, e, sizeof *e);
  *result = entry;
  return 0;
}

/* ---- stdio.h ---- */
char *tmpnam_r (char *s) { return s ? tmpnam (s) : NULL; }
char *tempnam (const char *dir, const char *pfx)
{
  char *t, buf[L_tmpnam + 16];
  const char *d = dir ? dir : getenv ("TMPDIR");
  size_t n;
  if (!d) d = "/tmp";
  if (!tmpnam (buf)) return NULL;
  { const char *base = strrchr (buf, '/'); base = base ? base + 1 : buf;
    n = strlen (d) + strlen (pfx ? pfx : "") + strlen (base) + 2;
    t = malloc (n);
    if (t) snprintf (t, n, "%s/%s%s", d, pfx ? pfx : "", base); }
  return t;
}
int fpurge (FILE *f) { return fflush (f); }
int getw (FILE *f) { int w; return fread (&w, sizeof w, 1, f) == 1 ? w : EOF; }
int putw (int w, FILE *f) { return fwrite (&w, sizeof w, 1, f) == 1 ? 0 : EOF; }
char *getwd (char *buf) { return getcwd (buf, PATH_MAX); }

/* ---- search.h ---- */
void *lfind (const void *key, const void *base, size_t *nmemb, size_t size, int (*cmp) (const void *, const void *))
{
  size_t i;
  for (i = 0; i < *nmemb; i++) { const char *p = (const char *) base + i * size; if (!cmp (key, p)) return (void *) p; }
  return NULL;
}
void *lsearch (const void *key, void *base, size_t *nmemb, size_t size, int (*cmp) (const void *, const void *))
{
  void *p = lfind (key, base, nmemb, size, cmp);
  if (p) return p;
  p = (char *) base + *nmemb * size;
  memcpy (p, key, size);
  ++*nmemb;
  return p;
}
struct qelem { struct qelem *q_forw, *q_back; };
void insque (void *elem, void *prev)
{
  struct qelem *e = elem, *p = prev;
  if (!p) { e->q_forw = e->q_back = NULL; return; }
  e->q_forw = p->q_forw; e->q_back = p;
  if (p->q_forw) p->q_forw->q_back = e;
  p->q_forw = e;
}
void remque (void *elem)
{
  struct qelem *e = elem;
  if (e->q_forw) e->q_forw->q_back = e->q_back;
  if (e->q_back) e->q_back->q_forw = e->q_forw;
}

/* ---- time ---- */
int ftime (struct timeb *tb)
{
  struct timespec ts;
  if (clock_gettime (CLOCK_REALTIME, &ts)) return -1;
  tb->time = ts.tv_sec; tb->millitm = (unsigned short) (ts.tv_nsec / 1000000); tb->timezone = 0; tb->dstflag = 0;
  return 0;
}
time_t timelocal (struct tm *tm) { return mktime (tm); }

/* ---- limits and priorities, wait3 and wait4 ---- */
long ulimit (int cmd, ...)
{
  struct rlimit rl;
  if (cmd == 3) { if (getrlimit (RLIMIT_DATA, &rl)) return -1; return (long) (rl.rlim_cur / 512); }	/* UL_GETMAXBRK */
  if (cmd == 1) { if (getrlimit (RLIMIT_FSIZE, &rl)) return -1; return (long) (rl.rlim_cur / 512); }	/* UL_GETFSIZE */
  errno = EINVAL; return -1;
}
int getpriority (int which, id_t who) { (void) which; (void) who; return 0; }
int setpriority (int which, id_t who, int prio) { (void) which; (void) who; (void) prio; return 0; }
pid_t wait4 (pid_t pid, int *status, int options, struct rusage *ru)
{
  pid_t r = waitpid (pid, status, options);
  if (r > 0 && ru) memset (ru, 0, sizeof *ru);
  return r;
}
pid_t wait3 (int *status, int options, struct rusage *ru) { return wait4 (-1, status, options, ru); }

/* ---- syslog.h: nothing is kept (compat.c); the mask is remembered ---- */
static int log_mask = 0xFF;
int setlogmask (int mask) { int old = log_mask; if (mask) log_mask = mask; return old; }

/* ---- UnixLib string extras ---- */
int stricmp (const char *a, const char *b) { return strcasecmp (a, b); }
int strnicmp (const char *a, const char *b, size_t n) { return strncasecmp (a, b, n); }
char *strichr (const char *s, int c)
{
  c = tolower ((unsigned char) c);
  for (; *s; s++) if (tolower ((unsigned char) *s) == c) return (char *) s;
  return c == 0 ? (char *) s : NULL;
}
char *strrichr (const char *s, int c)
{
  const char *last = NULL;
  c = tolower ((unsigned char) c);
  for (; *s; s++) if (tolower ((unsigned char) *s) == c) last = s;
  return c == 0 ? (char *) s : (char *) last;
}

/* ---- grp.h: there is one user and one group (the ids of the system), so the list of groups of a user is the group given ---- */
int getgrouplist (const char *user, gid_t group, gid_t *groups, int *ngroups)
{
  (void) user;
  if (*ngroups < 1) { *ngroups = 1; return -1; }
  groups[0] = group; *ngroups = 1;
  return 1;
}

int chroot (const char *path) { (void) path; errno = EPERM; return -1; }	/* one user, no root directory to change */

/* ---- strsep: picolibc's drops the empty field after a final delimiter and returns NULL for an empty string (glibc and BSD return "" there) ---- */
char *strsep (char **sp, const char *delim)
{
  char *s = *sp, *p;
  if (!s) return NULL;
  p = s + strcspn (s, delim);
  if (*p) { *p++ = 0; *sp = p; } else *sp = NULL;
  return s;
}

/* ---- one user, one machine: these report what they cannot do ---- */
int settimeofday (const struct timeval *tv, const struct timezone *tz) { (void) tv; (void) tz; errno = EPERM; return -1; }
int adjtime (const struct timeval *d, struct timeval *o) { (void) d; if (o) { o->tv_sec = 0; o->tv_usec = 0; } errno = EPERM; return -1; }
int mknod (const char *path, mode_t mode, dev_t dev) { (void) path; (void) mode; (void) dev; errno = EPERM; return -1; }
int shm_open (const char *name, int flags, mode_t mode) { (void) name; (void) flags; (void) mode; errno = ENOSYS; return -1; }
int shm_unlink (const char *name) { (void) name; errno = ENOSYS; return -1; }
int malloc_trim (size_t pad) { (void) pad; return 0; }			/* nothing is given back to the system by this allocator */
char *ctermid (char *s) { static char buf[] = "/dev/tty"; if (s) { strcpy (s, buf); return s; } return buf; }
char *cuserid (char *s) { static char buf[] = "user"; const char *u = getenv ("USER"); if (!u) u = buf; if (s) { strcpy (s, u); return s; } return (char *) u; }
int setpgrp (void) { return 0; }
useconds_t ualarm (useconds_t usecs, useconds_t interval) { (void) interval; alarm ((usecs + 999999) / 1000000); return 0; }

/* ---- more one-machine answers ---- */
int clock_settime (clockid_t id, const struct timespec *ts) { (void) id; (void) ts; errno = EPERM; return -1; }
int dysize (int year) { return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 366 : 365; }
int getdomainname (char *name, size_t len) { if (len < 1) { errno = EINVAL; return -1; } name[0] = 0; return 0; }
int setdomainname (const char *name, size_t len) { (void) name; (void) len; errno = EPERM; return -1; }
int brk (void *addr)
{
  char *cur = sbrk (0);
  if (cur == (char *) -1) return -1;
  if (sbrk ((char *) addr - cur) == (void *) -1) return -1;
  return 0;
}
int execle (const char *path, const char *arg, ...)
{
  va_list ap; const char *argv[64]; int n = 0; char *const *envp;
  va_start (ap, arg);
  argv[n++] = arg;
  while (arg && n < 63) argv[n++] = arg = va_arg (ap, const char *);
  envp = va_arg (ap, char *const *);
  va_end (ap);
  argv[n] = NULL;
  return execve (path, (char *const *) argv, envp);
}

/* ---- b64_ntop and b64_pton: the base 64 of the resolver library (RFC 4648 alphabet), used by SSH and DNSSEC code ---- */
static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int b64_ntop (const unsigned char *src, size_t len, char *dst, size_t size)
{
  size_t o = 0;
  while (len >= 3)
  {
    if (o + 4 >= size) return -1;
    dst[o++] = b64chars[src[0] >> 2];
    dst[o++] = b64chars[((src[0] & 3) << 4) | (src[1] >> 4)];
    dst[o++] = b64chars[((src[1] & 15) << 2) | (src[2] >> 6)];
    dst[o++] = b64chars[src[2] & 63];
    src += 3; len -= 3;
  }
  if (len)
  {
    if (o + 4 >= size) return -1;
    dst[o++] = b64chars[src[0] >> 2];
    if (len == 1) { dst[o++] = b64chars[(src[0] & 3) << 4]; dst[o++] = '='; }
    else { dst[o++] = b64chars[((src[0] & 3) << 4) | (src[1] >> 4)]; dst[o++] = b64chars[(src[1] & 15) << 2]; }
    dst[o++] = '=';
  }
  if (o >= size) return -1;
  dst[o] = 0;
  return (int) o;
}
int b64_pton (const char *src, unsigned char *dst, size_t size)
{
  unsigned acc = 0; int bits = 0, pad = 0; size_t o = 0; const char *p;
  for (; *src; src++)
  {
    int v;
    if (isspace ((unsigned char) *src)) continue;
    if (*src == '=') { pad++; continue; }
    if (pad) return -1;				/* data after the padding */
    p = strchr (b64chars, *src);
    if (!p || !*src) return -1;
    v = (int) (p - b64chars);
    acc = (acc << 6) | (unsigned) v; bits += 6;
    if (bits >= 8)
    {
      bits -= 8;
      if (o >= size) return -1;
      dst[o++] = (unsigned char) (acc >> bits);
      acc &= (1u << bits) - 1;
    }
  }
  if (bits >= 6 || (acc != 0)) return -1;		/* stray bits: not a valid end */
  return (int) o;
}
