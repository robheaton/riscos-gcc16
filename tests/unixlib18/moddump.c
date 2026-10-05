/* moddump.c -- copy the image of a LOADED RISC OS module out of memory into a file.  Read-only: it changes nothing in the machine.   usage: moddump NAME OUTFILE     e.g.  moddump ARMEABISupport ARMEABISupport-loaded
   OS_Module 18 (look up a module by name) gives the base address of the module; the module was loaded into a block of the RMA heap, whose size is the word in front of the block (the heap keeps it there,
   size of the block including that word).  What is written is what runs: header, code, strings, tables and the module's own data as it is NOW (variables the module has changed are changed).  All reads are
   guarded against SIGSEGV.  Used to look at the ARMEABISupport and SharedUnixLibrary that are installed (their versions are not the ones of the sources on the build machine).  */
#define _GNU_SOURCE
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>

static sigjmp_buf jb;
static void segv(int s) { (void) s; siglongjmp(jb, 1); }
static __attribute__((noinline)) int read_words(unsigned addr, unsigned *dst, unsigned nwords)   /* 1 when all words could be read */
{
  struct sigaction sa, old; memset(&sa, 0, sizeof sa); sa.sa_handler = segv; sigaction(SIGSEGV, &sa, &old);
  int ok = 0;
  if (sigsetjmp(jb, 1) == 0) { for (unsigned i = 0; i < nwords; i++) dst[i] = ((const unsigned *) addr)[i]; ok = 1; }
  sigaction(SIGSEGV, &old, NULL);
  return ok;
}

int main(int argc, char **argv)
{
  if (argc < 3) { fprintf(stderr, "usage: moddump NAME OUTFILE\n"); return 2; }
  unsigned num = 0, inst = 0, base = 0;
  _kernel_oserror *e = _swix(OS_Module, _INR(0, 1) | _OUTR(1, 3), 18, argv[1], &num, &inst, &base);
  if (e) { printf("moddump: module \"%s\" not found: %s\n", argv[1], e->errmess); return 1; }
  unsigned blk = 0, hdr[10];
  if (!read_words(base, hdr, 10)) { printf("moddump: the header of \"%s\" (at 0x%08x) cannot be read from a user program\n", argv[1], base); return 1; }
  int have_size = read_words(base - 4, &blk, 1) && blk >= 0x400 && blk <= 0x400000 && (blk & 3) == 0;
  unsigned size = have_size ? blk - 4 : 0x18000;
  printf("moddump: %s: module number %u at 0x%08x, header words: start %x init %x final %x service %x title %x help %x; %s %u bytes\n", argv[1], num, base,
         hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], have_size ? "RMA block of" : "the size word is not plausible: dumping", size);
  unsigned *buf = malloc(size);
  if (!buf) { printf("moddump: out of memory\n"); return 1; }
  unsigned got = 0;
  for (unsigned off = 0; off < size; off += 1024) {                     /* in pieces of 1 KB, so that an unreadable end only cuts the dump short */
    unsigned n = size - off < 1024 ? size - off : 1024;
    if (!read_words(base + off, buf + off / 4, n / 4)) break;
    got = off + n;
  }
  FILE *f = fopen(argv[2], "wb");
  if (!f) { printf("moddump: cannot write %s\n", argv[2]); return 1; }
  fwrite(buf, 1, got, f); fclose(f);
  printf("moddump: wrote %u bytes to %s\n", got, argv[2]);
  return 0;
}
