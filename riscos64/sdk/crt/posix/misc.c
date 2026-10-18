/* misc.c - small POSIX functions that GCC, binutils and make link but that picolibc does not have: confstr, secure_getenv, eaccess, getentropy, getloadavg, pselect, madvise, mkfifo, syscall,
   the stdio *_unlocked functions and the posix_spawn attribute and file action objects (stored; the actions are not carried out: a started program has the screen and keyboard of its parent).
   The SDK of RISC OS 64. */
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Called by the calls of the SDK that are not implemented (they fail with ENOSYS): with the system variable UL64_TraceNosys set, the name goes to the error stream - the quickest way to find which
   call a program needs. */
void __ul64_nosys (const char *name)
{
  if (getenv ("UL64_TraceNosys")) { fputs ("[ENOSYS: ", stderr); fputs (name, stderr); fputs ("]\n", stderr); }
  errno = ENOSYS;
}

size_t confstr (int name, char *buf, size_t len)
{
  const char *s = 0;
  if (name == _CS_PATH) s = "";				/* RISC OS looks along Run$Path itself */
  if (!s) { errno = EINVAL; return 0; }
  if (len) { strncpy (buf, s, len); buf[len - 1] = 0; }
  return strlen (s) + 1;
}
/* pathconf: the limits of the file system of RISC OS 64 as the SDK implements them (a path up to 1024 characters, a leaf name up to 255) */
long pathconf (const char *p, int name)
{
  (void) p;
  switch (name)
    {
    case _PC_LINK_MAX: return 1;
    case _PC_NAME_MAX: return 255;
    case _PC_PATH_MAX: return 1024;
    case _PC_PIPE_BUF: return 4096;
    case _PC_NO_TRUNC: return 1;
    default: errno = EINVAL; return -1;
    }
}
long fpathconf (int fd, int name) { (void) fd; return pathconf ("/", name); }
char *secure_getenv (const char *n) { return getenv (n); }
int eaccess (const char *p, int m) { return access (p, m); }
int getentropy (void *buf, size_t n)
{
  unsigned char *b = (unsigned char *) buf;
  size_t i;
  if (n > 256) { errno = EIO; return -1; }
  for (i = 0; i < n; i++) b[i] = (unsigned char) rand ();	/* not secure: there is no entropy source yet */
  return 0;
}
int getloadavg (double *l, int n) { int i; for (i = 0; i < n; i++) l[i] = 0; return n; }
int pselect (int n, fd_set *r, fd_set *w, fd_set *e, const struct timespec *ts, const sigset_t *m)
{
  struct timeval tv;
  (void) m;
  if (ts) { tv.tv_sec = ts->tv_sec; tv.tv_usec = ts->tv_nsec / 1000; }
  return select (n, r, w, e, ts ? &tv : 0);
}
int madvise (void *a, size_t l, int adv) { (void) a; (void) l; (void) adv; return 0; }
int mkfifo (const char *p, mode_t m) { (void) p; (void) m; __ul64_nosys ("mkfifo"); return -1; }
long syscall (long n, ...) { (void) n; __ul64_nosys ("syscall"); return -1; }

int fputc_unlocked (int c, FILE *f) { return fputc (c, f); }
int fputs_unlocked (const char *s, FILE *f) { return fputs (s, f); }
size_t fread_unlocked (void *p, size_t s, size_t n, FILE *f) { return fread (p, s, n, f); }
size_t fwrite_unlocked (const void *p, size_t s, size_t n, FILE *f) { return fwrite (p, s, n, f); }
int fileno_unlocked (FILE *f) { return fileno (f); }

struct __posix_spawnattr { int flags; sigset_t sigmask, sigdefault; pid_t pgroup; };
struct __posix_spawn_file_actions { int n; };
int posix_spawnattr_init (posix_spawnattr_t *a) { *a = (posix_spawnattr_t) calloc (1, sizeof **a); return *a ? 0 : ENOMEM; }
int posix_spawnattr_destroy (posix_spawnattr_t *a) { free (*a); *a = 0; return 0; }
int posix_spawnattr_setflags (posix_spawnattr_t *a, short f) { (*a)->flags = f; return 0; }
int posix_spawnattr_setsigmask (posix_spawnattr_t *a, const sigset_t *m) { (*a)->sigmask = *m; return 0; }
int posix_spawnattr_getsigmask (const posix_spawnattr_t *a, sigset_t *m) { *m = (*a)->sigmask; return 0; }
int posix_spawnattr_setsigdefault (posix_spawnattr_t *a, const sigset_t *m) { (*a)->sigdefault = *m; return 0; }
int posix_spawnattr_getsigdefault (const posix_spawnattr_t *a, sigset_t *m) { *m = (*a)->sigdefault; return 0; }
int posix_spawnattr_getflags (const posix_spawnattr_t *a, short *f) { *f = (short) (*a)->flags; return 0; }
int posix_spawnattr_setpgroup (posix_spawnattr_t *a, pid_t p) { (*a)->pgroup = p; return 0; }
int posix_spawnattr_getpgroup (const posix_spawnattr_t *a, pid_t *p) { *p = (*a)->pgroup; return 0; }
int posix_spawn_file_actions_addopen (posix_spawn_file_actions_t *a, int fd, const char *path, int flags, mode_t mode) { (void) fd; (void) path; (void) flags; (void) mode; (*a)->n++; return 0; }
int posix_spawn_file_actions_addchdir_np (posix_spawn_file_actions_t *a, const char *path) { (void) path; (*a)->n++; return 0; }
int posix_spawn_file_actions_init (posix_spawn_file_actions_t *a) { *a = (posix_spawn_file_actions_t) calloc (1, sizeof **a); return *a ? 0 : ENOMEM; }
int posix_spawn_file_actions_destroy (posix_spawn_file_actions_t *a) { free (*a); *a = 0; return 0; }
int posix_spawn_file_actions_addclose (posix_spawn_file_actions_t *a, int fd) { (void) fd; (*a)->n++; return 0; }
int posix_spawn_file_actions_adddup2 (posix_spawn_file_actions_t *a, int fd, int nfd) { (void) fd; (void) nfd; (*a)->n++; return 0; }

int get_nprocs (void) { return 1; }
int get_nprocs_conf (void) { return 1; }
