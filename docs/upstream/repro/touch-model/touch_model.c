/* Host model of __touch_stack_buffer () (incl-local/internal/unix.h, patched by unixlib-touch-stack-buffers).
   build-and-run.sh cuts the function out of the patched header, replaces the stack pointer read ("mov %0, sp") by a model variable and the load that touches a page by a call that
   records the page, and compiles it.  The model then runs random sp / buffer / length triples and checks, against an independent statement of the rule:
     - a buffer that starts less than 1 MB above sp: every 4 KB page that holds at least one byte of it is touched, up to (and not beyond) sp + 1 MB; nothing outside those pages
     - any other buffer (heap, static, below sp): nothing is touched
   and that the call returns (no endless loop) for lengths up to 4 GB and buffers near the top of the address space.
   usage: touch_model [CASES]; the exit status is the number of failures.  */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

static unsigned int model_sp;
static unsigned char *touched;            /* one flag per page of the 4 GB space: 1 M pages */
static long ntouched;
static unsigned int *list; static long nlist;                  /* the touched pages, to clear only those between cases */
static void record (unsigned int addr) { if (!touched[addr >> 12]) { touched[addr >> 12] = 1; ntouched++; if (nlist < 1000000) list[nlist++] = addr >> 12; } }
#define __ARM_EABI__ 1
#include "touch_fn.inc"

static uint64_t rng = 88172645463325252ull;
static uint32_t rnd (void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t) (rng >> 11); }
static int bad;
#define CHECK(c, ...) do { if (!(c)) { if (bad++ < 12) { printf ("FAIL line %d: ", __LINE__); printf (__VA_ARGS__); printf ("\n"); } } } while (0)

int main (int argc, char **argv)
{
  long n = argc > 1 ? atol (argv[1]) : 2000000, mapped = 0, ignored = 0;
  touched = calloc (1u << 20, 1); list = calloc (1000000, sizeof *list);
  for (long i = 0; i < n; i++)
    {
      static unsigned int pages_touched[4];
      (void) pages_touched;
      model_sp = (rnd () % 0xE0000u) << 12 | (rnd () & 0xff8);               /* 8-byte aligned, anywhere below 3.5 GB */
      unsigned int lo, len;
      switch (rnd () % 9)
        {
        case 0: lo = model_sp + (rnd () % 0x100000u); len = rnd () % 70000; break;          /* a buffer on the stack */
        case 1: lo = model_sp + (rnd () % 0x100000u); len = rnd () % 0x300000u; break;      /* a big one, running past the 1 MB limit */
        case 2: lo = model_sp + 0x100000u + (rnd () % 0x1000000u); len = rnd () % 70000; break;   /* above the limit: not this thread's stack */
        case 3: lo = model_sp - 1 - (rnd () % 0x100000u); len = rnd () % 70000; break;      /* below sp */
        case 4: lo = 0xFFE00000u + (rnd () % 0xF0000u); len = rnd () % 0x20000u; model_sp = lo - (rnd () % 0x1000u); break;  /* the top of the address space (the last 2 MB; a buffer in the very last page would make the loop wrap round, which no RISC OS stack can do) */
        case 6: lo = model_sp + 0x100000u - (rnd () % 3); len = rnd () % 9000; break;      /* at the very edge of the 1 MB: exactly on it, 1 and 2 bytes inside */
        case 7: lo = model_sp + (rnd () % 0x100000u); len = 0u - (rnd () % 0x400u); break;  /* a length that makes lo + len wrap round the 4 GB */
        case 8: lo = model_sp + 0x100000u; len = 1 + rnd () % 9000; break;                 /* exactly 1 MB above sp: not on the stack */
        default: lo = model_sp + (rnd () % 5); len = rnd () % 5000; break;                  /* tiny and next to sp */
        }
      while (nlist) touched[list[--nlist]] = 0;
      ntouched = 0;
      __touch_stack_buffer ((const void *) (uintptr_t) lo, len);
      int on_stack = (unsigned int) (lo - model_sp) < 0x100000u;
      if (!on_stack) { CHECK (ntouched == 0, "touched %ld pages for a buffer at %08x that is not on the stack (sp %08x)", ntouched, lo, model_sp); ignored++; continue; }
      mapped++;
      uint64_t first = lo >> 12, limit = (uint64_t) model_sp + 0x100000u, last_byte = (uint64_t) lo + len;   /* exclusive end of the range that has to be covered */
      if (last_byte > limit) last_byte = limit;
      uint64_t lastpage = last_byte > lo ? (last_byte - 1) >> 12 : first;                                 /* an empty buffer: at most its own page */
      long expect = (long) (lastpage - first + 1);
      if (len == 0) expect = 0;
      for (uint64_t pg = first; pg <= lastpage && pg < (1u << 20); pg++) if (len) CHECK (touched[pg], "page %llx of the buffer %08x + %u (sp %08x) not touched", (unsigned long long) pg, lo, len, model_sp);
      CHECK (ntouched <= expect + (len == 0 ? 1 : 0), "touched %ld pages, the buffer %08x + %u (sp %08x) covers %ld", ntouched, lo, len, model_sp, expect);
    }
  printf ("%s: %ld cases (%ld on the stack, %ld not): %d failed check(s)\n", getenv ("MODEL_LABEL") ? getenv ("MODEL_LABEL") : "touch model", n, mapped, ignored, bad);
  return bad != 0;
}
