/* modreloc - the flat module image from a module ELF file (the C version of modkit/bin/modreloc.py: the same output, byte for byte; no Python, no readelf / objcopy / nm).

     modreloc [-q] ELF OUT [READELF]      read ELF, write the module image to OUT (OUT may be ELF itself)
     modreloc [-q] --driver FILE          what  gcc -mmodule  runs after the link: FILE is the output file of the link.  An ELF file is replaced by the module image, as the linker of
                                          GCCSDK 4.7.4 wrote one; a file whose name ends in .elf is left alone (name the output X.elf to keep the ELF file for a debugger or a simulation),
                                          and so is a file that is not an ELF file (a partial link with -r, or a file that is a module already)
     -q  no message.   (READELF is accepted and ignored: the ELF file is read here.)

The ELF file is linked at address 0 with --emit-relocs and the linker script of modkit: one section .image (header, code, data, bss) and the relocations that refer to it.  Every R_ARM_ABS32 /
R_ARM_TARGET1 relocation (a word that holds an address) goes into a table of image offsets that is appended to the image; the two words reloc_info (offset of the table, number of entries) of the module
header are filled in.  The init veneer of the module adds the load address to every listed word.  PC relative relocations were resolved by the linker; any other kind (movw / movt addresses) is an error. */
#include <stdlib.h>
#include <string.h>
#if defined (__riscos__)
#include <kernel.h>
#include <unixlib/local.h>
#else
#include <sys/stat.h>
#endif
#include "modcommon.h"

/* On RISC OS the file of a module has the file type &FFA (Module).  A ,ffa at the end of a Unix name is only the file type when a program asks UnixLib to read it that way (ld does not), so the output of
   the link is named plainly (gcc -mmodule -o MyModule ...) and this sets the type of the file: OS_File 18.  On Linux the ,ffa of the name is the convention of the file server. */
static void set_module_type (const char *path)
{
#if defined (__riscos__)
  char name[1024];
  int filetype;
  if (__riscosify_std (path, 0, name, sizeof name, &filetype))
    {
      _kernel_swi_regs r;
      _kernel_oserror *e;
      r.r[0] = 18; r.r[1] = (int) name; r.r[2] = 0xFFA;
      e = _kernel_swi (8, &r, &r);                          /* OS_File 18: set the file type of the file */
      if (e) fprintf (stderr, "%s: cannot set the file type of %s to &FFA: %s\n", progname, path, e->errmess);
    }
#else
  (void) path;
#endif
}

static int cmp_u32 (const void *a, const void *b)
{
  unsigned x = *(const unsigned *) a, y = *(const unsigned *) b;
  return x < y ? -1 : x > y;
}

static const char *reloc_name (unsigned t)
{
  static char buf[8][24];
  static int k;
  char *b;
  switch (t)
    {
    case 0: return "R_ARM_NONE";
    case 1: return "R_ARM_PC24";
    case 2: return "R_ARM_ABS32";
    case 3: return "R_ARM_REL32";
    case 28: return "R_ARM_CALL";
    case 29: return "R_ARM_JUMP24";
    case 30: case 31: return "R_ARM_THM_JUMP24";
    case 38: return "R_ARM_TARGET1";
    case 40: return "R_ARM_V4BX";
    case 42: return "R_ARM_PREL31";
    case 43: return "R_ARM_MOVW_ABS_NC";
    case 44: return "R_ARM_MOVT_ABS";
    case 45: return "R_ARM_MOVW_PREL_NC";
    case 46: return "R_ARM_MOVT_PREL";
    case 47: return "R_ARM_THM_MOVW_ABS_NC";
    case 48: return "R_ARM_THM_MOVT_ABS";
    }
  b = buf[k++ & 7];
  sprintf (b, "R_ARM_%u", t);
  return b;
}

