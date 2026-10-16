/* t02swi - what the OS says: OS_WriteN and OS_Write0, OS_GetEnv, OS_ReadMonotonicTime, an unknown SWI (the error return: the V flag and x0), the data model.
   Lines that start with INFO are the machine's answers (not compared); PASS and FAIL are checks. */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);
extern u64 t_cmdline;

static void say_error (const char *what, u64 *r)
{
  const u32 *e = (const u32 *) r[0];
  t_puts ("INFO "); t_puts (what); t_puts (": error number &"); t_hex (e[0], 8); t_puts (" ");
  t_puts ((const char *) (e + 1)); t_putc ('\n');
}

int main (void)
{
  u64 r[10] = { 0 };
  u64 v, t0, t1, i;
  char buf[16] = "ABCDEFGH";
  int local = 0;

  t_puts ("RISC OS 64 test t02swi\n");
  t_info ("sizeof (void *)", sizeof (void *));
  t_info ("sizeof (long)", sizeof (long));
  t_info ("sizeof (int)", sizeof (int));
  t_info ("sizeof (long long)", sizeof (long long));
  CHECK (sizeof (void *) == 8);
  CHECK (sizeof (u64) == 8);

  /* OS_WriteN: four characters of "ABCDEFGH" */
  t_puts ("OS_WriteN: [");
  r[0] = (u64) buf; r[1] = 4;
  v = swi_x (0x46, r);
  t_puts ("]\n");
  CHECK (v == 0);

  /* OS_Write0 of a string */
  r[0] = (u64) "OS_Write0 works"; v = swi_x (0x02, r); t_putc ('\n');
  CHECK (v == 0);

  /* OS_GetEnv: x0 -> the command line, x1 = the RAM limit, x2 -> the time */
  r[0] = r[1] = r[2] = 0; v = swi_x (0x10, r);
  CHECK (v == 0);
  t_info ("OS_GetEnv x1 (RAM limit)", r[1]);
  t_info ("address of a local variable", (u64) &local);
  t_info ("address of main", (u64) main);
  t_info ("address of the command line seen at start", t_cmdline);
  CHECK ((u64) &local < r[1]);                          /* the stack is below the RAM limit */
  CHECK ((u64) main >= 0x8000);                         /* the image is linked at &8000 */
  CHECK (t_cmdline != 0);
  if (t_cmdline) { t_puts ("INFO command line: "); t_puts ((const char *) t_cmdline); t_putc ('\n'); }

  /* OS_ReadMonotonicTime twice, with a loop between them */
  r[0] = 0; v = swi_x (0x42, r); t0 = r[0];
  for (i = 0; i < 2000000; i++) __asm__ volatile ("");
  r[0] = 0; v |= swi_x (0x42, r); t1 = r[0];
  CHECK (v == 0);
  t_info ("OS_ReadMonotonicTime first", t0);
  t_info ("OS_ReadMonotonicTime second", t1);
  CHECK (t1 >= t0);

  /* an unknown SWI with the X bit: the error comes back with the V flag set and the error block in x0 */
  r[0] = 1; r[1] = 2;
  v = swi_x (0x1FFFF0, r);                              /* a SWI of the chunk &1FFFC0: nothing there */
  t_info ("unknown SWI: V flag set (1 = yes)", v);
  if (v) say_error ("unknown SWI", r);
  CHECK (v == 1);

  /* OS_SWINumberFromString: the number of OS_WriteC, and of a SWI that does not exist */
  r[1] = (u64) "OS_WriteC"; v = swi_x (0x39, r);
  t_info ("OS_SWINumberFromString OS_WriteC: V", v);
  if (!v) { t_info ("number of OS_WriteC", r[0]); CHECK ((r[0] & 0x1FFFF) == 0); } else say_error ("OS_SWINumberFromString", r);

  return t_summary ("t02swi");
}
