/* fsdiff - the file system calls where the kit's library test (libtest64) found that Pyromaniac's file system differs from RISC OS 5.30 on a Raspberry Pi (FileSwitch and FileCore, 2026-10-07: the results are in
   modkit/tests/libtest/pi-probe.txt).  Each line is one SWI call and what came back; the answer of RISC OS is in the heading of the group or on the line.  Built with sdk/bin/aarch64-riscos-gcc.
   "fsdiff" runs everything but OS_Args 3 on a stream that is open for reading, which stops Pyromaniac ("fsdiff args3" does only that). */
#include <stdio.h>
#include <string.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);        /* the SDK's SWI veneer: x0-x9 in and out, 1 if V was set (x0 = the error block) */

#define OS_File 0x08
#define OS_Args 0x09
#define OS_GBPB 0x0C
#define OS_Find 0x0D
#define OS_FSControl 0x29

static void show (const char *what, unsigned long err, unsigned long *r)
{
  if (err) { const unsigned *e = (const unsigned *) r[0]; printf ("%-62s ERROR &%X \"%s\"\n", what, e[0], (const char *) (e + 1)); }
  else printf ("%-62s ok  r0=%lu r1=%lu r2=%lu r3=%lu r4=%lu r5=%lu\n", what, r[0] & 0xFFFFFFFF, r[1] & 0xFFFFFFFF, r[2] & 0xFFFFFFFF, r[3] & 0xFFFFFFFF, r[4] & 0xFFFFFFFF, r[5] & 0xFFFFFFFF);
}
static unsigned long find (const char *what, unsigned long reason, const char *name)
{
  unsigned long r[10] = { reason, (unsigned long) name }, e = swi_x (OS_Find, r);
  show (what, e, r);
  return e ? 0 : r[0];
}
static void closeh (unsigned long h) { unsigned long r[10] = { 0, h }; swi_x (OS_Find, r); }
static void writeb (unsigned long h, const char *buf, unsigned long n) { unsigned long r[10] = { 2, h, (unsigned long) buf, n }; swi_x (OS_GBPB, r); }
static unsigned long args (const char *what, unsigned long reason, unsigned long h, unsigned long v)
{
  unsigned long r[10] = { reason, h, v }, e = swi_x (OS_Args, r);
  show (what, e, r);
  return e ? (unsigned long) -1 : r[2];
}
static void attrs (const char *name, unsigned long a) { unsigned long r[10] = { 4, (unsigned long) name, 0, 0, 0, a }; swi_x (OS_File, r); }
static void remove_ (const char *name)                                         /* unlock, then delete (an error here does not matter) */
{
  unsigned long r[10] = { 6, (unsigned long) name };
  attrs (name, 0x03);
  swi_x (OS_File, r);
}
static void mkfile (const char *name, unsigned n)
{
  static char buf[1000];
  unsigned long r[10] = { 0x8F, (unsigned long) name };
  memset (buf, 'x', sizeof buf);
  remove_ (name);
  if (!swi_x (OS_Find, r)) { unsigned long h = r[0]; writeb (h, buf, n); closeh (h); }
}
static unsigned long objtype (const char *name, unsigned long *len)           /* OS_File 17: 0 = not found, 1 = file, 2 = directory; the length */
{
  unsigned long r[10] = { 17, (unsigned long) name };
  if (swi_x (OS_File, r)) return (unsigned long) -1;
  if (len) *len = r[4] & 0xFFFFFFFF;
  return r[0] & 0xFFFFFFFF;
}
static void delete (const char *what, const char *name) { unsigned long r[10] = { 6, (unsigned long) name }, e = swi_x (OS_File, r); show (what, e, r); }
static void rename_ (const char *what, const char *from, const char *to) { unsigned long r[10] = { 25, (unsigned long) from, (unsigned long) to }, e = swi_x (OS_FSControl, r); show (what, e, r); }
/* open the file (10 bytes, with the attributes ATTR), look at the stream's status (OS_Args 254), write one byte, close it, and look at the length */
static void open_test (const char *what, unsigned long attr, unsigned long reason)
{
  unsigned long r[10] = { reason, (unsigned long) "fsd6" }, e, h, st = 0, len = 0;
  char wr[48];
  mkfile ("fsd6", 10);
  attrs ("fsd6", attr);
  e = swi_x (OS_Find, r);
  if (e) { show (what, e, r); remove_ ("fsd6"); return; }
  h = r[0];
  { unsigned long q[10] = { 254, h }; if (!swi_x (OS_Args, q)) st = q[0] & 0xFFFFFFFF; }
  { unsigned long w[10] = { 2, h, (unsigned long) "Z", 1 };
    if (swi_x (OS_GBPB, w)) snprintf (wr, sizeof wr, "write ERROR &%X", ((const unsigned *) w[0])[0]); else snprintf (wr, sizeof wr, "write ok"); }
  closeh (h);
  objtype ("fsd6", &len);
  printf ("%-62s handle, status &%lX, %s, length afterwards %lu\n", what, st, wr, len);
  remove_ ("fsd6");
}

