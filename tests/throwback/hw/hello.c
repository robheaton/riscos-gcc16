/* hello.c -- the smallest whole program for RunTb11: compile, link (the linker command line is long) and run */
#include <stdio.h>
int main (int argc, char **argv)
{
  printf ("hello from a program built by the native compiler: %d arguments\n", argc - 1);
  return 0;
}
