/* host-sultrace-test.c -- test sl_mark () of sultrace.h on the build host (the target code cannot run here): the absolute-address reads (SL_RD), _swix and _kernel_swi_c are mocks, sl_put keeps the line.
   Checks the line format (the fields v sp ctx t p g c h, 8 hex digits each), the values read from the mock application space, the guard of g (OS_ValidateAddress: C set / an error -> ffffffff), and the cut of a long tag.
   build + run:  gcc -std=gnu11 -O1 -Wall -Wextra -Ihost-stubs -DSL_PUT_HOST host-sultrace-test.c -o /tmp/host-sultrace-test && /tmp/host-sultrace-test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned mem_ws[64];                                     /* the words 0x8000 ... 0x80FF */
static unsigned g_target = 0x9878;                              /* the word at [0x8038] */
static unsigned g_addr;                                         /* the address that is "valid" for OS_ValidateAddress */
static int validate_mode;                                       /* 0: valid for g_addr only, 1: always an error, 2: always C set */
static unsigned rd_log[200]; static int rd_n;

static unsigned mock_rd(uintptr_t a)
{
  rd_log[rd_n++ % 200] = (unsigned) a;
  if (a >= 0x8000 && a < 0x8100) return mem_ws[(a - 0x8000) / 4];
  if (a == g_addr) return g_target;
  fprintf(stderr, "FAIL: sl_mark read the address %08lx, which is neither the workspace nor the validated pointer\n", (unsigned long) a);
  exit(1);
}
#define SL_RD(a) mock_rd ((uintptr_t) (a))

static char line[1024]; static int line_n;
static void sl_put (const char *s, int n) { memcpy (line, s, (size_t) n); line[n] = 0; line_n = n; }

#include "sultrace.h"

#include <stdarg.h>
_kernel_oserror *_swix (int swi, unsigned flags, ...)
{
  (void) flags; va_list ap; va_start (ap, flags); unsigned *out = va_arg (ap, unsigned *); va_end (ap);
  if (swi == 0x58EC6) *out = 0x6706AEE8; else if (swi == OS_ReadMonotonicTime) *out = 0x0000EB92; else { fprintf (stderr, "FAIL: unexpected SWI %x\n", swi); exit (1); }
  return NULL;
}
static _kernel_oserror err_block;
_kernel_oserror *_kernel_swi_c (int no, _kernel_swi_regs *in, _kernel_swi_regs *out, int *carry)
{
  (void) out;
  if (no != 0x2003A) { fprintf (stderr, "FAIL: unexpected SWI %x\n", no); exit (1); }
  if (validate_mode == 1) { *carry = 1; return &err_block; }
  if (validate_mode == 2) { *carry = 1; return NULL; }
  *carry = !((unsigned) in->r[0] == g_addr && (unsigned) in->r[1] == g_addr + 4);
  return NULL;
}

static int bad;
static void check (int ok, const char *what) { if (!ok) { printf ("FAIL: %s\n  line: %s", what, line); bad++; } }

int main (void)
{
  for (int i = 0; i < 64; i++) mem_ws[i] = 0x01010101u * (unsigned) i + 7;
  unsigned sum = 0; for (int i = 0; i < 64; i++) sum += mem_ws[i];
  unsigned v, sp, ctx, t, p, g, c, h; char tag[400];

  /* 1: a valid pointer */
  mem_ws[0x38 / 4] = 0x5ED67188; mem_ws[0x40 / 4] = 0x5ED67DB0; sum = 0; for (int i = 0; i < 64; i++) sum += mem_ws[i];
  g_addr = 0x5ED67188; validate_mode = 0; rd_n = 0;
  sl_mark ("X3 before stack free", 0x20F18874);
  check (sscanf (line, "X3 before stack free v=%8X sp=%8X ctx=%8X t=%8X p=%8X g=%8X c=%8X h=%8X\n", &v, &sp, &ctx, &t, &p, &g, &c, &h) == 8, "format of the line");
  check (v == 0x20F18874 && ctx == 0x6706AEE8 && t == 0xEB92 && p == 0x5ED67188 && g == 0x9878 && c == 0x5ED67DB0 && h == sum, "values (valid pointer)");
  check (line[line_n - 1] == '\n' && line_n == (int) strlen (line), "ends with one newline, length");
  check (rd_n == 64 + 2 + 1, "number of reads: 64 (the sum) + p + c + g");

  /* 2: an invalid pointer (C set): g = ffffffff, no read through it */
  mem_ws[0x38 / 4] = 0x18; g_addr = 0x5ED67188; rd_n = 0; sum = 0; for (int i = 0; i < 64; i++) sum += mem_ws[i];
  sl_mark ("UL20", 5);
  check (sscanf (line, "UL20 v=%8X sp=%8X ctx=%8X t=%8X p=%8X g=%8X c=%8X h=%8X\n", &v, &sp, &ctx, &t, &p, &g, &c, &h) == 8, "format (invalid pointer)");
  check (p == 0x18 && g == 0xFFFFFFFFu && h == sum && rd_n == 64 + 2, "invalid pointer: g = ffffffff and no read through the pointer");

  /* 3: OS_ValidateAddress fails with an error, and answers C set */
  for (int mode = 1; mode <= 2; mode++) {
    validate_mode = mode; mem_ws[0x38 / 4] = 0x5ED67188; rd_n = 0;
    sl_mark ("E", 0);
    check (sscanf (line, "E v=%8X sp=%8X ctx=%8X t=%8X p=%8X g=%8X c=%8X h=%8X\n", &v, &sp, &ctx, &t, &p, &g, &c, &h) == 8 && g == 0xFFFFFFFFu && rd_n == 64 + 2, "validate mode: g = ffffffff");
  }

  /* 4: a long tag is cut; the line stays inside its buffer */
  validate_mode = 0; g_addr = 0x5ED67188; memset (tag, 'x', sizeof tag - 1); tag[sizeof tag - 1] = 0;
  sl_mark (tag, 1);
  check (line_n <= 150 + 92 && strstr (line, " v=00000001 ") == line + 150 && strstr (line, " h=") && line[line_n - 1] == '\n', "a 399-character tag is cut at 150 characters");
  check (sscanf (line + 150, " v=%8X sp=%8X ctx=%8X t=%8X p=%8X g=%8X c=%8X h=%8X\n", &v, &sp, &ctx, &t, &p, &g, &c, &h) == 8 && g == 0x9878, "the fields after a cut tag");

  if (bad) { printf ("%d checks FAILED\n", bad); return 1; }
  printf ("ok: sl_mark: format, values, the guard of g (C set / error), the cut of a long tag\n");
  return 0;
}
