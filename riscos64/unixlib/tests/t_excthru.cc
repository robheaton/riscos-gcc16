/* t_excthru.cc - a C++ exception thrown by a callback must unwind through the frame of a function of the C library.  lfind () calls its comparison function from libriscosstub.a: that library has to be built
   with -fexceptions (unwind tables), or the exception finds no handler above the frame and the program ends in std::terminate.  Prints "caught" and returns 0 when it works.  The SDK of RISC OS 64. */
#include <search.h>
#include <stdio.h>
#include <stdexcept>
static int cmp (const void *a, const void *b)
{
  if (*(const int *) a == 3) throw std::runtime_error ("thrown from the comparison function");
  return *(const int *) a - *(const int *) b;
}
int main ()
{
  int v[4] = { 1, 2, 3, 4 }, key = 3;
  size_t n = 4;
  try { lfind (&key, v, &n, sizeof v[0], cmp); printf ("not thrown\n"); return 1; }
  catch (const std::exception &e) { printf ("caught: %s\n", e.what ()); }
  return 0;
}
