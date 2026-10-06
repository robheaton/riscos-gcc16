/* sulfile -- which SharedUnixLibrary module is a FILE, and where is that file really?  READ-ONLY: it opens files for reading and changes nothing except one system variable when asked.
   The scripts of the SharedULibFix package (Install, Restore, Check) use it to refuse to replace a file they do not know, and to find the file that has to be replaced.

   usage:  sulfile FILE               FILE is any RISC OS file name; a path variable name such as System:Modules.SharedULib is fine
           sulfile --real VAR FILE    also finds the REAL name of the file that FILE finds (System:Modules.SharedULib can be in any of the directories of System$Path), checks that the file
                                      with that name is the very same file (same size, same hash), prints the name and sets the system variable VAR to it.  VAR is deleted first, so
                                      after a failure it is not set (an Obey file tests  If "<VAR>" = "" ).
   It prints the size, the file type, the attributes, the FNV-1a hash of the whole file and what that is (the table below: the STOCK 1.16 module, or the fixed 1.16-vforkfix3).  A file that is
   not in the table is shown with the title and help string of its module header (when it looks like a module).
   exit status:  0 = the STOCK SharedUnixLibrary 1.16 (3 Apr 2020)      2 = 1.16-vforkfix3 (the fixed module)      3 = another file      1 = the file cannot be found or read completely, or (--real)
   the real name cannot be found.  (The status stays below 4: the Obey files test it with  If "<Sys$ReturnCode>" ...  and a return code limit would turn a bigger one into an error.)  */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct known { const char *what; unsigned size, fnv; int code; };
static const struct known table[] = {
  { "the STOCK SharedUnixLibrary 1.16 (3 Apr 2020), unfixed: the module that RISC OS ships", 3192, 0x20b60ed1u, 0 },
  { "SharedUnixLibrary 1.16-vforkfix3 (4 Oct 2026): the FIXED module", 3228, 0xefc7dfeau, 2 },
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
  snprintf (text, textn, "= NOT a file this tool knows%s%s%s", hdr[0] ? ": its module header says \"" : " (it does not look like a module)", hdr, hdr[0] ? "\"" : "");
  return 3;
}

/* ---- finding the real name of a file that a path variable finds: the pure parts (they are tested on the host) ---- */

/* "System:Modules.SharedULib" -> VAR = "System", REST = "Modules.SharedULib".  Returns 0 when NAME does not start with a path variable name ("ADFS::4.$.x" and "ADFS:$.x" do not). */
static int split_pathvar (const char *name, char *var, size_t varn, const char **rest)
{
  size_t i = 0;
  while (name[i] && strchr (":.$&@%\\^*#", name[i]) == NULL && i + 1 < varn) { var[i] = name[i]; i++; }
  if (i == 0 || name[i] != ':' || name[i + 1] == ':' || name[i + 1] == '$' || name[i + 1] == '&' || name[i + 1] == '@' || name[i + 1] == 0) return 0;
  var[i] = 0;
  *rest = name + i + 1;
  return 1;
}

/* the next element of a comma separated list of directories: copied to EL, without the comma.  Returns the place after it, or NULL at the end of the list. */
static const char *next_element (const char *p, char *el, size_t n)
{
  if (*p == 0) return NULL;
  const char *c = strchr (p, ',');
  size_t l = c ? (size_t) (c - p) : strlen (p);
  if (l >= n) l = n - 1;
  memcpy (el, p, l);
  el[l] = 0;
  return c ? c + 1 : p + strlen (p);
}

/* ELEMENT and REST as one file name.  An element names a directory and ends in '.' or ':'; if it does not, a '.' is put between.  0 when it does not fit or the element is empty. */
static int join_name (const char *el, const char *rest, char *out, size_t n)
{
  size_t l = strlen (el);
  if (l == 0) return 0;
  int sep = el[l - 1] != '.' && el[l - 1] != ':';
  int w = snprintf (out, n, "%s%s%s", el, sep ? "." : "", rest);
  return w > 0 && (size_t) w < n;
}

#ifndef SULFILE_HOST
#include <errno.h>
#include <kernel.h>
#include <swis.h>

/* OS_File 5: 0 = nothing there, 1 = a file, 2 = a directory, -1 = an error (a path variable that is not set, a disc that is not there ...) */
static int object_type (const char *name)
{
  unsigned type = 0;
  _kernel_oserror *e = _swix (OS_File, _INR (0, 1) | _OUT (0), 5, name, &type);
  return e != NULL ? -1 : (int) type;
}

/* OS_FSControl 37: the full name of NAME (the file system, the disc and every directory, no path variable), 0 when it does not fit or the call fails */
static int canonical (const char *name, char *out, size_t n)
{
  int spare = 0;
  _kernel_oserror *e = _swix (OS_FSControl, _INR (0, 5) | _OUT (5), 37, name, out, 0, 0, (int) n, &spare);
  return e == NULL && spare >= 0 && out[0] != 0;
}

/* the real name of the file that NAME finds.  A name that starts with a path variable (System:...) is looked for in the directories of <var>$Path, the first one that has it is the one RISC OS
   finds; any other name is only given its full form. */
static int real_name (const char *name, char *real, size_t n)
{
  char var[64], vn[80], list[2048];
  const char *rest;
  if (!split_pathvar (name, var, sizeof var, &rest)) return canonical (name, real, n);
  snprintf (vn, sizeof vn, "%s$Path", var);
  int len = 0;
  _kernel_oserror *e = _swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), vn, list, (int) sizeof list - 1, 0, 0, &len);
  if (e != NULL || len <= 0 || len >= (int) sizeof list) { printf ("sulfile: the system variable %s is not set (or too long to read)\n", vn); return 0; }
  list[len] = 0;
  char el[512], cand[768];
  for (const char *p = list; (p = next_element (p, el, sizeof el)) != NULL; ) {
    if (!join_name (el, rest, cand, sizeof cand)) continue;
    if (object_type (cand) != 1) continue;
    return canonical (cand, real, n);
  }
  printf ("sulfile: no directory of %s has %s\n", vn, rest);
  return 0;
}

