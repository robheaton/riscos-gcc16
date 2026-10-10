/* cxxdel.c - the global operator delete (void *) of a module: free.  A program replaces it by defining operator delete (void *) itself. */
#include <stdlib.h>
void _ZdlPv (void *p) { free (p); }
