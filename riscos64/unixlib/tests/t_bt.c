/* t_bt.c - backtrace () and the report of a fatal signal.  Compile with -fno-omit-frame-pointer -Wl,-Map=t_bt.map; the job ul_bt runs "t_bt" (the frames of backtrace () as addresses) and "t_bt segv" (the
   report of a fault); tools/crashsym.py names the addresses.   t_bt names: print the addresses followed by the name of the function (checked on the host against the map) */
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *frames[16];
int nframes;
__attribute__ ((noinline)) void level_c (int segv)
{
  if (segv) { volatile int *p = (volatile int *) 16; *p = 1; }
  nframes = backtrace (frames, 16);
  __asm__ volatile ("");
}
__attribute__ ((noinline)) void level_b (int segv) { level_c (segv); __asm__ volatile (""); }
__attribute__ ((noinline)) void level_a (int segv) { level_b (segv); __asm__ volatile (""); }

int main (int argc, char **argv)
{
  int i;
  level_a (argc > 1 && !strcmp (argv[1], "segv"));
  printf ("frames %d\n", nframes);
  for (i = 0; i < nframes; i++) printf ("  0x%016lx\n", (unsigned long) frames[i]);
  backtrace_symbols_fd (frames, nframes < 3 ? nframes : 3, 1);
  return 0;
}
