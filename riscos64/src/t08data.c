/* t08data - the test of Mod64D (mod64d.c, mod64d_hdr.S), module test 0.4: a module with writable data, a zero-initialised area, pointers that it relocates itself, a service call entry and error blocks.
   Run after RMLoad Mod64D.  The module prints what it saw at initialisation (before this program starts); this program asks it, SWI by SWI, and checks the answers.
   The order puts the things that cannot hurt first.  A program never reads an error block that is inside the module (a program cannot read the module's code).
   With the word "prep" on the command line it only changes the state of the module (.data, a marker in the zero-initialised area) and with "reinit" it only looks at it again: the sequence is
   "t08data prep", *RMReInit Mod64D and "t08data reinit". */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);
extern u64 t_cmdline;					/* start.S: the command line, from OS_GetEnv */

static u64 sw[14];					/* the SWI numbers: Add Add32 Setup Info Reloc Count Zero Stack FailCopy FailRMA Svc Args Heap Dirty */
static const char *const names[14] = { "Mod64D_Add", "Mod64D_Add32", "Mod64D_Setup", "Mod64D_Info", "Mod64D_Reloc", "Mod64D_Count", "Mod64D_Zero", "Mod64D_Stack",
				       "Mod64D_FailCopy", "Mod64D_FailRMA", "Mod64D_Svc", "Mod64D_Args", "Mod64D_Heap", "Mod64D_Dirty" };
static u64 mod_lo, mod_hi;				/* the module and its zero-initialised area: [mod_lo, mod_hi) */

static int lookup (const char *name, u64 *num)
{
  u64 r[10] = { 0 };
  int v;
  r[1] = (u64) name;
  v = (int) swi_x (0x39, r);				/* OS_SWINumberFromString */
  if (v) { const u32 *e = (const u32 *) r[0]; t_puts ("INFO OS_SWINumberFromString: "); t_puts ((const char *) (e + 1)); t_putc ('\n'); return 0; }
  *num = r[0];
  return 1;
}
static int in_module (u64 p) { return p >= mod_lo && p < mod_hi; }
static int streq (const char *a, const char *b) { u64 n = strlen (a); return strlen (b) == n && memcmp (a, b, n) == 0; }

static void kv (const char *name, const char *what, u64 val)
{
  char k[96];
  u64 n = strlen (name), m = strlen (what);
  memcpy (k, name, n); k[n] = ':'; k[n + 1] = ' '; memcpy (k + n + 2, what, m + 1);
  t_info (k, val);
}

static void kvn (const char *name, int n, u64 val)	/* INFO name<digit> = &val */
{
  char k[96];
  u64 m = strlen (name);
  memcpy (k, name, m); k[m] = (char) ('0' + n); k[m + 1] = 0;
  t_info (k, val);
}

/* an error block that came back in x0: say where it is, and read it only if it is not inside the module */
static void examine (const char *name, u64 p, u32 want, const char *text, int must_be_outside)
{
  kv (name, "the error block", p);
  if (must_be_outside) t_check (!in_module (p), "the error block is not inside the module");
  if (in_module (p))
    {
      t_puts ("INFO "); t_puts (name); t_puts (": the pointer is inside the module: not read by this program\n");
      return;
    }
  t_puts ("INFO "); t_puts (name); t_puts (": reading the block from this program ...\n");
  {
    const u32 *e = (const u32 *) p;
    t_check (e[0] == want && streq ((const char *) (e + 1), text), text);
  }
}

static int has_word (const char *w)			/* is the word w on the command line (it ends at the first control character) */
{
  const char *c = (const char *) t_cmdline;
  u64 n = strlen (w);
  int i;
  if (!c) return 0;
  for (i = 0; c[i] >= 32; i++)
    if ((i == 0 || c[i - 1] == ' ') && memcmp (c + i, w, n) == 0 && (c[i + n] < 33)) return 1;
  return 0;
}

