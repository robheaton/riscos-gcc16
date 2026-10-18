/* t_compat.c - the small POSIX calls of the SDK: users and groups, inet_*, scandir, mkdtemp, getrandom, the *at calls, signals (sigpending, sigsuspend, sigwait, setitimer), clock_nanosleep. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("ok   %s\n", #c); } while (0)
static volatile int got;
static void h (int s) { got = s; }

int main (void)
{
  struct passwd *pw;
  struct group *gr;
  char buf[64];
  unsigned char a[16];
  struct in_addr ia;
  struct dirent **list;
  int n, i;
  struct sigaction sa;
  sigset_t set, pend;
  struct itimerval it;
  struct timespec ts;

  pw = getpwuid (getuid ());
  CHECK (pw && pw->pw_uid == getuid () && pw->pw_name && pw->pw_dir);
  CHECK (pw && getpwnam (pw->pw_name) && getpwnam (pw->pw_name)->pw_uid == getuid ());
  CHECK (getpwnam ("root") && getpwnam ("root")->pw_uid == 0);
  CHECK (getpwnam ("nobody-here") == NULL);
  gr = getgrgid (getgid ());
  CHECK (gr && gr->gr_gid == getgid () && getgrnam (gr->gr_name));
  CHECK (getlogin_r (buf, sizeof buf) == 0 && buf[0]);
  CHECK (getgroups (0, NULL) == 1);

  CHECK (inet_pton (AF_INET, "192.168.1.20", a) == 1 && a[0] == 192 && a[3] == 20);
  CHECK (inet_pton (AF_INET, "256.1.1.1", a) == 0 && inet_pton (AF_INET, "1.2.3", a) == 0);
  CHECK (inet_ntop (AF_INET, a, buf, sizeof buf) != NULL);
  inet_pton (AF_INET, "10.0.0.80", a);
  CHECK (!strcmp (inet_ntop (AF_INET, a, buf, sizeof buf), "10.0.0.80"));
  CHECK (inet_pton (AF_INET6, "fe80::1", a) == 1 && a[0] == 0xfe && a[1] == 0x80 && a[15] == 1 && a[2] == 0);
  CHECK (!strcmp (inet_ntop (AF_INET6, a, buf, sizeof buf), "fe80::1"));
  CHECK (inet_pton (AF_INET6, "::", a) == 1 && !strcmp (inet_ntop (AF_INET6, a, buf, sizeof buf), "::"));
  CHECK (inet_pton (AF_INET6, "1:2:3:4:5:6:7:8", a) == 1 && !strcmp (inet_ntop (AF_INET6, a, buf, sizeof buf), "1:2:3:4:5:6:7:8"));
  CHECK (inet_pton (AF_INET6, "1::2::3", a) == 0);
  CHECK (inet_aton ("127.0.0.1", &ia) == 1 && ia.s_addr == htonl (INADDR_LOOPBACK));
  CHECK (inet_addr ("127.0.0.1") == htonl (INADDR_LOOPBACK) && inet_addr ("x") == INADDR_NONE);
  CHECK (!strcmp (inet_ntoa (ia), "127.0.0.1"));
  CHECK (ntohs (htons (0x1234)) == 0x1234 && htons (0x1234) == 0x3412);

  mkdir ("tcdir", 0755);
  for (i = 0; i < 3; i++) { sprintf (buf, "tcdir/f%d", 3 - i); close (open (buf, O_WRONLY | O_CREAT, 0644)); }
  n = scandir ("tcdir", &list, NULL, alphasort);
  CHECK (n >= 3);
  for (i = 0; i < n; i++) if (!strcmp (list[i]->d_name, "f1")) break;
  CHECK (i + 2 < n && !strcmp (list[i + 1]->d_name, "f2") && !strcmp (list[i + 2]->d_name, "f3"));
  for (i = 0; i < n; i++) free (list[i]);
  free (list);
  {
    char t[] = "tcdirXXXXXX";
    struct stat st;
    CHECK (mkdtemp (t) != NULL && stat (t, &st) == 0 && S_ISDIR (st.st_mode));
    CHECK (unlinkat (AT_FDCWD, t, AT_REMOVEDIR) == 0);
  }
  CHECK (faccessat (AT_FDCWD, "tcdir/f1", R_OK, 0) == 0);
  {
    int fd = openat (AT_FDCWD, "tcdir/f1", O_RDWR);
    CHECK (fd >= 0 && posix_fallocate (fd, 0, 100) == 0 && lseek (fd, 0, SEEK_END) == 100);
    CHECK (fdatasync (fd) == 0);
    close (fd);
  }
  CHECK (unlinkat (AT_FDCWD, "tcdir/f1", 0) == 0 && unlinkat (AT_FDCWD, "tcdir/f2", 0) == 0 && unlinkat (AT_FDCWD, "tcdir/f3", 0) == 0);
  CHECK (rmdir ("tcdir") == 0);

  {
    unsigned char r1[16], r2[16];
    CHECK (getrandom (r1, 16, 0) == 16 && getrandom (r2, 16, 0) == 16 && memcmp (r1, r2, 16) != 0);
  }

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = h;
  sigaction (SIGUSR1, &sa, NULL);
  sigemptyset (&set);
  sigaddset (&set, SIGUSR1);
  sigprocmask (SIG_BLOCK, &set, NULL);
  raise (SIGUSR1);
  CHECK (got == 0 && sigpending (&pend) == 0 && sigismember (&pend, SIGUSR1));
  { int s = 0; CHECK (sigwait (&set, &s) == 0 && s == SIGUSR1 && got == 0); }
  raise (SIGUSR1);
  sigprocmask (SIG_UNBLOCK, &set, NULL);
  CHECK (got == SIGUSR1);

  got = 0;
  sa.sa_handler = h;
  sigaction (SIGALRM, &sa, NULL);
  memset (&it, 0, sizeof it);
  it.it_value.tv_usec = 200000;
  CHECK (setitimer (ITIMER_REAL, &it, NULL) == 0);
  CHECK (getitimer (ITIMER_REAL, &it) == 0 && it.it_value.tv_sec == 0 && it.it_value.tv_usec > 0);
  sigemptyset (&set);
  CHECK (sigsuspend (&set) == -1 && errno == EINTR && got == SIGALRM);

  {
    struct timespec t0, t1;
    clock_gettime (CLOCK_MONOTONIC, &t0);
    ts.tv_sec = 0; ts.tv_nsec = 100000000;
    CHECK (clock_nanosleep (CLOCK_MONOTONIC, 0, &ts, NULL) == 0);
    clock_gettime (CLOCK_MONOTONIC, &t1);
    CHECK ((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000 >= 90);
  }
  CHECK (clock_getres (CLOCK_MONOTONIC, &ts) == 0 && ts.tv_sec == 0 && ts.tv_nsec > 0);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
