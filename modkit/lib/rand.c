/* rand.c - rand and srand: a linear congruential generator, the high 31 bits.  The first value after srand (1) - the state at start - is the same on every run. */
#include <stdlib.h>

static unsigned rand_state = 1;
int rand (void) { rand_state = rand_state * 1103515245u + 12345u; return (int) (rand_state >> 1); }
void srand (unsigned seed) { rand_state = seed; }