/* what Mod64D_Args was given by one command: OS_CLI of cmd, then the SWI that reports it */
static void try_args (const char *cmd, int want_count, char want_first)
{
  u64 r[10], a[10];
  int i, v;
  for (i = 0; i < 10; i++) r[i] = 0;
  r[0] = (u64) cmd;
  v = (int) swi_x (0x05, r);				/* OS_CLI */
  t_puts ("INFO OS_CLI of: *"); t_puts (cmd); t_puts ("    V = "); t_hex ((u64) v, 1); t_putc ('\n');
  if (v)						/* the kernel refused it: show its error (not read if it is inside the module) */
    {
      kv (cmd, "the error block", r[0]);
      if (in_module (r[0])) t_puts ("INFO   the pointer is inside the module: not read by this program\n");
      else { const u32 *e = (const u32 *) r[0]; t_puts ("INFO   error &"); t_hex (e[0], 1); t_puts (": "); t_puts ((const char *) (e + 1)); t_putc ('\n'); }
      return;
    }
  for (i = 0; i < 10; i++) a[i] = 0;
  swi_x (sw[11], a);
  t_info ("  parameters counted by the kernel (x1)", a[0]); t_info ("  command number (x2)", a[1]);
  t_info ("  argument string at (x0): inside the module (1 = yes)", (u64) in_module (a[2]));
  t_puts ("INFO   the first 28 bytes of the argument string, up to the first control character: ");
  for (i = 0; i < 28; i++)
    {
      u32 w = (u32) a[3 + i / 4];
      u32 c = (w >> (8 * (i % 4))) & 255;
      if (c < 32) { t_puts ("<"); t_hex (c, 2); t_puts (">"); break; }
      t_putc ((int) c);
    }
  t_putc ('\n');
  if (want_count >= 0) t_check (a[0] == (u64) want_count, "the kernel counted the parameters as expected");
  if (want_first) t_check ((a[3] & 255) == (u32) want_first, "the argument string starts at the first parameter");
}

