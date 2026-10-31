/* random.c - random, srandom, initstate, setstate, rand, srand and rand_r of UnixLib 64 with the generators of glibc (the additive feedback generator of random_r.c, the five state sizes; rand () is random ()), so that the same
   seed gives the same numbers as on Linux.  picolibc's own generators give other sequences.  The SDK of RISC OS 64. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_TYPES 5
#define TYPE_0 0
#define TYPE_3 3
static const struct { int deg, sep; } tab[MAX_TYPES] = { { 0, 0 }, { 7, 3 }, { 15, 1 }, { 31, 3 }, { 63, 1 } };

static int32_t randtbl[32] =
{ TYPE_3,
  -1726662223, 379960547, 1735697613, 1040273694, 1313901226, 1627687941, -179304937, -2073333483, 1780058412, -1989503057, -615974602, 344556628, 939512070, -1249116260, 1507946756,
  -812545469, 1362785011, -1210203903, 1286590146, -1066606351, 1211146906, -1136868589, 1316365381, -1253049385, -1019428467, 1590684398, 1146868084, 1067536301, -1004936339, -1631152396 };

static struct { uint32_t *fptr, *rptr, *state, *end; int type, deg, sep; } R =
{ (uint32_t *) &randtbl[1 + 3], (uint32_t *) &randtbl[1], (uint32_t *) &randtbl[1], (uint32_t *) &randtbl[1 + 31], TYPE_3, 31, 3 };

static int32_t next_random (void)
{
  uint32_t v;
  if (R.type == TYPE_0)
    {
      int32_t val = (R.state[0] * 1103515245u + 12345u) & 0x7fffffff;
      R.state[0] = val;
      return val;
    }
  *R.fptr += *R.rptr;
  v = (*R.fptr >> 1) & 0x7fffffff;
  ++R.fptr;
  if (R.fptr >= R.end) { R.fptr = R.state; ++R.rptr; }
  else { ++R.rptr; if (R.rptr >= R.end) R.rptr = R.state; }
  return (int32_t) v;
}

void srandom (unsigned int seed)
{
  int i;
  int32_t word;
  if (seed == 0) seed = 1;
  R.state[0] = seed;
  if (R.type == TYPE_0) return;
  word = (int32_t) seed;
  for (i = 1; i < R.deg; i++)
    {
      long hi = word / 127773, lo = word % 127773;
      word = 16807 * lo - 2836 * hi;
      if (word < 0) word += 2147483647;
      R.state[i] = word;
    }
  R.fptr = &R.state[R.sep];
  R.rptr = &R.state[0];
  for (i = 0; i < R.deg * 10; i++) next_random ();
}

long random (void)
{
  return next_random ();
}

char *initstate (unsigned int seed, char *arg_state, size_t n)
{
  int32_t *ostate = (int32_t *) (R.state) - 1;
  int type;
  uint32_t *state;
  if (R.type == TYPE_0) ostate[0] = TYPE_0;
  else ostate[0] = MAX_TYPES * (int) (R.rptr - R.state) + R.type;
  if (n < 8) { errno = EINVAL; return 0; }
  type = n >= 256 ? 4 : n >= 128 ? 3 : n >= 64 ? 2 : n >= 32 ? 1 : 0;
  state = (uint32_t *) arg_state + 1;
  R.type = type; R.deg = tab[type].deg; R.sep = tab[type].sep;
  R.state = state;
  R.rptr = &state[0];
  R.fptr = &state[R.sep];
  R.end = &state[R.deg];
  srandom (seed);
  if (type == TYPE_0) state[-1] = TYPE_0;
  else state[-1] = (uint32_t) (MAX_TYPES * (R.rptr - state) + type);
  return (char *) ostate;
}

char *setstate (char *arg_state)
{
  uint32_t *new_state = (uint32_t *) arg_state + 1;
  int32_t *ostate = (int32_t *) (R.state) - 1;
  int type, rear;
  if (arg_state == 0) { errno = EINVAL; return 0; }
  if (R.type == TYPE_0) ostate[0] = TYPE_0;
  else ostate[0] = MAX_TYPES * (int) (R.rptr - R.state) + R.type;
  type = (int) (new_state[-1] % MAX_TYPES);
  rear = (int) (new_state[-1] / MAX_TYPES);
  if (type < 0 || type >= MAX_TYPES) { errno = EINVAL; return 0; }
  R.type = type; R.deg = tab[type].deg; R.sep = tab[type].sep;
  R.state = new_state;
  if (type != TYPE_0) { R.rptr = &new_state[rear]; R.fptr = &new_state[(rear + R.sep) % R.deg]; }
  R.end = &new_state[R.deg];
  return (char *) ostate;
}

int rand (void) { return (int) next_random (); }
void srand (unsigned int seed) { srandom (seed); }

int rand_r (unsigned int *seed)
{
  unsigned int next = *seed;
  int result;
  next *= 1103515245; next += 12345;
  result = (unsigned int) (next / 65536) % 2048;
  next *= 1103515245; next += 12345;
  result <<= 10; result ^= (unsigned int) (next / 65536) % 1024;
  next *= 1103515245; next += 12345;
  result <<= 10; result ^= (unsigned int) (next / 65536) % 1024;
  *seed = next;
  return result;
}