/* the whole file, read for reading only.  Prints why it cannot.  Returns the buffer (the caller frees it) or NULL with *STATUS = 1 (cannot be found or read) or 3 (empty or far too big for
   this module: not a file that this tool knows); *LEN, *ATTR and *LOAD_ADDR are the catalogue information */
static unsigned char *load (const char *path, unsigned *len, unsigned *attr, unsigned *load_addr, int *how, int *status)
{
  *status = 1;
  unsigned type = 0, load_a = 0, exec = 0, size = 0, at = 0;
  /* the catalogue information (OS_File 5: read-only; a path variable name such as System:Modules.SharedULib is fine): file type and attributes */
  _kernel_oserror *e = _swix (OS_File, _INR (0, 1) | _OUT (0) | _OUTR (2, 5), 5, path, &type, &load_a, &exec, &size, &at);
  if (e != NULL) { printf ("sulfile: %s: %s\n", path, e->errmess); return NULL; }
  if (type == 0) { printf ("sulfile: %s: not found\n", path); return NULL; }
  if (type != 1) { printf ("sulfile: %s: is a directory, not a file\n", path); return NULL; }
  if (size == 0 || size > 1000000u) { printf ("sulfile: %s: %u bytes: not a SharedUnixLibrary module (empty or far too big)\n", path, size); *status = 3; return NULL; }
  unsigned char *buf = malloc (size);
  if (buf == NULL) { printf ("sulfile: out of memory\n"); return NULL; }
  *how = 0;                                                              /* 0: fopen, 1: OS_File 255 (load named file) as the fallback */
  FILE *f = fopen (path, "rb");
  if (f != NULL) {
    size_t got = fread (buf, 1, size, f);
    int extra = fgetc (f);                                               /* the file must end where the catalogue says it does */
    fclose (f);
    if (got != size || extra != EOF) { printf ("sulfile: %s: read %zu of %u bytes: cannot read it completely\n", path, got, size); free (buf); return NULL; }
  } else {
    *how = 1;
    e = _swix (OS_File, _INR (0, 3), 255, path, buf, 0);                  /* load the named file at BUF (R3 = 0: at the address in R2) */
    if (e != NULL) { printf ("sulfile: %s: cannot open or load it: %s (%s)\n", path, strerror (errno), e->errmess); free (buf); return NULL; }
  }
  *len = size; *attr = at; *load_addr = load_a;
  return buf;
}

int main (int argc, char **argv)
{
  const char *var = NULL, *path = NULL;
  if (argc == 2 && argv[1][0] != '-') path = argv[1];
  else if (argc == 4 && !strcmp (argv[1], "--real")) { var = argv[2]; path = argv[3]; }
  else { fprintf (stderr, "usage: sulfile FILE\n       sulfile --real VARIABLE FILE\n"); return 1; }
  if (var != NULL) _swix (OS_SetVarVal, _INR (0, 4), var, "", -1, 0, 0);       /* delete it: after a failure it must not hold an old answer */
  unsigned len = 0, attr = 0, load_addr = 0; int how = 0, status = 1;
  unsigned char *buf = load (path, &len, &attr, &load_addr, &how, &status);
  if (buf == NULL) return status;
  unsigned h; char text[400];
  int code = identify (buf, len, &h, text, sizeof text);
  char attrs[16]; char *a = attrs;
  if (attr & 8) *a++ = 'L';
  if (attr & 2) *a++ = 'W';
  if (attr & 1) *a++ = 'R';
  *a++ = '/'; if (attr & 32) *a++ = 'W'; if (attr & 16) *a++ = 'R'; *a = 0;
  printf ("sulfile: %s: %u bytes, file type %s%03X, attributes %s%s, FNV-1a %08x\n", path, len, (load_addr >> 20) == 0xFFF ? "&" : "(none) ", (load_addr >> 20) == 0xFFF ? (load_addr >> 8) & 0xFFF : 0, attrs, (attr & 8) ? " (LOCKED)" : "", h);
  printf ("sulfile: %s%s\n", text, how ? "   (read with OS_File 255)" : "");
  if (var != NULL) {
    char real[512];
    if (!real_name (path, real, sizeof real)) { printf ("sulfile: cannot find the real name of %s: %s is NOT set\n", path, var); free (buf); return 1; }
    unsigned len2 = 0, attr2 = 0, load2 = 0; int how2 = 0, status2 = 1;
    unsigned char *buf2 = load (real, &len2, &attr2, &load2, &how2, &status2);
    unsigned h2 = 0; char text2[400];
    if (buf2 == NULL || len2 != len || (identify (buf2, len2, &h2, text2, sizeof text2), h2 != h)) {
      printf ("sulfile: the file %s is not the file that %s finds (or cannot be read): %s is NOT set\n", real, path, var);
      free (buf); free (buf2); return 1;
    }
    printf ("sulfile: the real name of %s is %s (the same %u bytes, the same hash)\n", path, real, len);
    _kernel_oserror *e = _swix (OS_SetVarVal, _INR (0, 4), var, real, (int) strlen (real), 0, 0);
    if (e != NULL) { printf ("sulfile: cannot set %s: %s\n", var, e->errmess); free (buf); free (buf2); return 1; }
    free (buf2);
  }
  free (buf);
  return code;
}
#endif
