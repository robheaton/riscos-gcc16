/* env.c - clearenv.  (setenv, putenv and unsetenv are picolibc's: they keep the variables in environ, which getenv of the SDK looks in before the system variables.)  The SDK of RISC OS 64. */
#include <stdlib.h>
extern char **environ;
int clearenv (void)
{
  if (environ) environ[0] = 0;
  return 0;
}
