/* sdktest - the system functions of the SDK that libctest does not cover: unlink, rename, mkdir, stat, access, time, gettimeofday, clock, getenv, system (picolibc on RISC OS 64). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("PASS %s\n", #c); } while (0)

int main (void)
{
  FILE *f;
  struct stat st;
  struct timeval tv;
  time_t t1, t2;
  clock_t c1, c2;
  char *e;
  printf ("RISC OS 64 test sdktest\n");
  remove ("sdkt1"); remove ("sdkt2");
  f = fopen ("sdkt1", "w"); fputs ("0123456789", f); fclose (f);
  CHECK (stat ("sdkt1", &st) == 0 && S_ISREG (st.st_mode) && st.st_size == 10 && st.st_mtime > 1700000000);
  CHECK (access ("sdkt1", F_OK) == 0 && access ("sdkt1", W_OK) == 0);
  CHECK (stat ("sdkt-nothing", &st) == -1 && errno == ENOENT);
  CHECK (rename ("sdkt1", "sdkt2") == 0);
  CHECK (stat ("sdkt1", &st) == -1 && stat ("sdkt2", &st) == 0 && st.st_size == 10);
  CHECK (rename ("sdkt-nothing", "sdkt3") == -1);
  CHECK (unlink ("sdkt2") == 0);
  CHECK (stat ("sdkt2", &st) == -1 && errno == ENOENT);
  CHECK (unlink ("sdkt2") == -1 && errno == ENOENT);
  f = fopen ("sdkt3", "w"); fclose (f);
  CHECK (remove ("sdkt3") == 0 && fopen ("sdkt3", "r") == NULL);
  CHECK (mkdir ("sdktd", 0755) == 0 && stat ("sdktd", &st) == 0 && S_ISDIR (st.st_mode));
  CHECK (unlink ("sdktd") == 0);
  t1 = time (NULL);
  CHECK (t1 > 1700000000);                                    /* after November 2023 */
  CHECK (gettimeofday (&tv, NULL) == 0 && tv.tv_sec >= t1 - 1 && tv.tv_sec <= t1 + 2 && tv.tv_usec >= 0 && tv.tv_usec < 1000000);
  { struct tm *g = gmtime (&t1); char b[40]; CHECK (g && strftime (b, sizeof b, "%Y-%m-%d %H:%M:%S", g) == 19); printf ("INFO now (UTC): %s\n", b); }
  c1 = clock ();
  { volatile unsigned long x = 0; unsigned long i; for (i = 0; i < 3000000; i++) x += i; (void) x; }
  c2 = clock ();
  printf ("INFO clock: %ld to %ld (microseconds)\n", (long) c1, (long) c2);
  CHECK (c2 >= c1);
  t2 = time (NULL);
  CHECK (t2 >= t1);
  system ("Set Sdk$Test hello from the SDK");
  e = getenv ("Sdk$Test");
  CHECK (e && !strcmp (e, "hello from the SDK"));
  CHECK (getenv ("Sdk$Nothing") == NULL);
  CHECK (system ("Echo system () works") == 0);
  printf ("SUMMARY %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
