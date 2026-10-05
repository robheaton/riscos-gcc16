/* exittest.c 1.1 -- every way a process can end, and what each leaves of the range of address space that ALL EABI stacks share ("UnixLib stacks").  For libunixlib 16.2.0-7 (fix level 9), which gives
   back the one page signal stack of a process when the process ends; before, 4 KB per process stayed in use until the ROOT process (the one without a parent) ended.
   usage: exittest            the series: this root process starts one child ("exittest child KIND") per run, one after the other, four runs of each KIND, and after each KIND works out what is
                              left of the stack range in use while this root is still alive (the first run is a warm-up; the other three must leave the same amount)
          exittest child KIND   one kind of exit (started by the series; KIND = one of the names below)
   KINDs and what must be true (leak: "nothing" = the stack range in use does not grow; "0 or 4" = at most 4 KB per run, because the free is skipped on purpose when the exit runs ON the signal stack):
     return0      main returns 0                                          status 0, leaves nothing
     exit3        exit (3)                                                status 3, leaves nothing
     _Exit4       _Exit (4) (ISO C: the status is encoded for you)        status 4, leaves nothing
     _exit0       _exit (0): UnixLib's _exit takes an already ENCODED wait status (see below), 0 is exit code 0     status 0, leaves nothing
     abort        abort (): ends with SIGABRT                             killed by SIGABRT, leaves 0 or 4
     fault_fatal  a real fault (a store to address 16), default action     killed by a signal (UnixLib says SIGEMT), leaves 0 or 4: it ends on the signal stack
     fault_caught the same fault, caught: the handler runs ON the signal stack and siglongjmps out; the program goes on and returns 0     status 0, leaves 0 or 4
     fault_exit   the same fault, the handler calls exit (5) (ON the signal stack: it must not be freed from there)    status 5, leaves 0 or 4
     thread_exit  exit (6) called by another thread                        status 6; the amount left is only reported (the thread's own stack is not what this tests)
     vfork_exec   a vfork child that execs "exittest child return0"; afterwards a caught fault here (the parent's signal stack, shared with the child until it exec'd, must still be there)   status 0, leaves nothing
     exec_self    execv of "exittest child return0" in place, no fork: the old image's signal stack goes at the exec     status 0; the amount left is only reported (the old image's main stack is not freed at an exec either)
   Not tested, on purpose, two things found by the first run of this test (both are in UnixLib / SharedUnixLibrary, not in libunixlib 16.2.0-7):
     - a vfork child that ends with _exit () WITHOUT exec: SharedUnixLibrary frees the PARENT's main stack (sul_fork copies the parent's process structure, PROC_STACK too, and sul_exit frees it), and the
       parent dies on its next stack access ("Internal error: abort on data transfer ... fork_common").  vforkbare.c shows it.
     - alarm () and the interval timers: not available in a Task window (setitimer returns ENOSYS there), so no ASYNCHRONOUS signal can be had; a real fault gives a handler on the signal stack as well.
   _exit (n) is NOT the POSIX function: it takes the 16-bit encoded wait status of <sys/wait.h> (exit code in bits 8-15, signal in bits 0-6), so _exit (4) is "killed by signal 4" and _exit (127) is exit code 0.
   The checks need libunixlib fix level 9; on an older one the "leaves nothing" kinds FAIL by 4 KB per process (that is the control).  Prints ok / FAIL lines and a SUMMARY; returns 1 on a failure. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "dautil.h"

#ifndef ROTEST_CFG
#define ROTEST_CFG "exittest"
#endif
#ifndef SIGEMT                                 /* not on Linux (the host run of the logic): the fault there is SIGSEGV anyway */
#define SIGEMT SIGSEGV
#endif

