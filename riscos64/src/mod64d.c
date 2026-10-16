/* mod64d.c - the C part of Mod64D, the module of the data test (module test 0.4).  It answers, one question for each SWI, what a C module on RISC OS 64 needs to know about its own memory:
     - Info, Zero: is the zero-initialised area (the size word after the flags of the header) allocated right after the module, zeroed, and writable?  (flags bit 2)
     - Reloc, Count: do the pointers in .data.rel.ro and .data.rel get relocated by the module's own relocator (mod_reloc in mod64d_hdr.S), and is .data writable and kept?
     - Info, Stack: what are x12 and the stack pointer at initialisation and in a SWI?
     - Svc: what does the kernel give a service call entry (and does it call it at all)?
     - FailCopy, FailRMA, and the commands: error blocks that a program can read (the veneer copies a static block into the RMA).
   Built with -mgeneral-regs-only (no floating point or vector register is used: the floating point state is the question of the other module, Mod64F), -fPIC -fvisibility=hidden (no GOT, the pointer
   tables go to .data.rel.ro), -ffixed-x18, linked at 4 by mod64d.ld.  It calls the OS only through swi_x (swi.S).  It must not call memcpy and the like: there is no library. */
typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned char u8;

extern u64 swi_x (u64 swi, u64 *regs);			/* swi.S: the SWI with the X bit, x0-x9 from and to regs[]; returns 1 if V was set */
extern char _mod_start[] __attribute__ ((visibility ("hidden")));
extern char __image_end[] __attribute__ ((visibility ("hidden")));
extern char __ZIStart[] __attribute__ ((visibility ("hidden")));
extern char __ZIEnd[] __attribute__ ((visibility ("hidden")));
extern u64 link_addr __attribute__ ((visibility ("hidden")));
extern const u32 reloc_count __attribute__ ((visibility ("hidden")));
extern u64 svc_state[14] __attribute__ ((visibility ("hidden")));
extern u64 cs_state __attribute__ ((visibility ("hidden")));		/* mod64d_swi.S: &5A while every svc preserved x19-x28, &BAD if one did not */

#define KEEP __attribute__ ((noinline, used))
#define BARRIER(x) __asm__ volatile ("" : "+r" (x))	/* the compiler must not know what x is: it cannot fold a comparison of a pointer that it was given */

/* ---- output: OS_WriteC and OS_NewLine ---- */
static void put_c (int c) { u64 r[10] = { (u64) c }; swi_x (0x0, r); }
static void put_nl (void) { u64 r[10] = { 0 }; swi_x (0x3, r); }
static void put_s (const char *s) { while (*s) put_c (*s++); }
static void put_h (u64 v)
{
  int started = 0, sh;
  for (sh = 60; sh >= 0; sh -= 4)
    {
      int d = (int) ((v >> sh) & 15);
      if (d || started || sh == 0) { started = 1; put_c ("0123456789ABCDEF"[d]); }
    }
}
static void put_kv (const char *k, u64 v) { put_s (k); put_s (" &"); put_h (v); }

/* ---- the state that is in .data (initialised to a non-zero value on purpose: it is then in the file, and it does not need the zero-initialised area) ---- */
static u64 slot = 1;				/* the size of a slot of the SWI register block: 4 or 8; Setup tells us.  1 = not told yet: the SWIs that write several registers then refuse (a block of 32-bit slots is 44 bytes) */
static u64 init_x12 = 1, init_sp = 1, init_zero_nonzero = 1, init_counter = 1, swi_x12 = 1, init_delta = 1, err_buf_addr = 1;
static u64 args_x0 = 1, args_x1 = 1, args_x2 = 1, args_calls = 1;
static u8 args_buf[32] = { 1 };			/* the first 31 bytes of the argument string of the last Mod64D_Args, up to and including its first control character */
static int data_counter = 1000;			/* .data: must be 1000 at initialisation and keep what is written to it */

