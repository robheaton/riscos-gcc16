/* tickerbug.c - does UnixLib's thread ticker fire into another task?  A Wimp task with a second thread.
   usage: tickerbug early|late LOGFILE
     early: the thread is created BEFORE Wimp_Initialise (as SDL programs do): UnixLib's ticker starts with no Wimp filters (the task handle is still 0)
     late:  the thread is created AFTER Wimp_Initialise (the filters are registered for the task)
   The program polls with Wimp_PollIdle (it sleeps for 0.5 s at a time, so other tasks run while it is paged out) and writes a line to LOGFILE every second.
   After 120 seconds it closes down by itself and writes SURVIVED.  A click on its icon, or Message_Quit, also ends it. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <kernel.h>

#define Wimp_Initialise  0x400C0
#define Wimp_CloseDown   0x400DD
#define Wimp_CreateIcon  0x400C2
#define Wimp_PollIdle    0x400E1
#define OS_ReadMonotonicTime 0x42

static volatile unsigned long counter, stop;
static FILE *log_;

static void *worker (void *p)
{
  (void) p;
  while (!stop) counter++;
  return 0;
}

static unsigned now (void)
{
  _kernel_swi_regs r = {{0}};
  _kernel_swi (OS_ReadMonotonicTime, &r, &r);
  return r.r[0];
}

static void say (const char *s, unsigned long a, unsigned long b)
{
  if (!log_) return;
  fprintf (log_, "%s %lu %lu\n", s, a, b);
  fflush (log_);
}

int main (int argc, char **argv)
{
  int early = argc > 1 && !strcmp (argv[1], "early");
  const char *logname = argc > 2 ? argv[2] : "/<Wimp$ScrapDir>/TickLog";
  pthread_t th;
  _kernel_swi_regs r = {{0}};
  unsigned t0, next_log, polls = 0;
  int task = 0, running = 1;
  unsigned block[64];
  unsigned icon[9];

  log_ = fopen (logname, "w");
  if (!log_) log_ = fopen ("/<Wimp$ScrapDir>/TickLog", "w");
  say (early ? "start early" : "start late", 0, 0);
  if (early && pthread_create (&th, 0, worker, 0)) { say ("pthread_create failed", 0, 0); return 1; }

  r.r[0] = 310; r.r[1] = 0x4B534154; r.r[2] = (int) "TickerBug"; r.r[3] = 0;
  if (_kernel_swi (Wimp_Initialise, &r, &r)) { say ("Wimp_Initialise failed", 0, 0); return 1; }
  task = r.r[1];
  say ("Wimp_Initialise task", task, 0);
  if (!early && pthread_create (&th, 0, worker, 0)) { say ("pthread_create failed", 0, 0); return 1; }

  icon[0] = (unsigned) -1; icon[1] = 0; icon[2] = 0; icon[3] = 100; icon[4] = 68;      /* the icon bar, right hand side */
  icon[5] = 0x1700001D;                                                              /* text, border, centred, black on white */
  memcpy (&icon[6], early ? "TickEarly\0\0\0" : "TickLate\0\0\0\0", 12);
  r.r[0] = 0; r.r[1] = (int) icon;
  _kernel_swi (Wimp_CreateIcon, &r, &r);

  t0 = now (); next_log = t0 + 100;
  while (running)
    {
      unsigned t, ev;
      r.r[0] = 0; r.r[1] = (int) block; r.r[2] = now () + 50;                         /* sleep up to 0.5 s */
      if (_kernel_swi (Wimp_PollIdle, &r, &r)) { say ("Wimp_PollIdle error", 0, 0); break; }
      ev = r.r[0]; polls++;
      if (ev == 6) running = 0;                                                        /* a click on the icon */
      else if ((ev == 17 || ev == 18) && block[4] == 0) running = 0;                    /* Message_Quit */
      t = now ();
      if (t >= next_log) { say ("t=", t - t0, counter); next_log += 100; }
      if (t - t0 > 12000) { say ("SURVIVED 120 s: polls, counter", polls, counter); running = 0; }
    }
  stop = 1;
  r.r[0] = task; r.r[1] = 0x4B534154;
  _kernel_swi (Wimp_CloseDown, &r, &r);
  say ("closed down", polls, counter);
  if (log_) fclose (log_);
  return 0;
}
