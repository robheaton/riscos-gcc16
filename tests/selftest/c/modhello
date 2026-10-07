/* modhello.c -- a module for the smoke test of  gcc -mmodule : no UnixLib or Shared C Library, the C library and the header of modkit, the veneers that cmunge wrote.  *ModHello_Sum 2 3 prints 5.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "modhello.h"

_kernel_oserror *modhello_init (const char *tail, int podule_base, void *pw)
{
  (void) tail; (void) podule_base; (void) pw;
  return NULL;
}

_kernel_oserror *modhello_final (int fatal, int podule_base, void *pw)
{
  (void) fatal; (void) podule_base; (void) pw;
  return NULL;
}

_kernel_oserror *modhello_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) argc; (void) pw;
  if (number == CMD_ModHello_Sum)
    {
      char *end;
      long a = strtol (arg_string, &end, 10), b = strtol (end, NULL, 10);
      printf ("%ld\n", a + b);
    }
  return NULL;
}