/* ---- .bss: the zero-initialised area that the kernel allocates after the module ---- */
#define ZI_BYTES 6000
static volatile u8 zi_buf[ZI_BYTES];
static volatile u64 zi_canary[2];

/* ---- pointers that must be relocated: in .data.rel.ro (const), in .data.rel (writable) ---- */
typedef struct { const char *name; int (*fn) (int); } ent_t;
static const char s_one[] = "one", s_two[] = "two", s_three[] = "three";
KEEP static int f_one (int a) { return a + 1; }
KEEP static int f_two (int a) { return a * 3; }
KEEP static int f_three (int a) { return a ^ 0x55; }
static const ent_t table_ro[3] = { { s_one, f_one }, { s_two, f_two }, { s_three, f_three } };
static ent_t table_rw[2] = { { s_two, f_two }, { s_three, f_three } };
static int *volatile counter_ptr = &data_counter;
struct node { struct node *next; const char *s; };
static struct node n2 = { 0, s_two };
static struct node n1 = { &n2, s_one };
static int (*volatile fnptr) (int) = f_three;

/* ---- error blocks ---- */
typedef struct { u32 num; char msg[252]; } err_t;
struct err32 { u32 num; char msg[28]; };
static const struct err32 err_code = { 0x20A01, "Mod64D: code error" };	/* in the module's own code area: a program cannot read it */
static const struct err32 err_copy = { 0x20A02, "Mod64D: copied error" };
static const struct err32 err_setup = { 0x20A05, "Mod64D: Setup first" };

/* ---- registers of a SWI: slots of 4 or 8 bytes ---- */
static void wr (void *regs, unsigned i, u64 v) { if (slot == 8) ((u64 *) regs)[i] = v; else ((u32 *) regs)[i] = (u32) v; }

/* ---- the RMA: OS_Module 6 (claim) and 7 (free), as the 32-bit calls: r0 = 6, r3 = size, result r2 ---- */
static u64 last_rma_error = 1;			/* the block of the last FailRMA, freed by the next one (1 = none) */
static u64 rma_claim (u64 n)
{
  u64 r[10] = { 6, 0, 0, n };
  if (swi_x (0x1E, r)) return 0;
  return r[2];
}
static void rma_free (u64 p) { u64 r[10] = { 7, 0, p }; swi_x (0x1E, r); }
/* OS_Module 13: extend a block by n bytes: r0 = 13, r2 = the block, r3 = n; the result r2 is the block (it may have moved).  Returns 0 if V was set. */
static u64 rma_extend (u64 p, u64 n)
{
  u64 r[10] = { 13, 0, p, n };
  if (swi_x (0x1E, r)) return 0;
  return r[2];
}

/* the veneer's job in a kit: an error block that a program is going to read must be in memory a program can read (the RMA); copy it */
static u64 fix_error (u64 r)
{
  const u8 *s;
  u8 *d;
  unsigned n, i;
  if (r == 0 || r == (u64) -1 || err_buf_addr <= 1) return r;
  s = (const u8 *) r;
  d = (u8 *) err_buf_addr;
  n = 4;
  while (n < 256 && s[n]) n++;
  if (n < 256) n++;
  for (i = 0; i < n; i++) d[i] = s[i];
  d[255] = 0;					/* a message that is too long is cut, not unterminated */
  return err_buf_addr;
}
static u64 rma_error (u32 num, const char *msg)
{
  u64 p;
  u8 *d;
  unsigned i;
  if (last_rma_error > 1) { rma_free (last_rma_error); last_rma_error = 1; }
  p = rma_claim (64);
  d = (u8 *) p;
  if (!p) return (u64) &err_code;
  last_rma_error = p;
  d[0] = (u8) num; d[1] = (u8) (num >> 8); d[2] = (u8) (num >> 16); d[3] = (u8) (num >> 24);
  for (i = 0; msg[i] && i < 56; i++) d[4 + i] = (u8) msg[i];
  d[4 + i] = 0;
  return p;
}