static void convert (const char *elfpath, const char *outpath, int quiet)
{
  Elf e;
  int im, i;
  const ElfSec *s;
  size_t n, nrel = 0, nbad = 0, k;
  unsigned *offs, ri = 0;
  int have_ri = 0;
  unsigned char *out;
  char bad[400];
  size_t badlen = 0;

  elf_load (&e, elfpath);
  if (rd16 (e.data + 16) != 2) die ("%s is not a linked ELF file (a partial link, with -r?)", elfpath);
  im = elf_find_section (&e, ".image");
  if (im < 0) die ("%s has no .image section: it was not linked with the module linker script", elfpath);
  s = &e.sec[im];
  if (s->addr != 0) die ("%s: .image is at address %s, not 0", elfpath, hx (s->addr));
  n = s->size;
  if (n % 4) die ("%s: the size of .image is not a multiple of 4", elfpath);

  /* the relocations that refer to .image */
  for (i = 0; i < e.nsec; i++)
    if (e.sec[i].type == SHT_REL && e.sec[i].info == (unsigned) im) nrel += e.sec[i].size / 8;
  offs = xmalloc (sizeof (unsigned) * (nrel + 1));
  nrel = 0;
  bad[0] = 0;
  for (i = 0; i < e.nsec; i++)
    {
      const ElfSec *r = &e.sec[i];
      size_t j;
      if (r->type != SHT_REL || r->info != (unsigned) im) continue;
      for (j = 0; j + 8 <= r->size; j += 8)
        {
          const unsigned char *p = e.data + r->offset + j;
          unsigned off = rd32 (p), type = rd32 (p + 4) & 255;
          switch (type)
            {
            case 2: case 38:                                     /* R_ARM_ABS32, R_ARM_TARGET1: a word that holds an address */
              if (off + 4 > n) die ("%s: a relocation at %s is outside the image", elfpath, hx (off));
              offs[nrel++] = off;
              break;
            case 0: case 1: case 3: case 28: case 29: case 40: case 42:   /* NONE, PC24, REL32, CALL, JUMP24, V4BX, PREL31: PC relative, already resolved by the linker */
              break;
            default:
              if (nbad++ < 10 && badlen < sizeof bad - 60)
                badlen += (size_t) sprintf (bad + badlen, "%s(%u, %s)", badlen ? ", " : "", off, reloc_name (type));
            }
        }
    }
  if (nbad)
    die ("relocations the loader cannot apply (the image must be linked with only absolute words and PC-relative branches; no movw / movt addresses): %s%s", bad, nbad > 10 ? ", ..." : "");
  qsort (offs, nrel, sizeof (unsigned), cmp_u32);
  {
    size_t u = 0;
    for (k = 0; k < nrel; k++)
      if (k == 0 || offs[k] != offs[k - 1]) offs[u++] = offs[k];
    nrel = u;
  }

  /* reloc_info: the two words of the module header that tell the init veneer where the table is */
  for (i = 0; i < e.nsec && !have_ri; i++)
    {
      const ElfSec *y = &e.sec[i];
      size_t j;
      if (y->type != SHT_SYMTAB) continue;
      for (j = 0; j + 16 <= y->size; j += 16)
        {
          const unsigned char *p = e.data + y->offset + j;
          if (rd16 (p + 14) == 0) continue;                                  /* undefined */
          if (strcmp (elf_symname (&e, y, rd32 (p)), "reloc_info") == 0) { ri = rd32 (p + 4); have_ri = 1; break; }
        }
    }
  if (!have_ri) die ("%s has no reloc_info: it was not linked with the module header of cmunge", elfpath);
  if ((size_t) ri + 8 > n) die ("%s: reloc_info is outside the image", elfpath);

  out = xmalloc (n + 4 * nrel + 4);
  if (s->type == SHT_NOBITS) memset (out, 0, n); else memcpy (out, e.data + s->offset, n);
  for (k = 0; k < nrel; k++) wr32 (out + n + 4 * k, offs[k]);
  wr32 (out + ri, (unsigned) n);
  wr32 (out + ri + 4, (unsigned) nrel);
  write_file (outpath, out, n + 4 * nrel);                 /* last, and in place: nothing above has written anything, so a failure leaves OUT as it was */
#if !defined (__riscos__)
  chmod (outpath, 0644);
#endif
  set_module_type (outpath);
  if (!quiet) printf ("%s: %u bytes (image %u + table of %u words)\n", outpath, (unsigned) (n + 4 * nrel), (unsigned) n, (unsigned) nrel);
  free (out); free (offs); free (e.data); free (e.sec);
}

static void usage (FILE *f)
{
  fputs ("usage: modreloc [-q] ELF OUT [READELF]   |   modreloc [-q] --driver FILE\n"
         "  ELF OUT     the module image of the module ELF file ELF is written to OUT (OUT may be ELF)\n"
         "  --driver F  after the link of  gcc -mmodule : an ELF file F becomes the module image; F.elf, a file that is not ELF and a missing file are left alone\n"
         "  -q          no message\n", f);
}

int main (int argc, char **argv)
{
  int quiet = 0, i, npos = 0;
  const char *driver = NULL, *pos[3];
  progname = "modreloc";
  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "-q") == 0) quiet = 1;
      else if (strcmp (argv[i], "--driver") == 0)
        {
          if (++i >= argc) die ("--driver needs the name of the output file");
          driver = argv[i];
        }
      else if (strcmp (argv[i], "-h") == 0 || strcmp (argv[i], "--help") == 0) { usage (stdout); return 0; }
      else if (npos < 3) pos[npos++] = argv[i];
      else { usage (stderr); return 1; }
    }
  if (driver)
    {
      FILE *f;
      unsigned char magic[4];
      size_t got;
      if (npos) die ("--driver takes no other file");
      if (has_suffix_nocase (driver, ".elf")) return 0;
      f = fopen (driver, "rb");
      if (!f) return 0;
      {
        unsigned char head[18];
        got = fread (head, 1, 18, f);
        memcpy (magic, head, 4);
        fclose (f);
        if (got != 18 || memcmp (magic, "\177ELF", 4) != 0) return 0;
        if (rd16 (head + 16) != 2) return 0;                      /* not a linked executable (a partial link, -r): not a module */
      }
      convert (driver, driver, quiet);
      return 0;
    }
  if (npos < 2) { usage (stderr); return 1; }
  convert (pos[0], pos[1], quiet);
  return 0;
}