/* ---- a real fault, caught: UnixLib turns the data abort into a signal (SIGEMT for a store to address 16; SIGSEGV, SIGBUS and SIGILL are caught too) and runs the handler ON its signal stack ---- */
static sigjmp_buf fault_jb;
static volatile sig_atomic_t fault_signal;
static void on_fault_jump(int s) { fault_signal = s; siglongjmp(fault_jb, 1); }
static void on_fault_exit(int s) { (void) s; exit(5); }
static const int fault_signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGEMT };
static void catch_faults(void (*h)(int))
{
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = h;
  for (unsigned i = 0; i < sizeof fault_signals / sizeof fault_signals[0]; i++) sigaction(fault_signals[i], &sa, NULL);
}
static void default_faults(void)
{
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = SIG_DFL;
  for (unsigned i = 0; i < sizeof fault_signals / sizeof fault_signals[0]; i++) sigaction(fault_signals[i], &sa, NULL);
}
static __attribute__((noinline)) void do_fault(void) { volatile int *volatile bad = (volatile int *) 16; *bad = 1; }
/* 1 when the fault came and its handler ran and returned control here (by siglongjmp) */
static int take_fault(void)
{
  fault_signal = 0;
  catch_faults(on_fault_jump);
  if (sigsetjmp(fault_jb, 1) == 0) do_fault();
  default_faults();
  return fault_signal != 0;
}

static __attribute__((noinline)) int spawn(const char *prog, char *const av[], int *raw)   /* vfork + exec + wait: the child shares our memory and stack until it execs: nothing else happens here */
{
  pid_t pid = vfork();
  if (pid == 0) { execv(prog, av); _exit(127); }
  if (pid < 0) return -1;
  int st = 0;
  if (waitpid(pid, &st, 0) != pid) return -2;
  *raw = st;
  return 0;
}

static void *thread_exit_fn(void *a) { (void) a; exit(6); return NULL; }

static int child(const char *kind)
{
  if (!strcmp(kind, "return0")) return 0;
  if (!strcmp(kind, "exit3")) exit(3);
  if (!strcmp(kind, "_Exit4")) _Exit(4);
  if (!strcmp(kind, "_exit0")) _exit(0);
  if (!strcmp(kind, "abort")) abort();
  if (!strcmp(kind, "fault_fatal")) { do_fault(); return 90; }
  if (!strcmp(kind, "fault_caught")) return take_fault() ? 0 : 91;
  if (!strcmp(kind, "fault_exit")) { catch_faults(on_fault_exit); do_fault(); return 92; }
  if (!strcmp(kind, "thread_exit")) { pthread_t t; if (pthread_create(&t, NULL, thread_exit_fn, NULL)) return 93; sleep(6); return 94; }
  if (!strcmp(kind, "vfork_exec")) {
    char *av[] = { "exittest", "child", "return0", NULL };
    int st = 0;
    if (spawn("exittest", av, &st) || !WIFEXITED(st) || WEXITSTATUS(st) != 0) return 95;
    return take_fault() ? 0 : 96;                                      /* our signal stack must still be there for the handler */
  }
  if (!strcmp(kind, "exec_self")) { char *av[] = { "exittest", "child", "return0", NULL }; execv("exittest", av); return 97; }
  fprintf(stderr, "exittest: unknown kind %s\n", kind);
  return 99;
}

static unsigned stacks_kb(void)
{
  struct da_info s;
  return da_find("UnixLib stacks", &s) ? 0 : s.size >> 10;
}

struct kind { const char *name, *what; int want_exit, want_sig; int leak; };   /* want_sig: 0 = exit status, -1 = any signal; leak: 0 nothing, 1 nothing or 4 KB per run, 2 not judged */
static const struct kind kinds[] = {
  { "return0",     "main returns 0",                                                              0, 0,       0 },
  { "exit3",       "exit (3)",                                                                    3, 0,       0 },
  { "_Exit4",      "_Exit (4)",                                                                   4, 0,       0 },
  { "_exit0",      "_exit (0) (UnixLib's _exit takes an encoded wait status: 0 = exit code 0)",   0, 0,       0 },
  { "abort",       "abort (): SIGABRT",                                                           0, SIGABRT, 1 },
  { "fault_fatal", "a real fault, default action (ends ON the signal stack)",                     0, -1,      1 },
  { "fault_caught","a real fault caught: the handler runs on the signal stack, siglongjmp out",    0, 0,       1 },
  { "fault_exit",  "a real fault, the handler calls exit (5) (ON the signal stack)",              5, 0,       1 },
  { "thread_exit", "exit (6) called by another thread",                                           6, 0,       2 },
  { "vfork_exec",  "vfork child execs a program, then a caught fault here (our signal stack)",    0, 0,       0 },
  { "exec_self",   "execv in place (no fork)",                                                    0, 0,       2 },
};

