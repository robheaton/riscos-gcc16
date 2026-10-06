/* host-main.c -- the whole of sulfile.c (main () included) on the build host, on a MODEL of the RISC OS file system and of the calls it makes:  OS_File 5 and 255, OS_FSControl 37 (canonicalise),
   OS_ReadVarVal and OS_SetVarVal, and fopen ().  The scripts Install, Restore and Check are run on the same model by tools/sim-sulfix.py.
   THE MODEL (the layout of the author's machine; what it cannot know is how RISC OS itself behaves, and the places where this file had to assume that are marked ASSUMED):
     SIM_ROOT/boot/!System/<rest>        = NVMe::NVMe.$.!Boot.Resources.!System.<rest>          (with the dots of <rest> as directory separators)
     SIM_ROOT/app/<rest>                 = ADFS::HardDisc4.$.Apps.Utilities.!SULFix.<rest>
     Sys:NNN.<rest>                      = the first of these (Sys$Path is the one directory NVMe::NVMe.$.!Boot.Resources.!System.)
     System:<rest>                       is looked for in the directories of System$Path in order (reading); canonicalised it becomes the FIRST directory + <rest> (ASSUMED: no search).
   SIM_VARS is a file of NAME=value lines: the system variables (System$Path, SULFix$Dir ...), which OS_SetVarVal changes.  <Name> in a file name is expanded by the file system.
   SIM_MISSING_DIR_ERR=1: OS_File 5 on a name whose directory does not exist is an error instead of "not found" (ASSUMED either way is possible).
   build:  gcc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -Ihost-stubs host-main.c -o /tmp/sulfile-model */
#define _GNU_SOURCE
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "host-stubs/kernel.h"

static char *sim_root, *sim_vars;
static const char BOOT[] = "NVMe::NVMe.$.!Boot.Resources.!System.", APP[] = "ADFS::HardDisc4.$.Apps.Utilities.!SULFix.";

