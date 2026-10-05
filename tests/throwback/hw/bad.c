/* bad.c -- errors, a warning and notes in two files, for the throwback tests (the expected throwback is listed in the README of the pack) */
#include "bad.h"

int f (int x)
{
  int unused;
  return y + x;
}

int main (void)
{
  return f (1, 2);
}
