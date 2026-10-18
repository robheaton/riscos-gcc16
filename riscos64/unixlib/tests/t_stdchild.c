/* t_stdchild.c - a program whose standard output goes to a file (Sh$Stdout) starts another program that writes to the same file: both must get their text into it, in order (RISC OS lets a file be open for writing once, so the library
   closes the file while the child runs).  Run with  Set Sh$Stdout out_std  and  Set UnixEnv$swap 1; the file must read: parent one / child says hi / parent two / child says hi (popen) / parent three. */
#include <stdio.h>
#include <stdlib.h>
int main (int argc, char **argv)
{
  (void) argc; (void) argv;
  puts ("parent one"); fflush (stdout);
  system ("t_say");
  puts ("parent two"); fflush (stdout);
  { FILE *p = popen ("t_say", "w"); if (p) pclose (p); }
  puts ("parent three");
  return 0;
}
