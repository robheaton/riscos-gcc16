/* fixlevel -- is the UnixLib that this program uses the one the fixed SharedUnixLibrary needs?  usage: fixlevel [MINIMUM]      (default 10)
   The runtime package (SharedLibs-C-armeabihf 16.2.0-8 and later) answers sysconf (0x4700) with its private fix level.  Level 10 is the one that stopped a vfork child that ends without exec from
   freeing memory that its parent still uses (__pthread_prog_fini): with the fixed SharedUnixLibrary and a runtime BELOW it, a loop of such children can corrupt the RMA and freeze the machine.
   exit status: 0 = the fix level is MINIMUM or more      1 = it is lower, or the runtime does not answer (the stock libunixlib of GCCSDK 10.2.0)      (the status stays below 4: see sulfile.c) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char **argv)
{
  long want = argc > 1 ? strtol (argv[1], NULL, 10) : 10;
  errno = 0;
  long level = sysconf (0x4700);
  if (level < 0) { printf ("fixlevel: the runtime does not answer sysconf (0x4700) (errno %d): it is the STOCK libunixlib, not SharedLibs-C-armeabihf 16.2.0-8 or later\n", errno); return 1; }
  printf ("fixlevel: libunixlib fix level %ld (%ld or more is needed): %s\n", level, want, level >= want ? "OK" : "TOO LOW");
  return level >= want ? 0 : 1;
}
