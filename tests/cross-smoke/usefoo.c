#include <stdio.h>
int foo_value (void);
int main (void) { printf ("shared library: %d\n", foo_value ()); return 0; }
