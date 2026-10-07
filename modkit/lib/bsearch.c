/* bsearch.c - bsearch (the same probes as glibc's). */
#include <stddef.h>
#include <stdlib.h>
void *bsearch (const void *key, const void *base, size_t n, size_t size, int (*cmp) (const void *, const void *))
{
  size_t l = 0, u = n;
  while (l < u)
    {
      size_t idx = (l + u) / 2;
      const char *p = (const char *) base + idx * size;
      int c = cmp (key, p);
      if (c < 0) u = idx;
      else if (c > 0) l = idx + 1;
      else return (void *) p;
    }
  return 0;
}