static int var_get (const char *name, char *out, size_t n)
{
  FILE *f = fopen (sim_vars, "r"); if (!f) return 0;
  char line[4096]; size_t nl = strlen (name); int found = 0;
  while (fgets (line, sizeof line, f)) {
    line[strcspn (line, "\n")] = 0;
    if (strncmp (line, name, nl) == 0 && line[nl] == '=') { snprintf (out, n, "%s", line + nl + 1); found = 1; }
  }
  fclose (f);
  return found;
}
/* name = value; a NULL value deletes the variable */
static void var_set (const char *name, const char *value)
{
  char tmp[4096]; snprintf (tmp, sizeof tmp, "%s.new", sim_vars);
  FILE *f = fopen (sim_vars, "r"), *g = fopen (tmp, "w"); char line[4096]; size_t nl = strlen (name);
  while (f && fgets (line, sizeof line, f)) if (!(strncmp (line, name, nl) == 0 && line[nl] == '=')) fputs (line, g);
  if (value) fprintf (g, "%s=%s\n", name, value);
  if (f) fclose (f);
  fclose (g); rename (tmp, sim_vars);
}
static void expand (const char *in, char *out, size_t n)                         /* <Name> -> the value of the variable (empty when it is not set) */
{
  size_t o = 0;
  for (const char *p = in; *p && o + 1 < n; ) {
    if (*p == '<' && strchr (p, '>')) {
      char name[256], val[2048]; size_t l = (size_t) (strchr (p, '>') - p - 1);
      if (l < sizeof name) { memcpy (name, p + 1, l); name[l] = 0; if (!var_get (name, val, sizeof val)) val[0] = 0; for (char *v = val; *v && o + 1 < n; v++) out[o++] = *v; p += l + 2; continue; }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
}
static void dots (char *s) { for (; *s; s++) if (*s == '.') *s = '/'; }

/* a (possibly System:) RISC OS name -> the host path; 0 when the model does not know it.  SEARCH: System: is looked for along System$Path (an object must exist) */
static int to_host (const char *name0, char *out, size_t n, int search)
{
  char name[2048]; expand (name0, name, sizeof name);
  if (strncmp (name, "Sys:", 4) == 0) { char t[2048]; snprintf (t, sizeof t, "%s%s", BOOT, name + 4); snprintf (name, sizeof name, "%s", t); }
  if (strncmp (name, BOOT, sizeof BOOT - 1) == 0) { char rest[2048]; snprintf (rest, sizeof rest, "%s", name + sizeof BOOT - 1); dots (rest); snprintf (out, n, "%s/boot/!System/%s", sim_root, rest); return 1; }
  if (strncmp (name, APP, sizeof APP - 1) == 0) { char rest[2048]; snprintf (rest, sizeof rest, "%s", name + sizeof APP - 1); dots (rest); snprintf (out, n, "%s/app/%s", sim_root, rest); return 1; }
  if (strncmp (name, "System:", 7) == 0) {
    char list[2048], el[512], cand[2560], h[2560];
    if (!var_get ("System$Path", list, sizeof list)) return 0;
    for (char *p = list; *p; ) {
      char *c = strchr (p, ','); size_t l = c ? (size_t) (c - p) : strlen (p); if (l >= sizeof el) l = sizeof el - 1;
      memcpy (el, p, l); el[l] = 0; p = c ? c + 1 : p + strlen (p);
      snprintf (cand, sizeof cand, "%s%s", el, name + 7);
      struct stat st; if (to_host (cand, h, sizeof h, 0) && stat (h, &st) == 0) { snprintf (out, n, "%s", h); return 1; }
      (void) search;
    }
    return 0;
  }
  return 0;
}
static FILE *sim_fopen (const char *path, const char *mode) { char h[2560]; if (!to_host (path, h, sizeof h, 1)) { errno = ENOENT; return NULL; } return fopen (h, mode); }
#define fopen sim_fopen

#define main sulfile_main
#include "sulfile.c"
#undef main

static _kernel_oserror err_not_found = { 214, "File not found (model)" }, err_bad = { 0, "Bad parameters (model)" }, err_overflow = { 0, "Buffer overflow (model)" };

_kernel_oserror *_swix (int swi, unsigned flags, ...)
{
  va_list ap; va_start (ap, flags);
  long r[10] = { 0 }; int isptr[10] = { 0 };
  switch (swi) {
    case OS_File: isptr[1] = 1; isptr[2] = 1; break;
    case OS_FSControl: isptr[1] = 1; isptr[2] = 1; break;
    case OS_ReadVarVal: isptr[0] = 1; isptr[1] = 1; break;
    case OS_SetVarVal: isptr[0] = 1; isptr[1] = 1; break;
    default: va_end (ap); return &err_bad;
  }
  for (int i = 0; i < 10; i++) if (flags & (1u << i)) r[i] = isptr[i] ? (long) va_arg (ap, void *) : (long) va_arg (ap, int);
  void *outp[10] = { 0 };
  for (int i = 0; i < 10; i++) if (flags & (0x10000u << i)) outp[i] = va_arg (ap, void *);
  va_end (ap);
  char h[2560];
  if (swi == OS_File && r[0] == 5) {
    const char *name = (const char *) r[1]; unsigned type = 0, len = 0;
    int known = to_host (name, h, sizeof h, 1); struct stat st;
    if (known && stat (h, &st) == 0) { type = S_ISDIR (st.st_mode) ? 2 : 1; len = (unsigned) st.st_size; }
    else if (getenv ("SIM_MISSING_DIR_ERR") && atoi (getenv ("SIM_MISSING_DIR_ERR"))) {
      char d[2560]; snprintf (d, sizeof d, "%s", known ? h : "/nonexistent/x"); char *sl = strrchr (d, '/'); if (sl) *sl = 0;
      if (!known || stat (d, &st) != 0) return &err_not_found;
    }
    if (outp[0]) *(unsigned *) outp[0] = type;
    if (outp[2]) *(unsigned *) outp[2] = 0xFFFFFA00u;             /* load address: file type FFA */
    if (outp[3]) *(unsigned *) outp[3] = 0;
    if (outp[4]) *(unsigned *) outp[4] = len;
    if (outp[5]) *(unsigned *) outp[5] = 0x13;                    /* WR/R */
    return NULL;
  }
  if (swi == OS_File && r[0] == 255) {
    if (!to_host ((const char *) r[1], h, sizeof h, 1)) return &err_not_found;
    FILE *f = fopen (h, "rb"); if (!f) return &err_not_found;
    size_t n = fread ((void *) r[2], 1, 1000000, f); (void) n; fclose (f); return NULL;
  }
  if (swi == OS_FSControl && r[0] == 37) {
    char name[2048], res[2560]; expand ((const char *) r[1], name, sizeof name);
    if (strncmp (name, "Sys:", 4) == 0) snprintf (res, sizeof res, "%s%s", BOOT, name + 4);
    else if (strncmp (name, "System:", 7) == 0) {                /* ASSUMED: the first directory of System$Path, no search */
      char list[2048], first[512]; if (!var_get ("System$Path", list, sizeof list)) return &err_bad;
      size_t l = strcspn (list, ","); if (l >= sizeof first) l = sizeof first - 1; memcpy (first, list, l); first[l] = 0;
      char t[2048]; snprintf (t, sizeof t, "%s%s", first, name + 7);
      if (strncmp (t, "Sys:", 4) == 0) snprintf (res, sizeof res, "%s%s", BOOT, t + 4); else snprintf (res, sizeof res, "%s", t);
    } else if (strncmp (name, BOOT, sizeof BOOT - 1) == 0 || strncmp (name, APP, sizeof APP - 1) == 0) snprintf (res, sizeof res, "%s", name);
    else return &err_not_found;
    size_t need = strlen (res) + 1; char *buf = (char *) r[2]; long size = r[5];
    if ((long) need > size) { if (outp[5]) *(int *) outp[5] = (int) (size - (long) need); return &err_overflow; }
    memcpy (buf, res, need); if (outp[5]) *(int *) outp[5] = (int) (size - (long) need);
    return NULL;
  }
  if (swi == OS_ReadVarVal) {
    char val[2048]; if (!var_get ((const char *) r[0], val, sizeof val)) return &err_not_found;
    size_t l = strlen (val); if ((long) l > r[2]) return &err_overflow;
    memcpy ((char *) r[1], val, l); if (outp[2]) *(int *) outp[2] = (int) l; return NULL;
  }
  if (swi == OS_SetVarVal) {
    if (r[2] < 0) { var_set ((const char *) r[0], NULL); return NULL; }
    char val[2048]; size_t l = (size_t) r[2]; if (l >= sizeof val) return &err_bad;
    memcpy (val, (const char *) r[1], l); val[l] = 0; var_set ((const char *) r[0], val); return NULL;
  }
  return &err_bad;
}

int main (int argc, char **argv)
{
  sim_root = getenv ("SIM_ROOT"); sim_vars = getenv ("SIM_VARS");
  if (!sim_root || !sim_vars) { fprintf (stderr, "host-main: SIM_ROOT and SIM_VARS must be set\n"); return 99; }
  return sulfile_main (argc, argv);
}
