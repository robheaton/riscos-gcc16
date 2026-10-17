/* exp_env - E1, E5, E7 of the process model design: what OS_ChangeEnvironment (&40) gives for each handler (read only: R1 = 0), OS_GetEnv, and where the program's image, heap and stack are. */
#include <stdio.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern char __heap_start[];
extern unsigned long __riscos_ramlimit;
int main (void)
{
  int h;
  unsigned long r[10];
  char probe;
  r[0] = r[1] = r[2] = 0;
  if (!swi_x (0x10, r)) printf ("OS_GetEnv: command %p, memory limit %#lx, time %#lx\n", (void *) r[0], r[1], r[2]);
  printf ("heap start (end of the image) %p, ram limit %#lx, a local variable at %p\n", (void *) __heap_start, __riscos_ramlimit, (void *) &probe);
  for (h = 0; h <= 17; h++)
    {
      unsigned long q[10] = { (unsigned long) h, 0, 0, 0 };
      unsigned long e = swi_x (0x40, q);			/* OS_ChangeEnvironment, R1 = 0: read */
      if (e) printf ("handler %2d: error &%X %s\n", h, ((unsigned *) q[0])[0], (char *) q[0] + 4);
      else printf ("handler %2d: r1 = %#lx r2 = %#lx r3 = %#lx\n", h, q[1], q[2], q[3]);
    }
  return 1;
}
