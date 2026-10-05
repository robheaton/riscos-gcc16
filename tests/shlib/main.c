#include <stdio.h>
extern int foo_add(int);
extern int foo_counter;
int main(void) { printf("%d %d\n", foo_add(4), foo_counter); return 0; }
