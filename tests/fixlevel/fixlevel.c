/* fixlevel.c -- which libunixlib is this program using?  Prints the private fix level of the library (sysconf 0x4700; the stock library answers EINVAL) and ends with status 0 when it is the level given as the
   argument, else 1.   fixlevel 12   */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main (int argc, char **argv)
{
  errno = 0;
  long level = sysconf (0x4700);
  if (level < 0) printf ("fixlevel: this is the STOCK libunixlib (sysconf 0x4700 fails: %d)\n", errno);
  else printf ("fixlevel: libunixlib fix level %ld\n", level);
  long want = argc > 1 ? atol (argv[1]) : -1;
  if (want >= 0) printf ("fixlevel: asked for %ld: %s\n", want, level == want ? "yes" : "NO");
  return (want >= 0 && level != want) ? 1 : 0;
}