int main (int argc, char **argv)
{
  unsigned long h, t;
  if (argc > 1 && !strcmp (argv[1], "args3"))
    {
      mkfile ("fsd1", 10);
      h = find ("OS_Find &4F (input) of a file", 0x4F, "fsd1");
      printf ("RISC OS 5.30: OS_Args 3 gives the error &C1 \"Not open for update\"; Pyromaniac stops with an internal IOError\n");
      fflush (stdout);
      args ("OS_Args 3 (set the extent to 5) on the input stream", 3, h, 5);
      closeh (h);
      return 0;
    }
  printf ("1. OS_Find of a file that is not there (RISC OS: bit 3 = an error for input and update; &8F, output, makes the file)\n");
  remove_ ("fsd9");
  find ("   OS_Find &4F (input, error if not found)   RISC OS: ERROR &D6", 0x4F, "fsd9");
  h = find ("   OS_Find &CF (update, error if not found)  RISC OS: ERROR &D6", 0xCF, "fsd9");
  if (h) closeh (h);
  t = objtype ("fsd9", 0);
  printf ("   the file after that (0 = not there, 1 = a file)           RISC OS: 0, here: %lu\n", t);
  remove_ ("fsd9");
  printf ("2. the extent of a stream open for output (RISC OS: OS_Args 3 sets it and the stream is that long; OS_Args 1 beyond the end of a stream open for update makes it longer)\n");
  remove_ ("fsd2");
  h = find ("   OS_Find &8F (output) of a new file", 0x8F, "fsd2");
  writeb (h, "0123456789abcdef0123456789abcdef", 32);
  args ("   OS_Args 2 (read the extent)   RISC OS: 32", 2, h, 0);
  args ("   OS_Args 3 (set the extent to 495)", 3, h, 495);
  args ("   OS_Args 2 (read the extent)   RISC OS: 495", 2, h, 0);
  args ("   OS_Args 1 (set the pointer to 495)", 1, h, 495);
  args ("   OS_Args 0 (read the pointer)  RISC OS: 495", 0, h, 0);
  closeh (h);
  h = find ("   OS_Find &CF (update) of the same file", 0xCF, "fsd2");
  args ("   OS_Args 2 (read the extent)   RISC OS: 495", 2, h, 0);
  args ("   OS_Args 1 (pointer 600: beyond the end of an update stream)", 1, h, 600);
  args ("   OS_Args 2 (read the extent)   RISC OS: 600 (longer, zeros)", 2, h, 0);
  closeh (h);
  printf ("3. a locked file, a read-only file (RISC OS: only a locked file cannot be renamed or deleted; attribute 0x0B is locked, 0x01 is read only, 0x00 is none)\n");
  mkfile ("fsd3", 10);
  attrs ("fsd3", 0x0B);
  rename_ ("   rename of a locked file (0x0B)   RISC OS: ERROR &C3 locked", "fsd3", "fsd4");
  rename_ ("   (and back, if it was renamed)", "fsd4", "fsd3");
  delete ("   delete of a locked file (0x0B)   RISC OS: ERROR &C3 locked", "fsd3");
  mkfile ("fsd3", 10);
  attrs ("fsd3", 0x01);
  delete ("   delete of a read-only file (0x01) RISC OS: ok", "fsd3");
  remove_ ("fsd3");
  mkfile ("fsd3", 10);
  attrs ("fsd3", 0x00);
  delete ("   delete of a file without attributes (0x00) RISC OS: (not tried)", "fsd3");
  remove_ ("fsd3");
  printf ("4. a locked file (0x0B) and a read-only file (0x01), opened for output or update, 10 bytes, one byte written (RISC OS: a handle, the status is &140 (output) or &40 (update): not writable,\n"
	  "   the write gives ERROR &C1 \"Not open for update\", and the file is not changed: length 10)\n");
  open_test ("   OS_Find &8F (output) of a locked file", 0x0B, 0x8F);
  open_test ("   OS_Find &CF (update) of a locked file", 0x0B, 0xCF);
  open_test ("   OS_Find &8F (output) of a read-only file", 0x01, 0x8F);
  open_test ("   OS_Find &CF (update) of a read-only file", 0x01, 0xCF);
  printf ("5. a stream open for reading: the pointer beyond the end (RISC OS: ERROR &B7 Outside file)\n");
  mkfile ("fsd5", 10);
  h = find ("   OS_Find &4F (input)", 0x4F, "fsd5");
  args ("   OS_Args 1 (pointer 110 on a file of 10 bytes)  RISC OS: ERROR &B7", 1, h, 110);
  closeh (h);
  remove_ ("fsd5");
  remove_ ("fsd2");
  remove_ ("fsd1");
  return 0;
}
