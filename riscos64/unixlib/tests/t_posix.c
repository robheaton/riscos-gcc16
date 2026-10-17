/* t_posix - UnixLib 64 / the SDK: the process and memory calls: environment, ids, uname, sleeping, mmap, limits, and the file calls with an equivalent (ftruncate, truncate, chmod, utime, realpath, fcntl).
   Prints PASS/FAIL lines and a summary; exit code 1 so that the runner shows the output.  Built with  aarch64-riscos-gcc -munixlib t_posix.c */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <signal.h>

/* picolibc declares these only under feature macros that a single-threaded build does not set */
extern int clearenv (void);
extern int sched_yield (void);
extern int utime (const char *, const struct utimbuf *);

static int fails;
static volatile int got_sig;
static void on_sig (int sig) { got_sig = sig; }
#define CHECK(c, what) do { int ok_ = (c); printf ("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) { fails++; printf ("   errno %d (%s)\n", errno, strerror (errno)); } } while (0)

static double now (void) { struct timespec t; clock_gettime (CLOCK_MONOTONIC, &t); return (double) t.tv_sec + t.tv_nsec / 1e9; }

int main (int argc, char **argv)
{
  struct utsname u;
  (void) argc;
  struct rlimit rl;
  char host[64], *p, *rp;
  double t0;
  struct stat st;
  int fd;
  char *m;
  struct utimbuf ub;

  printf ("argv[0] = %s\n", argv[0]);
  /* the environment: setenv and getenv agree; a system variable is found too; unsetenv and clearenv */
  CHECK (setenv ("UL64_TEST", "hello", 1) == 0 && getenv ("UL64_TEST") && strcmp (getenv ("UL64_TEST"), "hello") == 0, "setenv / getenv");
  CHECK (setenv ("UL64_TEST", "again", 0) == 0 && strcmp (getenv ("UL64_TEST"), "hello") == 0, "setenv without overwrite keeps the value");
  CHECK (setenv ("UL64_TEST", "again", 1) == 0 && strcmp (getenv ("UL64_TEST"), "again") == 0, "setenv with overwrite");
  CHECK (putenv ("UL64_PUT=42") == 0 && strcmp (getenv ("UL64_PUT"), "42") == 0, "putenv");
  CHECK (unsetenv ("UL64_PUT") == 0 && getenv ("UL64_PUT") == NULL, "unsetenv");
  p = getenv ("Sys$Time");								/* a system variable that is always there on RISC OS */
  CHECK (p == NULL || *p, "getenv of a system variable does not crash");
  CHECK (getenv ("UL64_NOT_SET") == NULL, "getenv of a name that is not set");
  CHECK (clearenv () == 0 && getenv ("UL64_TEST") == NULL, "clearenv");

  /* who */
  CHECK (getuid () == geteuid () && getgid () == getegid (), "ids agree");
  CHECK (gethostname (host, sizeof host) == 0 && host[0], "gethostname");
  CHECK (uname (&u) == 0 && strcmp (u.sysname, "RISC OS") == 0 && strcmp (u.machine, "aarch64") == 0, "uname");
  printf ("host %s, release %s, version %s\n", host, u.release, u.version);

  /* time */
  t0 = now ();
  usleep (250000);
  CHECK (now () - t0 >= 0.2 && now () - t0 < 1.5, "usleep (250000) takes about a quarter of a second");
  t0 = now ();
  { struct timespec ts = { 0, 120000000L }; CHECK (nanosleep (&ts, NULL) == 0 && now () - t0 >= 0.1, "nanosleep 120 ms"); }
  CHECK (sched_yield () == 0, "sched_yield");
  { struct rusage ru; CHECK (getrusage (RUSAGE_SELF, &ru) == 0, "getrusage"); }

  /* limits and memory */
  CHECK (getrlimit (RLIMIT_STACK, &rl) == 0 && rl.rlim_cur > 65536, "getrlimit stack");
  m = (char *) mmap (NULL, 100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  CHECK (m != MAP_FAILED && ((unsigned long) m & 4095) == 0, "mmap anonymous, page aligned");
  if (m != MAP_FAILED) { int ok = 1; size_t i; for (i = 0; i < 100000; i++) if (m[i]) ok = 0; memset (m, 7, 100000); CHECK (ok && m[99999] == 7, "the block is zero and writable"); CHECK (munmap (m, 100000) == 0, "munmap"); }

  /* files */
  fd = open ("ulposix.dat", O_RDWR | O_CREAT | O_TRUNC, 0666);
  CHECK (fd >= 0 && write (fd, "0123456789", 10) == 10, "create a file of 10 bytes");
  CHECK (fsync (fd) == 0, "fsync");
  CHECK (ftruncate (fd, 4) == 0, "ftruncate to 4");
  close (fd);
  CHECK (stat ("ulposix.dat", &st) == 0 && st.st_size == 4, "the file is 4 bytes");
  CHECK (truncate ("ulposix.dat", 2) == 0 && stat ("ulposix.dat", &st) == 0 && st.st_size == 2, "truncate to 2");
  CHECK (chmod ("ulposix.dat", 0444) == 0 && stat ("ulposix.dat", &st) == 0 && !(st.st_mode & 0222), "chmod 0444: not writable");
  CHECK (chmod ("ulposix.dat", 0644) == 0 && stat ("ulposix.dat", &st) == 0 && (st.st_mode & 0222), "chmod 0644: writable");
  ub.actime = 0; ub.modtime = 1000000000;							/* 9 September 2001 */
  CHECK (utime ("ulposix.dat", &ub) == 0 && stat ("ulposix.dat", &st) == 0 && st.st_mtime == 1000000000, "utime sets the modification time");
  rp = realpath ("ulposix.dat", NULL);
  CHECK (rp != NULL && strstr (rp, "ulposix") != NULL, "realpath");
  printf ("realpath: %s\n", rp ? rp : "(null)");
  free (rp);
  CHECK (fcntl (1, F_GETFD) == 0 && ioctl (1, 0x5401, NULL) == -1 && errno == ENOTTY, "fcntl F_GETFD; ioctl on a non-tty: ENOTTY");
  CHECK (dup (1) == -1 && errno == ENOSYS, "dup: ENOSYS");
  CHECK (unlink ("ulposix.dat") == 0, "unlink");
  /* signals: no asynchronous delivery yet, but raise, kill (getpid ()) and sigaction work on the handler table */
  CHECK (signal (SIGUSR1, on_sig) == SIG_DFL, "signal: the old handler is SIG_DFL");
  got_sig = 0; CHECK (raise (SIGUSR1) == 0 && got_sig == SIGUSR1, "raise calls the handler");
  { struct sigaction sa; memset (&sa, 0, sizeof sa); sa.sa_handler = on_sig; got_sig = 0;
    CHECK (sigaction (SIGUSR2, &sa, NULL) == 0 && kill (getpid (), SIGUSR2) == 0 && got_sig == SIGUSR2, "sigaction and kill (getpid ())"); }
  signal (SIGUSR1, SIG_IGN); got_sig = 0; CHECK (raise (SIGUSR1) == 0 && got_sig == 0, "an ignored signal does nothing");
  CHECK (kill (999999, SIGTERM) == -1 && errno == ESRCH, "kill of another process: ESRCH");
  printf ("SUMMARY %d failed\n", fails);
  return 1;
}