/* ---- the relocation checks of the Reloc SWI: the expected values are computed PC-relative here (ADRP / ADD: they do not need relocating), the actual ones are read from the tables ---- */
KEEP static u64 reloc_check (u64 *actual)
{
  const ent_t *t = table_ro;
  ent_t *w = table_rw;
  struct node *p = &n1;
  u64 base = (u64) _mod_start, mask = 0, sum = 0;
  int *cp;
  int (*fp) (int);
  BARRIER (t); BARRIER (w); BARRIER (p);
  if (t[0].fn == f_one) { mask |= 1; sum += (u64) t[0].fn (7); }
  if (t[1].fn == f_two) { mask |= 2; sum += (u64) t[1].fn (7); }
  if (t[2].fn == f_three) { mask |= 4; sum += (u64) t[2].fn (7); }
  if (t[0].name == s_one) mask |= 8;
  if (t[1].name == s_two) mask |= 16;
  if (t[2].name == s_three) mask |= 32;
  if (w[0].fn == f_two && w[0].name == s_two) mask |= 64;
  if (w[1].fn == f_three && w[1].name == s_three) mask |= 128;
  cp = counter_ptr;
  if (cp == &data_counter) mask |= 256;
  if (p->next == &n2 && p->s == s_one && p->next->s == s_two) mask |= 512;
  fp = fnptr;
  if (fp == f_three) mask |= 1024;
  if (link_addr == base) mask |= 2048;
  if ((base & 0xFFF) == 4) mask |= 4096;
  if (sum == 111) mask |= 8192;
  actual[0] = (u64) t[0].fn; actual[1] = (u64) f_one;
  actual[2] = (u64) cp; actual[3] = (u64) &data_counter;
  actual[4] = (u64) p->next; actual[5] = (u64) &n2;
  return mask;
}

static u64 zi_nonzero (void)
{
  u64 n = 0;
  unsigned i;
  for (i = 0; i < ZI_BYTES; i++) n += (zi_buf[i] != 0);
  n += (zi_canary[0] != 0) + (zi_canary[1] != 0);
  return n;
}

/* ---- the entries ---- */
KEEP u64 c_mod_init (const char *tail, u64 instance, void *pw)
{
  u64 sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  init_x12 = (u64) pw;
  init_sp = sp;
  init_delta = (u64) _mod_start - 4;			/* link_addr was 4 and is now where the image is (the relocator has run) */
  init_counter = (u64) data_counter;
  put_s ("Mod64D: init");
  put_kv (" base", (u64) _mod_start); put_kv (" delta", init_delta); put_kv (" x12", init_x12); put_kv (" sp", sp);
  put_nl ();
  put_s ("Mod64D: zero-initialised area");
  put_kv (" at", (u64) __ZIStart); put_kv (" size", (u64) (__ZIEnd - __ZIStart)); put_kv (" image end", (u64) __image_end);
  put_nl ();
  {
    u64 vr[10] = { (u64) __ZIStart, (u64) __ZIEnd };
    u64 v = swi_x (0x3A, vr);				/* OS_ValidateAddress: the kernel's view of the area, shown before the module touches it (x0 = 1: valid) */
    put_s ("Mod64D: OS_ValidateAddress of the zero-initialised area: V"); put_h (v); put_kv (" x0", vr[0]); put_nl ();
  }
  init_zero_nonzero = zi_nonzero ();			/* reads the whole area: if the kernel did not allocate it this is where it would fail */
  err_buf_addr = rma_claim (264);
  put_s ("Mod64D: initialised (a 64-bit module with data)");
  put_kv (" nonzero", init_zero_nonzero); put_kv (" counter", init_counter); put_kv (" relocs", reloc_count); put_kv (" errbuf", err_buf_addr);
  put_nl ();
  return 0;
}

