/* perror.c - perror: the message of errno on stderr. */
#include <stdio.h>
#include <string.h>
#include <errno.h>

void perror (const char *s)
{
  int e = errno;
  if (s && *s)
    {
      fputs (s, stderr);
      fputs (": ", stderr);
    }
  fputs (strerror (e), stderr);
  fputc ('\n', stderr);
}
