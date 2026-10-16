/* t05file - the filing system from a 64-bit absolute: create a file with OS_Find and OS_GBPB, read it back, check the length with OS_File, delete it.
   Run it in a directory that can be written (the tests set the current directory first).  Pointers are 64 bits wide in the registers. */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);

#define OS_File 0x08
#define OS_Find 0x0D
#define OS_GBPB 0x0C

static void err (const char *what, u64 *r)
{
  const u32 *e = (const u32 *) r[0];
  t_puts ("INFO "); t_puts (what); t_puts (" error &"); t_hex (e[0], 8); t_puts (": "); t_puts ((const char *) (e + 1)); t_putc ('\n');
}

int main (void)
{
  static const char name[] = "t64file";
  char data[64], back[64];
  u64 r[10] = { 0 }, h, i;
  int v, same = 1;

  t_puts ("RISC OS 64 test t05file\n");
  for (i = 0; i < 64; i++) data[i] = (char) (0x30 + i % 40);

  /* open for output (OS_Find &80 = open for output, no path variable), write 64 bytes at the current pointer (OS_GBPB 2), close */
  r[0] = 0x80; r[1] = (u64) name;
  v = (int) swi_x (OS_Find, r);
  if (v) err ("OS_Find open for output", r);
  CHECK (v == 0);
  if (v) return t_summary ("t05file");
  h = r[0];
  t_info ("file handle", h);
  CHECK (h != 0);

  r[0] = 2; r[1] = h; r[2] = (u64) data; r[3] = 64;
  v = (int) swi_x (OS_GBPB, r);
  if (v) err ("OS_GBPB write", r);
  CHECK (v == 0);
  CHECK (r[3] == 0);                                      /* nothing left untransferred */
  r[0] = 0; r[1] = h; v = (int) swi_x (OS_Find, r);       /* close */
  if (v) err ("OS_Find close", r);
  CHECK (v == 0);

  /* OS_File 17: read the catalogue information, without path: r0 = object type (1 = file), r4 = length */
  r[0] = 17; r[1] = (u64) name;
  v = (int) swi_x (OS_File, r);
  if (v) err ("OS_File 17", r);
  CHECK (v == 0);
  t_info ("object type", r[0]);
  t_info ("length", r[4]);
  CHECK (r[0] == 1);
  CHECK (r[4] == 64);

  /* open for input (OS_Find &40), read 64 bytes (OS_GBPB 4 = at the current pointer), compare */
  r[0] = 0x40; r[1] = (u64) name;
  v = (int) swi_x (OS_Find, r);
  if (v) err ("OS_Find open for input", r);
  CHECK (v == 0);
  if (!v)
    {
      h = r[0];
      for (i = 0; i < 64; i++) back[i] = 0;
      r[0] = 4; r[1] = h; r[2] = (u64) back; r[3] = 64;
      v = (int) swi_x (OS_GBPB, r);
      if (v) err ("OS_GBPB read", r);
      CHECK (v == 0);
      CHECK (r[3] == 0);
      for (i = 0; i < 64; i++) if (back[i] != data[i]) same = 0;
      CHECK (same);
      r[0] = 0; r[1] = h; swi_x (OS_Find, r);
    }

  /* delete it again (OS_File 6) */
  r[0] = 6; r[1] = (u64) name;
  v = (int) swi_x (OS_File, r);
  if (v) err ("OS_File 6", r);
  CHECK (v == 0);
  r[0] = 17; r[1] = (u64) name;
  v = (int) swi_x (OS_File, r);
  CHECK (v == 0 && r[0] == 0);                            /* gone: object type 0 */
  return t_summary ("t05file");
}
