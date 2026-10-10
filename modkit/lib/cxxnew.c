/* cxxnew.c - the global operator new (size_t) of a module: malloc, and abort () for a failure (there are no exceptions).  A program replaces it by defining operator new (size_t) itself. */
#include <stdlib.h>
void *_Znwj (unsigned n)
{
  void *p = malloc (n ? n : 1);
  if (!p) abort ();
  return p;
}
