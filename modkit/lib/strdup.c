/* strdup.c - strdup and strndup (memory from the RMA: malloc). */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
char *strdup (const char *s)
{
  size_t k = strlen (s) + 1;
  char *p = malloc (k);
  return p ? memcpy (p, s, k) : 0;
}
char *strndup (const char *s, size_t n)
{
  size_t k = strnlen (s, n);
  char *p = malloc (k + 1);
  if (!p) return 0;
  memcpy (p, s, k);
  p[k] = 0;
  return p;
}