int main(int argc, char **argv)
{
  if (argc > 2 && !strcmp(argv[1], "child")) return child(argv[2]);
  long lvl = sysconf(0x4700);
  int checks = 0, fails = 0;
#define CHECK(c, ...) do { checks++; if (c) printf("  ok   "); else { fails++; printf("  FAIL "); } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
  printf("exittest 1.1 [%s]\n", ROTEST_CFG);
  CHECK(lvl >= 9, "libunixlib fix level %ld (9 or more frees the signal stack at exit; an older one is the control: the \"leaves nothing\" kinds are expected to FAIL)", lvl);
  struct da_info s;
  if (da_find("UnixLib stacks", &s)) { printf("no dynamic area called \"UnixLib stacks\": nothing to measure\n"); return 2; }
  printf("the shared stack range (\"UnixLib stacks\") has %u KB in use now, with this root process alive\n", s.size >> 10);
  { int raw = 0; char *av[] = { "exittest", "child", "return0", NULL }; if (spawn("exittest", av, &raw)) { printf("cannot start \"exittest\": run this from the folder it is in\n"); return 2; } }   /* warm-up: the root's own stack settles */
  for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
    const struct kind *kd = &kinds[k];
    unsigned kb1 = 0, kb4 = 0; int badstatus = 0; int lastraw = 0;
    for (int run = 1; run <= 4; run++) {
      char *av[] = { "exittest", "child", (char *) kd->name, NULL };
      int raw = 0, r = spawn("exittest", av, &raw);
      lastraw = raw;
      int good = r == 0 && (kd->want_sig == 0 ? (WIFEXITED(raw) && WEXITSTATUS(raw) == kd->want_exit)
                          : kd->want_sig == -1 ? WIFSIGNALED(raw) : (WIFSIGNALED(raw) && WTERMSIG(raw) == kd->want_sig));
      if (!good) badstatus++;
      if (run == 1) kb1 = stacks_kb();
      if (run == 4) kb4 = stacks_kb();
    }
    unsigned grew = kb4 > kb1 ? kb4 - kb1 : 0;                           /* over three runs */
    int leak_ok = kd->leak == 2 || (kd->leak == 1 ? grew <= 12 : grew == 0);
    char how[64];
    if (WIFEXITED(lastraw)) snprintf(how, sizeof how, "status %d", WEXITSTATUS(lastraw)); else if (WIFSIGNALED(lastraw)) snprintf(how, sizeof how, "signal %d", WTERMSIG(lastraw)); else snprintf(how, sizeof how, "raw status %d", lastraw);
    CHECK(badstatus == 0 && leak_ok, "%-12s %s: %s in each of 4 runs%s; the stack range in use grew by %u KB over the last 3 runs (%u -> %u KB)%s",
          kd->name, kd->what, how, badstatus ? " (NOT the expected status in some runs)" : "", grew, kb1, kb4,
          kd->leak == 2 ? " (only reported)" : kd->leak == 1 ? " (0 or 4 KB per run is fine: the free is skipped on purpose when the exit runs on the signal stack)" : leak_ok ? "" : "   EXPECTED 0");
  }
  printf("the shared stack range has %u KB in use at the end, with this root process still alive\n", stacks_kb());
  printf("SUMMARY [exittest, fix level %ld]: %d checks, %d failed -> %s\n", lvl, checks, fails, fails ? "FAIL" : "PASS");
  return fails != 0;
}
