/* vforktrace.c -- the steps of a loop of vfork children, one at a time, each LOGGED TO A FILE with raw SWIs (OS_Find open for update, OS_Args to the end, OS_GBPB write, OS_Find close) so that the
   log is on the disc/NAS when the machine freezes and the LAST line says which operation it did not survive.  (A Spool file stays 1024 bytes of zeros when the machine has to be reset; this does not.)
   Why: RunSul3 (2026-10-03 20:21, libunixlib 16.2.0-7 + SharedULib 1.16-vforkfix1) and RunSul6 stage c (21:17, libunixlib 16.2.0-8 + 1.16-vforkfix2: "seqtest 1 vforkloop 2 bare") froze the whole machine
   (pointer and desktop clock dead) as soon as they started, although ONE child that ends without exec is clean (vforkrma: RMA, Wimp slot, the pthread block: all unchanged).  What vforkloop does that vforkrma does not:
   a vfork + exec AFTER a child that ended without exec, a SECOND child that ends without exec, and a printf between them.
   usage: vforktrace LOGFILE [STEPS]        LOGFILE = a RISC OS path (e.g. <Sul$Dir>.Trace1); STEPS = a string of
          N  a vfork child that ends with _exit (0) without exec         E  a vfork child that execs this program again (vforktrace --noop) and is reaped
          D  a vfork child that execs  daprobe -m  (as vforkloop does)    P  a printf + fflush of one line (no fork)
          default "NPENPE" = two rounds of what  vforkloop 2 bare  does (with the program itself in place of daprobe for E).  vforktrace --noop LOGFILE TAG: the exec'd child: logs and ends.
   Every step writes a line with the state around it: the monotonic time, sp, the RMA free space (OS_Module 5), the Wimp slot, the application space and memory limits, the VFP context that is active
   (VFPSupport_ActiveContext), the CallBack handler's buffer (UnixLib's pthread block), the exit and error handlers.  The child writes plain lines only (no formatting, nothing of UnixLib but _swix).
   NOT to be run on a machine that cannot be reset: the point of it is that it may freeze it. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <kernel.h>
#include <swis.h>

static const char *logname;

static void put(const char *s)                       /* append S to the log file: open, go to the end, write, close; nothing of UnixLib is used but _swix and strlen */
{
  unsigned h = 0, ext = 0;
  if (_swix(OS_Find, _INR(0, 1) | _OUT(0), 0xC0, logname, &h) != NULL || h == 0) return;
  _swix(OS_Args, _INR(0, 1) | _OUT(2), 2, h, &ext);
  _swix(OS_Args, _INR(0, 2), 1, h, ext);
  _swix(OS_GBPB, _INR(0, 3), 2, h, s, (int) strlen(s));
  _swix(OS_Find, _INR(0, 1), 0, h);
}

static void state(const char *tag)                   /* parent only: formats */
{
  char b[420];
  unsigned lg = 0, rma = 0, slot = 0, nx = 0, pool = 0, app = 0, mem = 0, vfp = 0, cb1 = 0, cb2 = 0, cb3 = 0, ex = 0, er = 0, t = 0;
  _swix(OS_Module, _IN(0) | _OUTR(2, 3), 5, &lg, &rma);
  _swix(Wimp_SlotSize, _INR(0, 1) | _OUTR(0, 2), -1, -1, &slot, &nx, &pool);
  _swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(1), 14, 0, 0, 0, &app);
  _swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(1), 0, 0, 0, 0, &mem);
  _swix(OS_ChangeEnvironment, _INR(0, 3) | _OUTR(1, 3), 7, 0, 0, 0, &cb1, &cb2, &cb3);
  _swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(1), 11, 0, 0, 0, &ex);
  _swix(OS_ChangeEnvironment, _INR(0, 3) | _OUT(1), 6, 0, 0, 0, &er);
  _swix(0x58EC6, _OUT(0), &vfp);                      /* VFPSupport_ActiveContext */
  _swix(OS_ReadMonotonicTime, _OUT(0), &t);
  snprintf(b, sizeof b, "%s | t=%u sp=%08x rma=%u slot=%u app=%08x mem=%08x vfp=%08x cb=%08x/%08x/%08x exit=%08x err=%08x\n", tag, t, (unsigned) (uintptr_t) __builtin_frame_address(0), rma, slot, app, mem, vfp,
           cb1, cb2, cb3, ex, er);
  put(b);
}

