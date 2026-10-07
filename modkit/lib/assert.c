/* assert.c - what assert () of <assert.h> calls when its condition is false: the message, then abort (). */
#include <stdio.h>
#include <stdlib.h>

void __modlib_assert (const char *expr, const char *file, int line)
{
  printf ("Assertion failed: %s, file %s, line %d\n", expr, file, line);
  abort ();
}
