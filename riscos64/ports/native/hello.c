#include <stdio.h>
int main (int argc, char **argv)
{
  (void) argv;
  printf ("hello from a natively linked program, argc=%d\n", argc);
  return 0;
}
