/* iconv - convert text from one character set to another (the iconv command of POSIX, with the options of GNU iconv that scripts use) for RISC OS 64 (UnixLib 64).
   iconv [-f FROM] [-t TO] [-c] [-s] [-o FILE] [FILE...]      iconv -l
   -f, --from-code  character set of the input (default: that of the locale)    -t, --to-code  character set of the output (default: that of the locale)
   -c  leave out characters that cannot be converted (also the suffix //IGNORE)  -s  no messages about characters that cannot be converted
   -o, --output  write to FILE instead of the standard output                    -l, --list  the names of the character sets
   Written for the SDK of RISC OS 64; free to use (the same terms as the rest of the SDK). */
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *__ul64_iconv_name (int index);

#define INSZ 65536
#define OUTSZ 262144

static int silent, omit;

static int convert (iconv_t cd, FILE *in, FILE *out, const char *name)
{
  static char inb[INSZ + 16], outb[OUTSZ];
  size_t have = 0;				/* bytes in inb */
  long long pos = 0;				/* bytes of the input before inb */
  int status = 0, eof = 0;
  while (!eof || have)
    {
      char *ip = inb, *op = outb;
      size_t il, ol = OUTSZ, r;
      if (!eof)
	{
	  size_t n = fread (inb + have, 1, INSZ - have, in);
	  have += n;
	  if (n == 0) eof = 1;
	  if (ferror (in)) { fprintf (stderr, "iconv: cannot read %s\n", name); return 1; }
	}
      il = have;
      while (il > 0)
	{
	  op = outb; ol = OUTSZ;
	  r = iconv (cd, &ip, &il, &op, &ol);
	  if (op > outb && fwrite (outb, 1, op - outb, out) != (size_t) (op - outb)) { fprintf (stderr, "iconv: conversion stopped due to problem in writing the output\n"); return 1; }
	  if (r != (size_t) -1) { if (omit && r > 0) status = 1; break; }
	  if (errno == E2BIG) continue;
	  if (errno == EILSEQ)
	    {
	      if (omit) { ip++; il--; status = 1; continue; }
	      fprintf (stderr, "iconv: illegal input sequence at position %lld\n", pos + (ip - inb));
	      return 1;
	    }
	  if (errno == EINVAL)
	    {
	      if (!eof) break;			/* the rest of the character comes with the next block */
	      fprintf (stderr, "iconv: incomplete character or shift sequence at end of buffer\n");
	      return 1;
	    }
	  fprintf (stderr, "iconv: internal error (illegal descriptor)\n");
	  return 1;
	}
      /* the bytes not converted yet (an incomplete character) go to the front */
      pos += ip - inb;
      have = il;
      memmove (inb, ip, il);
      if (eof && il == 0) break;
      if (eof && il > 0 && !omit)
	{
	  fprintf (stderr, "iconv: incomplete character or shift sequence at end of buffer\n");
	  return 1;
	}
      if (eof) break;
    }
  /* reset (a shift sequence at the end, if the set has one) */
  {
    char *op = outb;
    size_t ol = OUTSZ;
    iconv (cd, 0, 0, &op, &ol);
    if (op > outb) fwrite (outb, 1, op - outb, out);
  }
  return status;
}

int main (int argc, char **argv)
{
  const char *from = 0, *to = 0, *outname = 0;
  char fromb[80], tob[80];
  int i, files = 0, rc = 0, list = 0;
  FILE *out = stdout;
  iconv_t cd;
  setlocale (LC_ALL, "");
  for (i = 1; i < argc; i++)
    {
      const char *a = argv[i];
      if (a[0] != '-' || !strcmp (a, "-")) break;
      if (!strcmp (a, "--")) { i++; break; }
      if (!strcmp (a, "-l") || !strcmp (a, "--list")) list = 1;
      else if (!strcmp (a, "-c")) omit = 1;
      else if (!strcmp (a, "-s") || !strcmp (a, "--silent")) silent = 1;
      else if (!strcmp (a, "--verbose")) ;
      else if (!strncmp (a, "--from-code=", 12)) from = a + 12;
      else if (!strncmp (a, "--to-code=", 10)) to = a + 10;
      else if (!strncmp (a, "--output=", 9)) outname = a + 9;
      else if (!strcmp (a, "-f") && i + 1 < argc) from = argv[++i];
      else if (!strcmp (a, "-t") && i + 1 < argc) to = argv[++i];
      else if (!strcmp (a, "-o") && i + 1 < argc) outname = argv[++i];
      else if (!strncmp (a, "-f", 2)) from = a + 2;
      else if (!strncmp (a, "-t", 2)) to = a + 2;
      else if (!strncmp (a, "-o", 2)) outname = a + 2;
      else if (!strcmp (a, "--version")) { puts ("iconv (UnixLib 64) 1.0"); return 0; }
      else if (!strcmp (a, "--help") || !strcmp (a, "-?"))
	{
	  puts ("Usage: iconv [-c] [-s] [-f FROM-ENCODING] [-t TO-ENCODING] [-o OUTPUT] [FILE...]\n       iconv -l");
	  return 0;
	}
      else { fprintf (stderr, "iconv: unrecognized option '%s'\n", a); return 1; }
    }
  if (list)
    {
      const char *n;
      for (i = 0; (n = __ul64_iconv_name (i)) != 0; i++) printf ("%s//\n", n);
      return 0;
    }
  if (!from) from = nl_langinfo (CODESET);
  if (!to) to = nl_langinfo (CODESET);
  if (omit)
    {
      snprintf (tob, sizeof tob, "%s//IGNORE", to);
      to = tob;
    }
  (void) fromb;
  cd = iconv_open (to, from);
  if (cd == (iconv_t) -1)
    {
      fprintf (stderr, errno == EINVAL ? "iconv: conversion from `%s' to `%s' is not supported\n" : "iconv: cannot open the conversion from `%s' to `%s'\n", from, to);
      return 1;
    }
  if (outname && !(out = fopen (outname, "wb"))) { fprintf (stderr, "iconv: cannot open output file %s\n", outname); return 1; }
  for (; i < argc; i++)
    {
      FILE *in = !strcmp (argv[i], "-") ? stdin : fopen (argv[i], "rb");
      files++;
      if (!in) { fprintf (stderr, "iconv: cannot open input file `%s'\n", argv[i]); rc = 1; continue; }
      if (convert (cd, in, out, argv[i])) rc = 1;
      if (in != stdin) fclose (in);
      if (rc && !omit) break;
    }
  if (!files && convert (cd, stdin, out, "(stdin)")) rc = 1;
  if (out != stdout) fclose (out); else fflush (stdout);
  iconv_close (cd);
  return rc;
}
