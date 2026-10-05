/* sulfile.c -- say which SharedUnixLibrary module a FILE is (READ-ONLY: it opens the file for reading only, it changes nothing).  The installer InstallSul3 uses it to refuse to replace a file it does not know, and to check the
   backups and the copy it makes.   usage: sulfile FILE      FILE is any RISC OS name, a path variable name such as System:Modules.SharedULib included.
   It prints the size, the file type, the attributes, the FNV-1a hash of the whole file and what that is (the table below: the STOCK 1.16 module, the fixed 1.16-vforkfix3, or another build of ours); for a file it does
   not know it prints the title and help string from the module header (when the file looks like a module), so that I can see what it is.
   exit status:  0 = the STOCK SharedUnixLibrary 1.16 (3 Apr 2020)      2 = 1.16-vforkfix3 (the fixed module)      3 = another file (a test build of ours, or something else)      1 = the file cannot be read
   (the status stays below 4: the Obey files test it with  If "<Sys$ReturnCode>" ...  and a return code limit would turn a bigger one into an error.)  */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct known { const char *what; unsigned size, fnv; int code; };
static const struct known table[] = {
  { "the STOCK SharedUnixLibrary 1.16 (3 Apr 2020), unfixed: the module that is installed", 3192, 0x20b60ed1u, 0 },
  { "SharedUnixLibrary 1.16-vforkfix3 (4 Oct 2026): THE FIXED module", 3228, 0xefc7dfeau, 2 },
  { "a test build of ours: 1.16-vforkfix2 (3 Oct 2026), the loops still freeze with it", 3216, 0x99be9eccu, 3 },
  { "a test build of ours: 1.16-vforkfix1 (3 Oct 2026)", 3208, 0x607da736u, 3 },
  { "a test build of ours: 1.16-orig (the stock source with a version marker)", 3196, 0xe9ac4cb4u, 3 },
  { "a TRACED test build of ours: 1.16-vforkfix3t (a debugging aid, never to be installed)", 6484, 0x8833dd6eu, 3 },
  { "a TRACED test build of ours: 1.16-vforkfix2t (a debugging aid, never to be installed)", 6472, 0x1e6917e3u, 3 },
};

static unsigned fnv1a (const unsigned char *p, size_t n)
{
  unsigned h = 0x811c9dc5u;
  while (n--) h = (h ^ *p++) * 0x01000193u;
  return h;
}

/* the module header (word 4: the offset of the title, word 5: the offset of the help string), copied as printable text; "" when the file does not look like a module */
static void header_text (const unsigned char *b, size_t n, char *out, size_t outn)
{
  out[0] = 0;
  if (n < 24) return;
  unsigned t = b[16] | b[17] << 8 | b[18] << 16 | (unsigned) b[19] << 24, h = b[20] | b[21] << 8 | b[22] << 16 | (unsigned) b[23] << 24;
  if (t >= n || h >= n) return;
  size_t o = 0;
  for (unsigned i = t; i < n && b[i] >= 32 && b[i] < 127 && o + 2 < outn; i++) out[o++] = (char) b[i];
  if (o + 3 < outn) { out[o++] = ' '; out[o++] = '-'; out[o++] = ' '; }
  for (unsigned i = h; i < n && o + 2 < outn; i++) {
    if (b[i] == 0) break;
    out[o++] = b[i] == '\t' ? ' ' : (b[i] >= 32 && b[i] < 127 ? (char) b[i] : '?');
  }
  out[o] = 0;
}

/* what is it?  fills TEXT with one line (the description, or the header text of an unknown file); returns the exit status */
static int identify (const unsigned char *b, size_t n, unsigned *hash, char *text, size_t textn)
{
  *hash = fnv1a (b, n);
  for (size_t k = 0; k < sizeof table / sizeof table[0]; k++)
    if (table[k].size == n && table[k].fnv == *hash) { snprintf (text, textn, "= %s", table[k].what); return table[k].code; }
  char hdr[160]; header_text (b, n, hdr, sizeof hdr);
  snprintf (text, textn, "= NOT a file this installer knows%s%s%s", hdr[0] ? ": its module header says \"" : " (it does not look like a module)", hdr, hdr[0] ? "\"" : "");
  return 3;
}

#ifndef SULFILE_HOST
#include <errno.h>
#include <kernel.h>
#include <swis.h>

int main (int argc, char **argv)
{
  if (argc != 2) { fprintf (stderr, "usage: sulfile FILE\n"); return 1; }
  const char *path = argv[1];
  /* the catalogue information (OS_File 5: read-only; a path variable name such as System:Modules.SharedULib is fine): file type and attributes */
  unsigned type = 0, load = 0, exec = 0, len = 0, attr = 0;
  _kernel_oserror *e = _swix (OS_File, _INR (0, 1) | _OUT (0) | _OUTR (2, 5), 5, path, &type, &load, &exec, &len, &attr);
  if (e != NULL) { printf ("sulfile: %s: %s\n", path, e->errmess); return 1; }
  if (type == 0) { printf ("sulfile: %s: not found\n", path); return 1; }
  if (type != 1) { printf ("sulfile: %s: is a directory, not a file\n", path); return 1; }
  if (len == 0 || len > 1000000u) { printf ("sulfile: %s: %u bytes: not a module of ours (empty or far too big)\n", path, len); return 3; }
  unsigned char *buf = malloc (len);
  if (buf == NULL) { printf ("sulfile: out of memory\n"); return 1; }
  int how = 0;                                                           /* 0: fopen (ulinfo reads the libraries the same way), 1: OS_File 255 (load named file) as the fallback */
#ifdef SULFILE_TEST_NOFOPEN
  FILE *f = NULL;
#else
  FILE *f = fopen (path, "rb");
#endif
  if (f != NULL) {
    size_t got = fread (buf, 1, len, f);
    int extra = fgetc (f);                                               /* the file must end where the catalogue says it does */
    fclose (f);
    if (got != len || extra != EOF) { printf ("sulfile: %s: read %zu of %u bytes: cannot read it completely\n", path, got, len); free (buf); return 1; }
  } else {
    how = 1;
    e = _swix (OS_File, _INR (0, 3), 255, path, buf, 0);                  /* load the named file at BUF (R3 = 0: at the address in R2) */
    if (e != NULL) { printf ("sulfile: %s: cannot open or load it: %s (%s)\n", path, strerror (errno), e->errmess); free (buf); return 1; }
  }
  unsigned h; char text[400];
  int code = identify (buf, len, &h, text, sizeof text);
  char attrs[16]; char *a = attrs;
  if (attr & 8) *a++ = 'L';
  if (attr & 2) *a++ = 'W';
  if (attr & 1) *a++ = 'R';
  *a++ = '/'; if (attr & 32) *a++ = 'W'; if (attr & 16) *a++ = 'R'; *a = 0;
  printf ("sulfile: %s: %u bytes, file type %s%03X, attributes %s%s, FNV-1a %08x\n", path, len, (load >> 20) == 0xFFF ? "&" : "(none) ", (load >> 20) == 0xFFF ? (load >> 8) & 0xFFF : 0, attrs, (attr & 8) ? " (LOCKED)" : "", h);
  printf ("sulfile: %s%s\n", text, how ? "   (read with OS_File 255)" : "");
  free (buf);
  return code;
}
#endif
