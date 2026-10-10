/* *StkLeft_Show [depth]: __modlib_stack_left () at the top and at DEPTH levels of recursion (each level has a 1 KB frame). */
#include <stdio.h>
#include <stdlib.h>
#include <kernel.h>
#include "header.h"

static long deep (int n)
{
  volatile char frame[1024];
  frame[0] = (char) n; frame[1023] = (char) n;
  return n <= 0 ? __modlib_stack_left () + frame[0] : deep (n - 1) + frame[1023] - n;
}

_kernel_oserror *sl_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) number; (void) pw;
  printf ("top %ld\n", __modlib_stack_left ());
  printf ("deep %ld\n", deep (argc > 0 ? atoi (arg_string) : 8));
  return 0;
}

_kernel_oserror *sl_final (int fatal, int podule, void *pw)
{
  (void) fatal; (void) podule; (void) pw;
  return 0;
}