KEEP u64 c_mod_final (void *pw)
{
  if (err_buf_addr > 1) rma_free (err_buf_addr);
  err_buf_addr = 1;
  if (last_rma_error > 1) rma_free (last_rma_error);
  last_rma_error = 1;
  put_s ("Mod64D: finalised");
  put_nl ();
  return 0;
}

KEEP u64 c_mod_command (const char *args, u64 argc, u64 num, void *pw)
{
  if (num == 0) return (u64) &err_code;			/* as it is: the pointer of a block in the module's code */
  if (num == 1) return fix_error ((u64) &err_copy);
  if (num == 3)						/* Mod64D_Args: remember what the kernel gave us */
    {
      unsigned i;
      args_x0 = (u64) args; args_x1 = argc; args_x2 = num; args_calls++;
      for (i = 0; i < 31; i++) { u8 c = (u8) args[i]; args_buf[i] = c; if (c < 32) { i++; break; } }
      for (; i < 32; i++) args_buf[i] = 0;
      return 0;
    }
  return rma_error (0x20A03, "Mod64D: RMA error");
}

KEEP u64 c_mod_swi (u64 off, void *regs, void *pw)
{
  u64 sp;
  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  swi_x12 = (u64) pw;
  if (off >= 3 && off <= 13 && slot != 4 && slot != 8) return fix_error ((u64) &err_setup);	/* Setup has not been called: the layout of the register block is not known */
  switch (off)
    {
    case 0: { u64 *r = (u64 *) regs; r[0] = r[0] + r[1]; return 0; }	/* Add: slots of 64 bits */
    case 1: { u32 *r = (u32 *) regs; r[0] = r[0] + r[1]; return 0; }	/* Add32: slots of 32 bits */
    case 2:							/* Setup: r0 = the slot size */
      {
	u32 s = *(const u32 *) regs;				/* the low word of r0: the same place in both layouts */
	if (s == 4 || s == 8) slot = s;
	wr (regs, 0, 0x1234); wr (regs, 1, 0x5678);
	return 0;
      }
    case 3:							/* Info */
      wr (regs, 0, (u64) _mod_start); wr (regs, 1, link_addr); wr (regs, 2, init_delta);
      wr (regs, 3, (u64) __ZIStart); wr (regs, 4, (u64) (__ZIEnd - __ZIStart)); wr (regs, 5, init_zero_nonzero);
      wr (regs, 6, init_x12); wr (regs, 7, swi_x12); wr (regs, 8, (u64) (__image_end - _mod_start)); wr (regs, 9, init_sp);
      return 0;
    case 4:							/* Reloc */
      {
	u64 a[6], mask = reloc_check (a);
	wr (regs, 0, mask); wr (regs, 1, a[0]); wr (regs, 2, a[1]); wr (regs, 3, a[2]); wr (regs, 4, a[3]); wr (regs, 5, a[4]); wr (regs, 6, a[5]);
	wr (regs, 7, reloc_count); wr (regs, 8, init_delta); wr (regs, 9, link_addr);
	return 0;
      }
    case 5: data_counter++; wr (regs, 0, (u64) (unsigned) data_counter); wr (regs, 1, init_counter); wr (regs, 2, (u64) &data_counter); return 0;	/* Count */
    case 6:							/* Zero: still zero? writable? */
      {
	u64 nz = zi_nonzero (), bad = 0, skipped = 0;
	unsigned i;
	if (init_zero_nonzero != 0 || nz != 0) skipped = 1;		/* the area was not zero when the module started or is not now: it is not what the header asked for, so do not write to it */
	else
	  {
	    for (i = 0; i < ZI_BYTES; i++) zi_buf[i] = (u8) (i * 7 + 3);
	    zi_canary[0] = 0x1122334455667788UL; zi_canary[1] = ~0UL;
	    for (i = 0; i < ZI_BYTES; i++) bad += (zi_buf[i] != (u8) (i * 7 + 3));
	    bad += (zi_canary[0] != 0x1122334455667788UL) + (zi_canary[1] != ~0UL);
	    for (i = 0; i < ZI_BYTES; i++) zi_buf[i] = 0;
	    zi_canary[0] = 0; zi_canary[1] = 0;
	  }
	wr (regs, 0, nz); wr (regs, 1, bad); wr (regs, 2, (u64) zi_buf); wr (regs, 3, ZI_BYTES); wr (regs, 4, skipped);
	return 0;
      }
    case 7: wr (regs, 0, sp); wr (regs, 1, (u64) pw); wr (regs, 2, init_sp); wr (regs, 3, cs_state); return 0;	/* Stack: r3 = whether x19-x28 survived the svc of this module's SWI calls */
    case 8: return fix_error ((u64) &err_copy);			/* FailCopy */
    case 9: return rma_error (0x20A03, "Mod64D: RMA error");	/* FailRMA */
    case 10:							/* Svc: what the service call entry recorded: count, entry (1 fast, 2 not indexed), x0-x5, x12, x30 */
      {
	unsigned i;
	for (i = 0; i < 10; i++) wr (regs, i, i < 8 ? svc_state[i] : (i == 8 ? svc_state[12] : svc_state[13]));
	return 0;
      }
    case 11:							/* Args: r0 = the count, r1 = the command number, r2 = the pointer, r3-r9 = the first 28 bytes of the argument string */
      {
	unsigned i;
	wr (regs, 0, args_x1); wr (regs, 1, args_x2); wr (regs, 2, args_x0);
	for (i = 0; i < 7; i++) wr (regs, 3 + i, (u64) args_buf[4 * i] | ((u64) args_buf[4 * i + 1] << 8) | ((u64) args_buf[4 * i + 2] << 16) | ((u64) args_buf[4 * i + 3] << 24));
	return 0;
      }
    case 12:							/* Heap: r0 = 0-4: claim a block of 1, 8, 24, 100, 4100 bytes and free it again; r0 = 5: claim 64, fill it, extend it by 64, check */
      {
	static const u64 sizes[5] = { 1, 8, 24, 100, 4100 };
	u32 t = *(const u32 *) regs;				/* the low word of r0: the same place in both layouts */
	if (t < 5)
	  {
	    u64 p = rma_claim (sizes[t]);
	    wr (regs, 0, p); wr (regs, 1, p & 15); wr (regs, 2, p ? ((const u32 *) p)[-1] : 0); wr (regs, 3, p ? ((const u32 *) p)[-2] : 0); wr (regs, 4, sizes[t]);
	    if (p) rma_free (p);
	    return 0;
	  }
	else
	  {
	    u64 p = rma_claim (64), q = 0, ok = 0, hdr = 0;
	    unsigned i;
	    if (p)
	      {
		for (i = 0; i < 64; i++) ((u8 *) p)[i] = (u8) (i * 3 + 1);
		q = rma_extend (p, 64);
		if (q)
		  {
		    ok = 1;
		    for (i = 0; i < 64; i++) if (((u8 *) q)[i] != (u8) (i * 3 + 1)) ok = 0;
		    for (i = 64; i < 128; i++) ((u8 *) q)[i] = 0xEE;		/* the added part is writable */
		    hdr = ((const u32 *) q)[-1];
		    rma_free (q);
		  }
		else rma_free (p);
	      }
	    wr (regs, 0, q && q != p); wr (regs, 1, ok); wr (regs, 2, hdr); wr (regs, 3, p); wr (regs, 4, q); wr (regs, 5, q != 0);
	    return 0;
	  }
      }
    case 13:							/* Dirty: a marker in the zero-initialised area, if it is clean: r0 = 1 if it was written */
      {
	u64 w = 0;
	if (init_zero_nonzero == 0 && zi_nonzero () == 0) { zi_buf[17] = 0x5A; w = 1; }
	wr (regs, 0, w);
	return 0;
      }
    default: return (u64) -1;					/* not one of ours: the kernel raises "not known" */
    }
}
