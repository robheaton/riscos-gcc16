/* modreloc64.c - C version of modreloc64.py, for hosts without Python (32 bit RISC OS, where the AArch64 cross compiler runs).
 *
 *   modreloc64 [-q] ELF OUT      makes the flat image of a 64-bit module from the linked ELF file (see modreloc64.py for what it does and checks)
 *   modreloc64 -q ELF            the same, in place (the form that the driver uses as the post-link step of  aarch64-riscos-gcc -mmodule)
 *   modreloc64 --info ELF        prints what it found, writes nothing
 *
 * Same checks, same output as the Python tool (tests/modreloc64-compare.sh runs both on the module sources and compares the files).  Little-endian host assumed (x86, ARM, AArch64).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SHF_ALLOC 2
#define SHN_ABS 0xFFF1
#define ABS64 257

static unsigned char *D;
static size_t DLEN;
static int quiet, info;

static void die (const char *fmt, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
#include <stdarg.h>
static void die (const char *fmt, ...)
{
  va_list ap;
  fprintf (stderr, "modreloc64: ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

static uint16_t r16 (const unsigned char *p) { uint16_t v; memcpy (&v, p, 2); return v; }
static uint32_t r32 (const unsigned char *p) { uint32_t v; memcpy (&v, p, 4); return v; }
static uint64_t r64 (const unsigned char *p) { uint64_t v; memcpy (&v, p, 8); return v; }
static void w32 (unsigned char *p, uint32_t v) { memcpy (p, &v, 4); }

typedef struct { uint32_t nameoff, type, link, info; uint64_t flags, addr, off, size, entsize; char *name; } Sec;
typedef struct { char *name; uint64_t value; unsigned shndx; } Sym;

static Sec *secs; static unsigned nsec;
static Sym *syms; static size_t nsym;

static const char *cstr (uint64_t base, uint64_t off)
{
  if (base + off >= DLEN) die ("bad string offset");
  return (const char *) D + base + off;
}

static Sym *findsym (const char *name)
{
  size_t i;
  for (i = 0; i < nsym; i++)
    if (syms[i].name[0] && strcmp (syms[i].name, name) == 0)
      return &syms[i];
  return NULL;
}

static Sec *findsec (const char *name)
{
  unsigned i;
  for (i = 0; i < nsec; i++)
    if (strcmp (secs[i].name, name) == 0)
      return &secs[i];
  return NULL;
}

static int is_pcrel (unsigned t)
{
  switch (t)
    {
    case 260: case 261: case 262: case 273: case 274: case 275: case 276: case 277: case 278: case 279:
    case 280: case 282: case 283: case 284: case 285: case 286: case 299:
      return 1;
    }
  return 0;
}

static const char *reason (unsigned typ)
{
  if (typ == 311 || typ == 312 || typ == 1025 || typ == 1026 || typ == 1027)
    return "a GOT or dynamic relocation: position independent code that reaches an extern symbol through the GOT";
  if (typ >= 263 && typ <= 272)
    return "an address built with MOVZ/MOVK is absolute: use ADRP/ADD (the compiler's small code model does)";
  if (typ == 258 || typ == 259)
    return "a 32-bit or 16-bit word that holds an address cannot be relocated";
  if (typ >= 512)
    return "thread-local storage";
  return "no loader support";
}

static int cmp32 (const void *a, const void *b)
{
  uint32_t x = *(const uint32_t *) a, y = *(const uint32_t *) b;
  return x < y ? -1 : x > y;
}

int main (int argc, char **argv)
{
  const char *pos[2]; int npos = 0, i;
  FILE *f;
  unsigned shentsize, shnum, shstrndx;
  uint64_t shoff, base, size, rcoff, rcapoff, rtaboff;
  Sec *img;
  unsigned char *data;
  uint32_t *offs = NULL; size_t noffs = 0, capoffs = 0;
  int nbad = 0;
  uint32_t cap, zi;
  const char *outname;
  Sym *s_rc, *s_rcap, *s_rtab, *s_link, *s_zis, *s_zie;
  size_t k;
  unsigned si;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "-q") == 0) quiet = 1;
      else if (strcmp (argv[i], "--info") == 0) info = 1;
      else if (npos < 2) pos[npos++] = argv[i];
      else die ("too many arguments");
    }
  /* the post-link form: one name, the file is rewritten */
  if (!info && npos == 1)
    {
      static char tmp[1100];
      snprintf (tmp, sizeof tmp, "%s.tmp", pos[0]);
      pos[1] = pos[0];
      outname = tmp;
    }
  else
    outname = pos[1];
  if ((info && npos != 1) || (!info && npos < 1))
    {
      fprintf (stderr, "usage: modreloc64 [-q] ELF [OUT]   |   modreloc64 --info ELF\n");
      return 2;
    }

  f = fopen (pos[0], "rb");
  if (!f) die ("cannot open %s", pos[0]);
  fseek (f, 0, SEEK_END); DLEN = (size_t) ftell (f); fseek (f, 0, SEEK_SET);
  D = malloc (DLEN + 1);
  if (!D || fread (D, 1, DLEN, f) != DLEN) die ("cannot read %s", pos[0]);
  fclose (f);

  if (DLEN < 64 || memcmp (D, "\177ELF", 4) != 0 || D[4] != 2 || D[5] != 1)
    die ("%s is not a little-endian ELF64 file", pos[0]);
  if (r16 (D + 18) != 183) die ("%s is not an AArch64 file (machine %d)", pos[0], r16 (D + 18));
  if (r16 (D + 16) != 2) die ("%s is not a linked file (a partial link, -r?): link it with module64.ld (or mod64d.ld) first", pos[0]);
  shoff = r64 (D + 40);
  shentsize = r16 (D + 58); shnum = r16 (D + 60); shstrndx = r16 (D + 62);
  nsec = shnum;
  secs = calloc (nsec, sizeof (Sec));
  for (si = 0; si < nsec; si++)
    {
      const unsigned char *p = D + shoff + (uint64_t) si * shentsize;
      secs[si].nameoff = r32 (p); secs[si].type = r32 (p + 4); secs[si].flags = r64 (p + 8); secs[si].addr = r64 (p + 16);
      secs[si].off = r64 (p + 24); secs[si].size = r64 (p + 32); secs[si].link = r32 (p + 40); secs[si].info = r32 (p + 44); secs[si].entsize = r64 (p + 56);
    }
  for (si = 0; si < nsec; si++)
    secs[si].name = (char *) cstr (secs[shstrndx].off, secs[si].nameoff);
  for (si = 0; si < nsec; si++)
    if (secs[si].type == 2)
      {
        Sec *st = &secs[secs[si].link];
        size_t n = secs[si].size / 24;
        syms = calloc (n, sizeof (Sym)); nsym = n;
        for (k = 0; k < n; k++)
          {
            const unsigned char *p = D + secs[si].off + k * 24;
            syms[k].name = (char *) cstr (st->off, r32 (p));
            syms[k].shndx = r16 (p + 6);
            syms[k].value = r64 (p + 8);
          }
      }

  img = findsec (".image");
  if (!img || img->type != 1) die ("%s has no .image section: it was not linked with module64.ld (or mod64d.ld)", pos[0]);
  for (si = 0; si < nsec; si++)
    if ((strcmp (secs[si].name, ".got") == 0 || strcmp (secs[si].name, ".got.plt") == 0) && secs[si].size)
      die ("%s has a GOT (%s, %llu bytes): the code was compiled position independent and reaches a symbol through it. Compile with -fPIC -fvisibility=hidden, and declare extern symbols hidden",
           pos[0], secs[si].name, (unsigned long long) secs[si].size);
  base = img->addr; size = img->size;
  if ((base & 0xFFF) != 4) die ("the image is linked at &%llX: it must be at a page boundary plus 4 (the module is loaded there)", (unsigned long long) base);
  if ((base + size) % 16) die ("the image ends at &%llX: that must be a multiple of 16, because the kernel puts the zero-initialised area right after the file", (unsigned long long) (base + size));
  data = malloc (size + 16);
  memcpy (data, D + img->off, size);

  for (si = 0; si < nsec; si++)
    {
      Sec *r = &secs[si], *tgt, *symsec;
      size_t n;
      if (r->type != 4) continue;
      tgt = &secs[r->info];
      if (!(tgt->flags & SHF_ALLOC) || tgt->addr < base || tgt->addr >= base + size) continue;
      symsec = &secs[r->link];
      n = r->size / 24;
      for (k = 0; k < n; k++)
        {
          const unsigned char *p = D + r->off + k * 24;
          uint64_t roff = r64 (p), rinfo = r64 (p + 8);
          int64_t addend = (int64_t) r64 (p + 16);
          unsigned typ = (unsigned) (rinfo & 0xFFFFFFFFu);
          uint32_t symi = (uint32_t) (rinfo >> 32);
          int isabs = 0, undef = 0;
          uint64_t value = 0;
          const char *name = "";
          if (symi)
            {
              const unsigned char *sp = D + symsec->off + (uint64_t) symi * 24;
              unsigned shndx = r16 (sp + 6);
              name = cstr (secs[symsec->link].off, r32 (sp));
              value = r64 (sp + 8);
              isabs = shndx == SHN_ABS; undef = shndx == 0;
            }
          if (typ == 0) continue;
          if (is_pcrel (typ))
            {
              if (symi && (isabs || undef))
                {
                  fprintf (stderr, "modreloc64: &%llX PC-relative reference against %s, which is %s: wrong after the image is loaded somewhere else\n", (unsigned long long) roff, name, isabs ? "an absolute symbol" : "undefined");
                  nbad++;
                }
              continue;
            }
          if (typ == ABS64)
            {
              uint64_t held;
              if (isabs) continue;
              if (!(base <= roff && roff < base + size) || roff % 8)
                { fprintf (stderr, "modreloc64: &%llX R_AARCH64_ABS64: the word is not inside the image or not 8-byte aligned\n", (unsigned long long) roff); nbad++; continue; }
              held = r64 (data + (roff - base));
              if (held != (uint64_t) ((int64_t) value + addend))
                { fprintf (stderr, "modreloc64: &%llX R_AARCH64_ABS64: the word holds &%llX, the relocation says &%llX (%s%+lld)\n", (unsigned long long) roff, (unsigned long long) held,
                           (unsigned long long) (value + addend), name, (long long) addend); nbad++; continue; }
              if (noffs == capoffs) { capoffs = capoffs ? capoffs * 2 : 256; offs = realloc (offs, capoffs * 4); }
              offs[noffs++] = (uint32_t) (roff - base);
            }
          else if ((typ == 258 || typ == 259) && isabs)
            continue;
          else
            {
              fprintf (stderr, "modreloc64: &%llX relocation type %u against %s: not allowed in a module image (%s)\n", (unsigned long long) roff, typ, *name ? name : "a section", reason (typ));
              nbad++;
            }
        }
    }
  if (nbad) die ("%s: %d relocation(s) that the module's loader cannot apply", pos[0], nbad);
  qsort (offs, noffs, 4, cmp32);
  { size_t j = 0; for (k = 0; k < noffs; k++) if (j == 0 || offs[j - 1] != offs[k]) offs[j++] = offs[k]; noffs = j; }

  s_rc = findsym ("reloc_count"); s_rcap = findsym ("reloc_cap"); s_rtab = findsym ("reloc_table");
  s_link = findsym ("link_addr"); s_zis = findsym ("__ZIStart"); s_zie = findsym ("__ZIEnd");
  if (!s_rc || !s_rcap || !s_rtab || !findsym ("_mod_start") || !s_link || !findsym ("__image_end") || !s_zis || !s_zie)
    die ("%s lacks one of reloc_count, reloc_cap, reloc_table, _mod_start, link_addr, __image_end, __ZIStart, __ZIEnd: it was not linked with the module header of mod64d_hdr.S", pos[0]);
  rcoff = s_rc->value - base; rcapoff = s_rcap->value - base; rtaboff = s_rtab->value - base;
  cap = r32 (data + rcapoff);
  zi = (uint32_t) (s_zie->value - s_zis->value);

  if (info)
    {
      printf ("image &%llX, %llu bytes, ZI %u bytes at &%llX, %lu relocations (room for %u), link_addr at +&%llX\n", (unsigned long long) base, (unsigned long long) size, zi,
              (unsigned long long) s_zis->value, (unsigned long) noffs, cap, (unsigned long long) (s_link->value - base));
      for (k = 0; k < noffs; k++)
        printf ("  +&%04X  (word at &%llX holds &%llX)\n", offs[k], (unsigned long long) (base + offs[k]), (unsigned long long) r64 (data + offs[k]));
      return 0;
    }

  /* a structural check of the module header */
  {
    int problems = 0;
    uint32_t h[13];
    size_t n = size;
    memcpy (h, data, 52);
#define PROB(...) do { fprintf (stderr, "modreloc64: " __VA_ARGS__); fputc ('\n', stderr); problems++; } while (0)
    if (n < 52) die ("the image is %lu bytes: no module header", (unsigned long) n);
    if (!(h[1] & 0x40000000u)) PROB ("bit 30 of the initialisation offset is not set (the entry is AArch64 code)");
    { uint32_t init = h[1] & 0x3FFFFFFFu;
      struct { const char *nm; uint32_t v; } c[] = { { "initialisation", init }, { "finalisation", h[2] }, { "service", h[3] }, { "title", h[4] }, { "help", h[5] }, { "SWI handler", h[8] }, { "flags", h[12] } };
      for (i = 0; i < 7; i++)
        if (c[i].v && !(c[i].v > 0 && c[i].v < n)) PROB ("the %s offset &%X is outside the image", c[i].nm, c[i].v);
    }
    if (h[6] && !(h[6] < n)) PROB ("the cmds offset &%X is outside the image", h[6]);
    if (h[9] && !(h[9] < n)) PROB ("the swinames offset &%X is outside the image", h[9]);
    if (h[10] && !(h[10] < n)) PROB ("the SWI decoding code offset &%X is outside the image", h[10]);
    if (h[11] && !(h[11] < n)) PROB ("the messages file offset &%X is outside the image", h[11]);
    if (h[12] > 0 && h[12] < n - 8)
      {
        uint32_t fl = r32 (data + h[12]), zz = r32 (data + h[12] + 4);
        if (fl != 0x14) PROB ("the flags word is &%X, not &14 (bit 0 clear, bit 2: zero-initialised size follows, architecture 1)", fl);
        if (zz != zi) PROB ("the zero-initialised size in the header is %u, the linker's is %u", zz, zi);
        if (s_zis->value != base + n) PROB ("the zero-initialised area is not right after the image");
      }
    else
      PROB ("no flags word");
    if (h[3] > 0 && h[3] < n && r32 (data + h[3]) == 0xD65F03C0u)
      PROB ("the service entry is a RET (the marker of a service table): the kit's entry is a plain one that filters the numbers itself (MOD_SERVICE_PLAIN)");
    if (h[6])
      {
        size_t o = h[6]; int nc = 0;
        while (o > 0 && o < n - 20 && data[o] != 0)
          {
            size_t e0 = o; uint32_t v[4];
            while (e0 < n && data[e0]) e0++;
            o = (e0 + 1 + 3) & ~(size_t) 3;
            if (o + 16 > n) break;
            memcpy (v, data + o, 16);
            if (!(v[0] > 0 && v[0] < n)) PROB ("command %d: the code offset &%X is outside the image", nc, v[0]);
            if (!(v[2] > 0 && v[2] < n)) PROB ("command %d: the syntax offset &%X is outside the image", nc, v[2]);
            if (!(v[3] > 0 && v[3] < n)) PROB ("command %d: the help offset &%X is outside the image", nc, v[3]);
            o += 16; nc++;
            if (o >= n - 4 || r32 (data + o) == 0) break;
          }
      }
    if (noffs > 0 && s_link->value - base >= n) PROB ("link_addr is outside the image");
    if (problems) die ("%s: the module header is wrong", pos[0]);
  }
  if (noffs > cap) die ("%lu words to relocate and room for %u: raise RELOC_CAP in mod64d_hdr.S", (unsigned long) noffs, cap);
  { int found = 0; for (k = 0; k < noffs; k++) if (offs[k] == s_link->value - base) found = 1;
    if (!found) die ("link_addr is not in the list of relocations: the header was not linked as expected"); }
  w32 (data + rcoff, (uint32_t) noffs);
  for (k = 0; k < noffs; k++) w32 (data + rtaboff + 4 * k, offs[k]);
  {
    uint32_t pad = getenv ("MOD64_NO_ZI_PAD") ? 0 : zi;
    FILE *o = fopen (outname, "wb");
    unsigned char *z;
    if (!o) die ("cannot write %s", outname);
    fwrite (data, 1, size, o);
    z = calloc (pad ? pad : 1, 1);
    if (pad) fwrite (z, 1, pad, o);
    if (fclose (o)) die ("write error on %s", outname);
    if (outname != pos[1])
      {
        /* in place: the temporary file replaces the original */
        remove (pos[1]);
        if (rename (outname, pos[1])) die ("cannot rename %s to %s", outname, pos[1]);
      }
    if (!quiet)
      printf ("%s: %lu bytes (image at &%llX, %lu relocations, zero-initialised area %u bytes%s)\n", pos[1], (unsigned long) (size + pad), (unsigned long long) base, (unsigned long) noffs, zi, pad ? " in the file" : "");
  }
  return 0;
}