static __attribute__((noinline)) int step_noexec(void)
{
  pid_t pid = vfork();
  if (pid == 0) { put("   child (no exec): running, about to _exit (0)\n"); _exit(0); }
  state("   parent resumed after the child that ended without exec");
  int st = 0; pid_t w = waitpid(pid, &st, 0);
  state("   after waitpid");
  return w == pid ? 0 : -2;
}

static __attribute__((noinline)) int step_exec(const char *log, int n, int daprobe)
{
  char tag[40];
  snprintf(tag, sizeof tag, "step %d: the exec'd child", n);
  char *av_self[] = { "vforktrace", "--noop", (char *) log, tag, NULL };
  char *av_dap[] = { "daprobe", "-m", NULL };
  pid_t pid = vfork();
  if (pid == 0) {
    put("   child (exec): about to execv\n");
    if (daprobe) execv("daprobe", av_dap); else execv("vforktrace", av_self);
    put("   child (exec): execv RETURNED\n"); _exit(127);
  }
  state("   parent resumed after the exec child");
  int st = 0; pid_t w = waitpid(pid, &st, 0);
  state("   after waitpid");
  return w == pid ? 0 : -2;
}

int main(int argc, char **argv)
{
  if (argc > 3 && !strcmp(argv[1], "--noop")) { logname = argv[2]; put("   "); put(argv[3]); put(": running, ends now\n"); return 0; }
  if (argc < 2) { fprintf(stderr, "usage: vforktrace LOGFILE [STEPS: N E D P]\n"); return 2; }
  logname = argv[1];
  const char *seq = argc > 2 ? argv[2] : "NPENPE";
  unsigned h = 0;
  if (_swix(OS_Find, _INR(0, 1) | _OUT(0), 0x80, logname, &h) != NULL || h == 0) { fprintf(stderr, "vforktrace: cannot create %s\n", logname); return 3; }
  _swix(OS_Find, _INR(0, 1), 0, h);                  /* an empty log */
  char hdr[200];
  snprintf(hdr, sizeof hdr, "vforktrace: steps %s, libunixlib fix level %ld\n", seq, sysconf(0x4700));
  put(hdr);
  unsigned len = 0;                                  /* the log must really be written before anything risky is done: OS_File 5 gives its length */
  if (_swix(OS_File, _INR(0, 1) | _OUT(4), 5, logname, &len) != NULL || len == 0) { fprintf(stderr, "vforktrace: the log %s cannot be written (length %u): not going on\n", logname, len); return 4; }
  printf("vforktrace: the log %s works (%u bytes after the first line); starting the steps %s\n", logname, len, seq); fflush(stdout);
  state("start");
  for (int i = 0; seq[i]; i++) {
    char tag[60]; int r = 0;
    snprintf(tag, sizeof tag, "step %d (%c): before", i + 1, seq[i]); state(tag);
    switch (seq[i]) {
      case 'N': r = step_noexec(); break;
      case 'E': r = step_exec(logname, i + 1, 0); break;
      case 'D': r = step_exec(logname, i + 1, 1); break;
      case 'P': printf("--- vforktrace step %d: a line of text, as vforkloop prints between the children\n", i + 1); fflush(stdout); break;
      default: r = -9; break;
    }
    snprintf(tag, sizeof tag, "step %d (%c): done rc=%d", i + 1, seq[i], r); state(tag);
  }
  put("vforktrace: the sequence is finished; the program ends by return\n");
  state("end");
  return 0;
}
