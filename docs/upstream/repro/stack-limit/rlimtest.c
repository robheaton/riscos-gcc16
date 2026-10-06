/* rlimtest [EXPECTED]: does getrlimit (RLIMIT_STACK) tell the truth?  The recursion below uses 75% of the reported soft limit (at most 8MB) in 1KB frames.
   EXPECTED is the size in bytes that this program must be given (1048576 for a plain EABI program, 67108864 for rlimtest64: __stack_size).  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#ifdef BIGSTACK
int __stack_size = 64 * 1024 * 1024;
#endif

static int checks, fails;
static void check (int ok, const char *what) { checks++; if (!ok) fails++; printf ("  %-4s %s\n", ok ? "ok" : "FAIL", what); }

static volatile char sink;
static long deep (long n)
{
  volatile char frame[1000];
  frame[0] = (char) n;
  frame[999] = (char) (n >> 3);
  if (n <= 0) return frame[0] + frame[999];
  return deep (n - 1) + (long) frame[0];
}

int main (int argc, char **argv)
{
  struct rlimit r, back;
  unsigned long cur, depth;
  int rc = getrlimit (RLIMIT_STACK, &r);

  check (rc == 0, "getrlimit (RLIMIT_STACK) succeeds");
  printf ("  RLIMIT_STACK: cur = %lu, max = %lu\n", (unsigned long) r.rlim_cur, (unsigned long) r.rlim_max);
  check (r.rlim_cur >= 1048576 || r.rlim_cur == RLIM_INFINITY, "the limit is at least 1MB");
  if (argc > 1)
    {
      unsigned long want = strtoul (argv[1], NULL, 10);
      check (r.rlim_cur == want, "the limit is the size of the main stack of this program");
    }
  cur = r.rlim_cur == RLIM_INFINITY ? 8UL * 1024 * 1024 : r.rlim_cur;
  if (cur > 8UL * 1024 * 1024) cur = 8UL * 1024 * 1024;
  depth = cur * 3 / 4 / 1040;
  printf ("  recursing %lu frames of about 1KB (%lu KB)\n", depth, depth * 1040 / 1024);
  sink = (char) deep ((long) depth);
  check (1, "the recursion up to 75% of the limit completes (no stack overflow)");
  r.rlim_cur = 524288;
  check (setrlimit (RLIMIT_STACK, &r) == 0, "the soft limit can be lowered");
  check (getrlimit (RLIMIT_STACK, &back) == 0 && back.rlim_cur == 524288, "and reads back");
  printf ("SUMMARY [rlimtest]: %d checks, %d failed -> %s\n", checks, fails, fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
}