int main (void)
{
  u64 r[10];
  u64 slot = 0, base, imgsize, zsize, zstart;
  int i, v, ok64, ok32, prep = has_word ("prep"), reinit = has_word ("reinit");
  t_puts ("RISC OS 64 test t08data"); if (prep) t_puts (" (prep)"); if (reinit) t_puts (" (reinit)"); t_putc ('\n');
  for (i = 0; i < 14; i++)
    if (!lookup (names[i], &sw[i])) { t_check (0, "the SWIs of Mod64D are there"); return t_summary ("t08data"); }
  t_check (1, "the SWIs of Mod64D are there");

  /* 1. the size of a slot of the register block (the module cannot know: a kernel may use 32 bits or 64) */
  for (i = 0; i < 10; i++) r[i] = 0;
  r[0] = 40; r[1] = 2; v = (int) swi_x (sw[0], r); ok64 = (v == 0 && r[0] == 42);
  for (i = 0; i < 10; i++) r[i] = 0;
  r[0] = 40; r[1] = 2; v = (int) swi_x (sw[1], r); ok32 = (v == 0 && (r[0] & 0xFFFFFFFF) == 42);
  slot = ok64 ? 8 : ok32 ? 4 : 0;
  t_puts ("INFO the SWI register block has slots of "); t_puts (ok64 ? "64 bits" : ok32 ? "32 bits" : "an unknown size"); t_putc ('\n');
  t_check (ok64 || ok32, "one of the two slot sizes works");
  if (!slot) return t_summary ("t08data");

  /* 2. Setup: tell the module; it answers in the new layout */
  for (i = 0; i < 10; i++) r[i] = 0;
  r[0] = slot; v = (int) swi_x (sw[2], r);
  t_check (v == 0 && r[0] == 0x1234 && r[1] == 0x5678, "Mod64D_Setup: the module reads and writes the register block in the same layout");

  /* prep and reinit: the two halves of the *RMReInit test */
  if (prep || reinit)
    {
      u64 counter, nonzero;
      for (i = 0; i < 10; i++) r[i] = 0;
      swi_x (sw[3], r);					/* Info */
      base = r[0]; mod_lo = base; mod_hi = r[3] + r[4]; nonzero = r[5];
      t_info ("module base", base); t_info ("link_addr", r[1]); t_info ("relocation delta", r[2]); t_info ("non-zero bytes in the zero-initialised area at initialisation", nonzero);
      if (prep)
	{
	  for (i = 0; i < 3; i++) { for (v = 0; v < 10; v++) r[v] = 0; swi_x (sw[5], r); counter = r[0]; kvn ("Mod64D_Count no. ", i + 1, counter); }
	  for (v = 0; v < 10; v++) r[v] = 0;
	  swi_x (sw[13], r);				/* Dirty: a marker in the zero-initialised area */
	  t_info ("Mod64D_Dirty: the marker was written (1 = yes)", r[0]);
	  t_check (r[0] == 1, "the zero-initialised area was clean and now has a marker");
	}
      else
	{
	  for (i = 0; i < 10; i++) r[i] = 0;
	  swi_x (sw[4], r);				/* Reloc */
	  t_info ("relocation checks that held (all = 3FFF)", r[0]);
	  t_check (r[0] == 0x3FFF, "after *RMReInit the pointers of the module are still right (the relocator did not add the delta twice)");
	  for (i = 0; i < 10; i++) r[i] = 0;
	  swi_x (sw[5], r); counter = r[0];
	  t_info ("Mod64D_Count after *RMReInit (1004 = .data was kept, 1001 = .data is back to its initial value)", counter);
	  t_info (".data persisted across *RMReInit (1 = yes, 0 = reset to the file)", (u64) (counter == 1004));
	  t_info ("the zero-initialised area was zeroed again by *RMReInit (1 = yes, 0 = the marker of the prep step was still there)", (u64) (nonzero == 0));
	}
      return t_summary ("t08data");
    }

  /* 3. Info: where the module is, where the zero-initialised area is, what x12 was */
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[3], r);
  t_check (v == 0, "Mod64D_Info");
  base = r[0]; imgsize = r[8]; zstart = r[3]; zsize = r[4];
  mod_lo = base; mod_hi = zstart + zsize;
  t_info ("module base", base); t_info ("link_addr (where the relocator says the image is)", r[1]); t_info ("relocation delta at initialisation", r[2]);
  t_info ("image size", imgsize); t_info ("zero-initialised area", zstart); t_info ("zero-initialised size", zsize); t_info ("non-zero bytes in it at initialisation", r[5]);
  t_info ("x12 at initialisation", r[6]); t_info ("x12 in this SWI", r[7]); t_info ("stack pointer at initialisation", r[9]);
  t_check ((base & 0xFFF) == 4, "the module is at a page boundary plus 4");
  t_check (r[1] == base, "link_addr is where the module is: the relocator ran");
  t_check (r[2] == base - 4, "the relocation delta is the distance from the link address 4");
  t_check (r[5] == 0, "the zero-initialised area was all zero at initialisation (the module read all of it)");
  t_check (zstart == base + imgsize, "(layout) the code expects the zero-initialised area right after the image: both sides are link-time values, the zero-fill and write checks are the evidence about the kernel");
  t_check (zsize >= 6000 && (zsize & 15) == 0, "the zero-initialised area has the size that the header says");

  /* 4. .data: written and kept */
  for (i = 0; i < 3; i++)
    {
      for (v = 0; v < 10; v++) r[v] = 0;
      v = (int) swi_x (sw[5], r);
      t_check (v == 0 && r[0] == (u64) (1001 + i) && r[1] == 1000, "Mod64D_Count: .data holds its value and is writable");
    }
  t_info ("the counter is at", r[2]);
  t_check (r[2] > base && r[2] < zstart, "the counter is in the image, not in the zero-initialised area");

  /* 5. the relocated pointers: the module compares the tables with the addresses that it computes PC-relative (and calls through them only when they agree) */
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[4], r);
  t_check (v == 0, "Mod64D_Reloc");
  t_info ("relocation checks that held (bit mask, all = 3FFF)", r[0]);
  t_info ("table_ro[0].fn", r[1]); t_info ("should be f_one", r[2]); t_info ("counter_ptr", r[3]); t_info ("should be &data_counter", r[4]);
  t_info ("node.next", r[5]); t_info ("should be &n2", r[6]); t_info ("number of relocations", r[7]);
  t_check (r[0] == 0x3FFF, "all the pointers of the module were relocated and the calls through them work");

  /* 6. the stack and x12 in a SWI */
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[7], r);
  t_check (v == 0, "Mod64D_Stack");
  t_info ("stack pointer in a SWI", r[0]); t_info ("x12 in a SWI", r[1]); t_info ("stack pointer at initialisation", r[2]);
  t_info ("x19-x28 across the svc of the module's own SWI calls (&5A = all preserved, &BAD = one was changed)", r[3]);
  t_check ((r[0] & 15) == 0, "the stack pointer in a SWI is 16-byte aligned");
  t_check (r[3] == 0x5A, "x19-x28 survive an svc taken from module code (EL1)");

  /* 7. the zero-initialised area: still zero, writable, and clean again */
  for (i = 0; i < 10; i++) r[i] = 0;
  t_puts ("INFO Mod64D_Zero writes the zero-initialised area (6000 bytes) ...\n");
  v = (int) swi_x (sw[6], r);
  t_info ("Mod64D_Zero: the write phase was skipped (1 = yes: the area was not zero)", r[4]);
  t_check (v == 0 && r[0] == 0, "Mod64D_Zero: the area is zero");
  t_check (r[4] == 0 && r[1] == 0, "Mod64D_Zero: what was written to the area was read back (no byte wrong)");
  t_info ("the buffer is at", r[2]);
  t_check (r[2] >= zstart && r[2] + r[3] <= zstart + zsize, "the buffer is inside the zero-initialised area");

  /* 8. error blocks that a program can read: a static block copied into the RMA by the module's veneer, and a block built in the RMA */
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[8], r);
  t_check (v == 1, "Mod64D_FailCopy: V is set");
  if (v) examine ("Mod64D_FailCopy", r[0], 0x20A02, "Mod64D: copied error", 1);
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[9], r);
  t_check (v == 1, "Mod64D_FailRMA: V is set");
  if (v) examine ("Mod64D_FailRMA", r[0], 0x20A03, "Mod64D: RMA error", 1);

  /* 9. the service call entry.  Mod64D has a table of service numbers in the form that CMunge writes (a RET at the entry, the fast entry after it): the program sends service &59A41, a number only this
         module lists, with OS_ServiceCall and the module says what it was given.  Mod64S (if it is loaded) has a plain entry, no table: it is called for every service, and says what it got for &59AC1. */
  for (i = 0; i < 10; i++) r[i] = 0;
  r[0] = 0xA0; r[1] = 0x59A41; r[2] = 0xA2; r[3] = 0xA3; r[4] = 0xA4; r[5] = 0xA5; r[6] = 0xA6; r[7] = 0xA7; r[8] = 0xA8;
  t_puts ("INFO OS_ServiceCall with service &59A41 (Mod64D's table) ...\n");
  v = (int) swi_x (0x30, r);				/* OS_ServiceCall */
  t_info ("OS_ServiceCall: V", (u64) v); t_info ("OS_ServiceCall: r1 after (0 = claimed)", r[1]);
  t_check (v == 0 && r[1] != 0, "OS_ServiceCall: the service was not claimed (r1 is not 0)");
  for (i = 0; i < 10; i++) r[i] = 0;
  v = (int) swi_x (sw[10], r);
  t_check (v == 0, "Mod64D_Svc");
  t_info ("Mod64D (the table): times its entry was called", r[0]); t_info ("Mod64D (the table): 1 = the fast (indexed) entry, 2 = the plain entry", r[1]);
  t_info ("Mod64D (the table): x0", r[2]); t_info ("Mod64D (the table): x1", r[3]); t_info ("Mod64D (the table): x2", r[4]); t_info ("Mod64D (the table): x3", r[5]);
  t_info ("Mod64D (the table): x4", r[6]); t_info ("Mod64D (the table): x5", r[7]); t_info ("Mod64D (the table): x12", r[8]); t_info ("Mod64D (the table): x30 (the caller)", r[9]);
  t_check (r[0] >= 1, "the kernel called the service entry of the module that has the table");
  {
    u64 ssetup, ssvc, sring;
    if (lookup ("Mod64S_Setup", &ssetup) && lookup ("Mod64S_Svc", &ssvc) && lookup ("Mod64S_Ring", &sring))
      {
	for (i = 0; i < 10; i++) r[i] = 0;
	r[0] = slot; swi_x (ssetup, r);
	for (i = 0; i < 10; i++) r[i] = 0;
	r[0] = 0xB0; r[1] = 0x59AC1; r[2] = 0xB2; r[3] = 0xB3; r[4] = 0xB4; r[5] = 0xB5; r[6] = 0xB6; r[7] = 0xB7; r[8] = 0xB8;
	t_puts ("INFO OS_ServiceCall with service &59AC1 (Mod64S, no table) ...\n");
	v = (int) swi_x (0x30, r);
	t_info ("OS_ServiceCall: V", (u64) v); t_info ("OS_ServiceCall: r1 after (0 = claimed)", r[1]);
	for (i = 0; i < 10; i++) r[i] = 0;
	v = (int) swi_x (ssvc, r);
	t_check (v == 0, "Mod64S_Svc");
	t_info ("Mod64S (no table): services that its entry was called for (all of them)", r[0]);
	t_info ("Mod64S (no table): where &59AC1 was: 1 = x1 (as in 32-bit RISC OS), 2 = x0, 0 = not seen", r[1]);
	t_info ("Mod64S (no table): calls with &59AC1", r[2]);
	t_info ("Mod64S (no table): x0", r[3]); t_info ("Mod64S (no table): x1", r[4]); t_info ("Mod64S (no table): x2", r[5]); t_info ("Mod64S (no table): x3", r[6]);
	t_info ("Mod64S (no table): x4", r[7]); t_info ("Mod64S (no table): x5", r[8]); t_info ("Mod64S (no table): x12", r[9]);
	t_check (r[2] >= 1, "the kernel called the plain service entry of the module that has no table");
	for (i = 0; i < 10; i++) r[i] = 0;
	swi_x (sring, r);
	t_info ("Mod64S (no table): x30 of the matching call", r[0]);
	for (i = 1; i < 9; i++) kvn ("Mod64S (no table): x1 of the service call no. ", i, r[i]);
      }
    else t_puts ("INFO Mod64S is not loaded: the plain service entry was not tested\n");
  }

  /* 10. OS_ValidateAddress: on RISC OS 64 the answer is in x0 (1 = valid), not in the carry flag */
  {
    u64 local = 0, probe[4];
    probe[0] = (u64) &local; probe[1] = base; probe[2] = 0x100; probe[3] = zstart;
    for (i = 0; i < 4; i++)
      {
	for (v = 0; v < 10; v++) r[v] = 0;
	r[0] = probe[i]; r[1] = probe[i] + 8;
	v = (int) swi_x (0x3A, r);
	t_puts ("INFO OS_ValidateAddress of "); t_hex (probe[i], 8); t_puts (": V = "); t_hex ((u64) v, 1); t_puts (", x0 = &"); t_hex (r[0], 1);
	t_puts (i == 0 ? " (a local variable)" : i == 1 ? " (the module's header)" : i == 2 ? " (address &100)" : " (the zero-initialised area)"); t_putc ('\n');
      }
  }
  /* 11. a command that returns an error: OS_CLI with the X bit gives the block to this program.  FailCode returns a block in the module's code, as it is: the program says where it is and does not
         read it when it is inside the module.  The other two must be readable. */
  {
    static const char *const cmds[3] = { "Mod64D_FailCode", "Mod64D_FailCopy", "Mod64D_FailRMA" };
    static const u32 wantn[3] = { 0x20A01, 0x20A02, 0x20A03 };
    static const char *const wantt[3] = { "Mod64D: code error", "Mod64D: copied error", "Mod64D: RMA error" };
    for (i = 0; i < 3; i++)
      {
	u64 q;
	for (v = 0; v < 10; v++) r[v] = 0;
	r[0] = (u64) cmds[i];
	v = (int) swi_x (0x05, r);			/* OS_CLI */
	q = r[0];
	kv (cmds[i], "OS_CLI: V", (u64) v);
	t_check (v == 1, "OS_CLI of the command returns with V set");
	if (v) examine (cmds[i], q, wantn[i], wantt[i], i != 0);
      }
  }

  /* 12. what the kernel gives a command: the argument string and the count.  The module's command Mod64D_Args (0 to 3 parameters) records them and Mod64D_Args (the SWI) reports them. */
  t_puts ("INFO the arguments of a command (the kernel's parsing of the command line):\n");
  try_args ("Mod64D_Args", 0, 0);
  try_args ("Mod64D_Args one two", 2, 'o');
  try_args ("Mod64D_Args   spaced   words  ", 2, 's');
  try_args ("Mod64D_Args \"quoted words\" x", 2, '"');
  try_args ("Mod64D_Args a b c d", -1, 0);			/* one too many: the kernel should refuse it with a syntax error of its own */
  {
    for (i = 0; i < 10; i++) r[i] = 0;
    r[0] = (u64) "Mod64D_Args a b c d";
    v = (int) swi_x (0x05, r);
    t_check (v == 1, "a command with more parameters than the table allows is refused");
  }

  /* 13. the RMA, as a module's malloc sees it: where the blocks are, how they are aligned, the words before them, and OS_Module 13 (extend) */
  t_puts ("INFO RMA blocks claimed by the module with OS_Module 6 (freed again at once):\n");
  for (i = 0; i < 5; i++)
    {
      static const u64 sz[5] = { 1, 8, 24, 100, 4100 };
      for (v = 0; v < 10; v++) r[v] = 0;
      r[0] = (u64) i;
      v = (int) swi_x (sw[12], r);
      t_check (v == 0 && r[0] != 0, "OS_Module 6 gives a block");
      t_puts ("INFO   size "); t_dec ((i64) sz[i]); t_puts (": block at &"); t_hex (r[0], 8); t_puts ("  address mod 16 = "); t_hex (r[1], 1);
      t_puts ("  word before it = &"); t_hex (r[2], 8); t_puts ("  the one before that = &"); t_hex (r[3], 8); t_putc ('\n');
    }
  for (v = 0; v < 10; v++) r[v] = 0;
  r[0] = 5;
  v = (int) swi_x (sw[12], r);
  t_check (v == 0, "Mod64D_Heap (extend)");
  t_info ("OS_Module 13 (extend by 64): did the block move", r[0]); t_info ("OS_Module 13: the first 64 bytes kept their contents (1 = yes)", r[1]);
  t_info ("OS_Module 13: the word before the new block", r[2]); t_info ("OS_Module 13: the old block", r[3]); t_info ("OS_Module 13: the new block", r[4]);
  t_check (r[5] == 1, "OS_Module 13 (extend) returned a block");
  t_check (r[1] == 1, "OS_Module 13 kept the contents of the block");

  return t_summary ("t08data");
}
