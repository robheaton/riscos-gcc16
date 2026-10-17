/* t_tls.c - thread-local variables in a program (tpidr_el0 is set up by the start-up code): expected output "42 7". */
#include <stdio.h>
__thread int counter = 41;
__thread char big[100];
int main (void) { counter++; big[5] = 7; printf ("%d %d\n", counter, big[5]); return 0; }
