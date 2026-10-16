/* t01hello - the first contact: does an AIF64 absolute start, print and exit? */
#include "t64.h"
int main (void)
{
  t_puts ("RISC OS 64 test t01hello: hello from AArch64\n");
  t_puts ("t01hello: a second line, after OS_NewLine\n");
  t_puts ("t01hello: done\n");
  return 0;
}
