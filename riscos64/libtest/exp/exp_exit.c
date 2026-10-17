/* exp_exit - E2 of the process model design: is the exit handler (OS_ChangeEnvironment 11) called when a CHILD program ends, and with what?  The handler is position independent code copied into a dynamic area (the
   child is loaded over this program).  It saves x0-x3, x12, sp and the first words at sp to the area, writes the area to the file "exitreg" (OS_File 10), prints a marker and ends with OS_Exit (return code 0: a non-zero code would be an error on RISC OS, where Sys$RCLimit is 1).
   Run: exp_exit [command]   (default "t_child"); afterwards "*Type exitreg" is not useful (binary): the job reads it with exp_exit -d.  With -d it prints the saved registers. */
#include <stdio.h>
#include <string.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);

__asm__ (
  ".text\n.balign 8\n.global exp_h_start\n.global exp_h_end\n"
  "exp_h_start:\n"
  "	adr	x16, exp_h_data\n"
  "	stp	x0, x1, [x16]\n"
  "	stp	x2, x3, [x16, #16]\n"
  "	mov	x17, sp\n"
  "	stp	x12, x17, [x16, #32]\n"
  "	ldp	x4, x5, [x17]\n"
  "	stp	x4, x5, [x16, #48]\n"
  "	adr	x0, exp_h_msg\n"
  "	mov	x10, #2\n	orr	x10, x10, #0x20000\n	svc	#0\n"			/* OS_Write0 */
  "	mov	x0, #10\n	adr	x1, exp_h_name\n	mov	x2, #0xFFD\n	mov	x3, #0\n"
  "	mov	x4, x16\n	add	x5, x16, #64\n"
  "	mov	x10, #8\n	orr	x10, x10, #0x20000\n	svc	#0\n"			/* OS_File 10: save */
  "	mov	x0, #11\n	ldr	x1, [x16, #64]\n	mov	x2, #0\n	mov	x3, #0\n"				/* put the old exit handler back, or OS_Exit calls this one again */
  "	mov	x10, #0x40\n	orr	x10, x10, #0x20000\n	svc	#0\n"
  "	mov	x0, #0\n	movz	x1, #0x4241\n	movk	x1, #0x5845, lsl #16\n	mov	x2, #0\n	mov	x10, #0x11\n	orr	x10, x10, #0x20000\n	svc	#0\n"	/* OS_Exit */
  "1:	b	1b\n"
  ".balign 8\nexp_h_msg:	.asciz	\"[exit handler called]\\r\\n\"\n"
  "exp_h_name:	.asciz	\"exitreg\"\n"
  ".balign 8\nexp_h_data:	.space	80\n"
  "exp_h_end:\n");
extern char exp_h_start[], exp_h_end[];

int main (int argc, char **argv)
{
  unsigned long r[10] = { 0, (unsigned long) -1, 8192, (unsigned long) -1, 0, 1UL << 20, 0, 0, (unsigned long) "UL64 exit test" };
  unsigned long base, n = (unsigned long) (exp_h_end - exp_h_start);
  if (argc > 1 && !strcmp (argv[1], "-d"))
    {
      FILE *f = fopen ("exitreg", "rb");
      unsigned long v[8];
      int i;
      if (!f || fread (v, 8, 8, f) != 8) { printf ("no exitreg file\n"); return 1; }
      for (i = 0; i < 8; i++) printf ("%s %#lx\n", (const char *[]) { "x0", "x1", "x2", "x3", "x12", "sp", "[sp]", "[sp+8]" }[i], v[i]);
      return 0;
    }
  if (swi_x (0x66, r)) { printf ("OS_DynamicArea 0 failed: %s\n", (char *) r[0] + 4); return 1; }
  base = r[3];
  memcpy ((void *) base, exp_h_start, n);
  { unsigned long s[10] = { 1, base, base + n }; swi_x (0x6E, s); }
  {
    unsigned long e[10] = { 11, 0, 0, 0 };
    swi_x (0x40, e);									/* the handler that is set now */
    *(unsigned long *) (base + (size_t) (exp_h_end - exp_h_start) - 16) = e[1];		/* into the data block, offset 64 */
  }
  {
    unsigned long e[10] = { 11, base, 0, base + 0x1000 };				/* handler, r12 value, buffer */
    if (swi_x (0x40, e)) { printf ("OS_ChangeEnvironment 11 failed: %s\n", (char *) e[0] + 4); return 1; }
    printf ("exit handler set at %#lx (was %#lx, r2 %#lx, r3 %#lx)\n", base, e[1], e[2], e[3]);
  }
  fflush (stdout);
  {
    unsigned long c[10] = { (unsigned long) (argc > 1 ? argv[1] : "t_child") };
    unsigned long err = swi_x (0x05, c);						/* OS_CLI */
    printf ("OS_CLI returned (%s)\n", err ? "error" : "no error");			/* reached only if the parent survives */
  }
  return 0;
}
