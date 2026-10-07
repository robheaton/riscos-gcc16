/* qsort.c - qsort: a heap sort (in place, no recursion, no memory, O (n log n) always); not stable. */
#include <stddef.h>
#include <stdlib.h>

static void swap (char *a, char *b, size_t n)
{
  while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}
static void sift (char *b, size_t root, size_t n, size_t size, int (*cmp) (const void *, const void *))
{
  for (;;)
    {
      size_t child = 2 * root + 1;
      if (child >= n) return;
      if (child + 1 < n && cmp (b + child * size, b + (child + 1) * size) < 0) child++;
      if (cmp (b + root * size, b + child * size) >= 0) return;
      swap (b + root * size, b + child * size, size);
      root = child;
    }
}
void qsort (void *base, size_t n, size_t size, int (*cmp) (const void *, const void *))
{
  char *b = base;
  size_t i;
  if (n < 2 || !size) return;
  for (i = n / 2; i > 0; i--) sift (b, i - 1, n, size, cmp);
  for (i = n - 1; i > 0; i--) { swap (b, b + i * size, size); sift (b, 0, i, size, cmp); }
}
