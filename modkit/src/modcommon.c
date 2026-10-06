/* modcommon.c - see modcommon.h */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "modcommon.h"

const char *progname = "modkit";

void die (const char *fmt, ...)
{
  va_list ap;
  fprintf (stderr, "%s: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

void *xmalloc (size_t n)
{
  void *p = malloc (n ? n : 1);
  if (!p) die ("out of memory");
  return p;
}

void *xrealloc (void *p, size_t n)
{
  p = realloc (p, n ? n : 1);
  if (!p) die ("out of memory");
  return p;
}

char *xstrdup (const char *s) { return xstrndup (s, strlen (s)); }

char *xstrndup (const char *s, size_t n)
{
  char *p = xmalloc (n + 1);
  memcpy (p, s, n);
  p[n] = 0;
  return p;
}

void buf_init (Buf *b) { b->s = xmalloc (64); b->s[0] = 0; b->len = 0; b->cap = 64; }

void buf_addn (Buf *b, const char *s, size_t n)
{
  if (b->len + n + 1 > b->cap)
    {
      while (b->len + n + 1 > b->cap) b->cap *= 2;
      b->s = xrealloc (b->s, b->cap);
    }
  memcpy (b->s + b->len, s, n);
  b->len += n;
  b->s[b->len] = 0;
}

void buf_adds (Buf *b, const char *s) { buf_addn (b, s, strlen (s)); }
void buf_addc (Buf *b, char c) { buf_addn (b, &c, 1); }

void buf_printf (Buf *b, const char *fmt, ...)
{
  va_list ap;
  char small[512];
  int n;
  va_start (ap, fmt);
  n = vsnprintf (small, sizeof small, fmt, ap);
  va_end (ap);
  if (n < 0) die ("formatting failed");
  if ((size_t) n < sizeof small) { buf_addn (b, small, (size_t) n); return; }
  {
    char *big = xmalloc ((size_t) n + 1);
    va_start (ap, fmt);
    vsnprintf (big, (size_t) n + 1, fmt, ap);
    va_end (ap);
    buf_addn (b, big, (size_t) n);
    free (big);
  }
}

unsigned char *read_file (const char *path, size_t *len)
{
  FILE *f = fopen (path, "rb");
  unsigned char *d;
  size_t n = 0, cap = 4096, r;
  if (!f) die ("cannot read %s: %s", path, strerror (errno));
  d = xmalloc (cap + 1);
  while ((r = fread (d + n, 1, cap - n, f)) > 0)
    {
      n += r;
      if (n == cap) { cap *= 2; d = xrealloc (d, cap + 1); }
    }
  if (ferror (f)) die ("cannot read %s: %s", path, strerror (errno));
  fclose (f);
  d[n] = 0;
  if (len) *len = n;
  return d;
}

void write_file (const char *path, const void *data, size_t len)
{
  FILE *f = fopen (path, "wb");
  if (!f) die ("cannot write %s: %s", path, strerror (errno));
  if (len && fwrite (data, 1, len, f) != len) die ("cannot write %s: %s", path, strerror (errno));
  if (fclose (f)) die ("cannot write %s: %s", path, strerror (errno));
}

const char *base_name (const char *path)
{
  const char *p = strrchr (path, '/');
  return p ? p + 1 : path;
}

const char *hx (unsigned v)
{
  static char buf[8][16];
  static int k;
  char *b = buf[k++ & 7];
  sprintf (b, "0x%x", v);
  return b;
}

int has_suffix_nocase (const char *s, const char *suffix)
{
  size_t a = strlen (s), b = strlen (suffix), i;
  if (a < b) return 0;
  for (i = 0; i < b; i++)
    if (tolower ((unsigned char) s[a - b + i]) != tolower ((unsigned char) suffix[i])) return 0;
  return 1;
}

/* ---------------------------------------------------------------- ELF32 little endian */
unsigned rd16 (const unsigned char *p) { return p[0] | (unsigned) p[1] << 8; }
unsigned rd32 (const unsigned char *p) { return p[0] | (unsigned) p[1] << 8 | (unsigned) p[2] << 16 | (unsigned) p[3] << 24; }
void wr32 (unsigned char *p, unsigned v) { p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = (v >> 24) & 255; }

void elf_load (Elf *e, const char *path)
{
  unsigned shoff, shentsize, shnum, shstrndx, strofs;
  int i;
  e->data = read_file (path, &e->len);
  if (e->len < 52 || memcmp (e->data, "\177ELF", 4) != 0) die ("%s is not an ELF file", path);
  if (e->data[4] != 1 || e->data[5] != 1) die ("%s is not a 32 bit little endian ELF file", path);
  shoff = rd32 (e->data + 0x20);
  shentsize = rd16 (e->data + 0x2E);
  shnum = rd16 (e->data + 0x30);
  shstrndx = rd16 (e->data + 0x32);
  if (shentsize < 40 || shoff == 0 || shnum == 0 || shoff + (size_t) shnum * shentsize > e->len || shstrndx >= shnum) die ("%s has no usable section table", path);
  e->nsec = (int) shnum;
  e->sec = xmalloc (sizeof (ElfSec) * shnum);
  for (i = 0; i < e->nsec; i++)
    {
      const unsigned char *h = e->data + shoff + (size_t) i * shentsize;
      e->sec[i].name = rd32 (h); e->sec[i].type = rd32 (h + 4); e->sec[i].flags = rd32 (h + 8); e->sec[i].addr = rd32 (h + 12);
      e->sec[i].offset = rd32 (h + 16); e->sec[i].size = rd32 (h + 20); e->sec[i].link = rd32 (h + 24); e->sec[i].info = rd32 (h + 28); e->sec[i].entsize = rd32 (h + 36);
      if (e->sec[i].type != SHT_NOBITS && (size_t) e->sec[i].offset + e->sec[i].size > e->len) die ("%s: section %d runs past the end of the file", path, i);
    }
  strofs = e->sec[shstrndx].offset;
  for (i = 0; i < e->nsec; i++)
    {
      if (strofs + e->sec[i].name >= e->len) die ("%s: bad section name", path);
      e->sec[i].namestr = (const char *) e->data + strofs + e->sec[i].name;
    }
}

int elf_find_section (const Elf *e, const char *name)
{
  int i;
  for (i = 0; i < e->nsec; i++)
    if (strcmp (e->sec[i].namestr, name) == 0) return i;
  return -1;
}

const char *elf_symname (const Elf *e, const ElfSec *symtab, unsigned strofs)
{
  const ElfSec *str;
  if (symtab->link >= (unsigned) e->nsec) die ("bad symbol table");
  str = &e->sec[symtab->link];
  if (strofs >= str->size) die ("bad symbol name");
  return (const char *) e->data + str->offset + strofs;
}
