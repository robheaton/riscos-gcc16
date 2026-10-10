/* tmpfile.c - tmpnam and tmpfile of <stdio.h>.  A name is  <Wimp$ScrapDir>.tmpXXXXXXX  (the scrap directory of the Wimp: FileSwitch expands the variable when the file is opened; seven hexadecimal digits keep the
   leaf name to ten characters, which every file system takes) with a number that starts somewhere different each time and counts up; a name is used only when OS_File 17 says that there is no object of
   that name.  tmpfile opens such a file "w+b" and removes it when the stream is closed (fclose, freopen, exit, __modlib_closeall): RISC OS cannot remove a file that is open, so the name is kept here and
   the stream says in its bits (B_TEMP) that fcore.c is to call the hook.  At most 16 temporary files are open at once.  A module that is killed without closing its files leaves them behind, as for
   any file.  __modlib_tmp_prefix is what the tests change (the host and the interpreter have no scrap directory); __modlib_tmpname gives the name of an open temporary file (for the tests). */
#pragma GCC optimize ("Os")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <kernel.h>
#include "fileimpl.h"

extern void (*__modlib_tmp_hook) (FILE *);                       /* fcore.c */
const char *__modlib_tmp_prefix = "<Wimp$ScrapDir>.tmp";

#define NTMP 16
static struct { FILE *f; char name[L_tmpnam]; } tmps[NTMP];
static unsigned counter;

/* a name for which there is no object (0), or -1 with errno set */
static int generate (char *out)
{
  size_t n = strlen (__modlib_tmp_prefix);
  int tries;
  if (n + 8 > L_tmpnam) { errno = ENAMETOOLONG; return -1; }
  for (tries = 0; tries < 2000; tries++)
    {
      unsigned v;
      int i, t;
      _kernel_osfile_block b = { 0, 0, 0, 0 };
      if (!counter) counter = (unsigned) clock () * 40503u;       /* somewhere that is not the same each time */
      v = counter++ & 0x0FFFFFFF;
      memcpy (out, __modlib_tmp_prefix, n);
      for (i = 0; i < 7; i++) out[n + i] = "0123456789abcdef"[(v >> (24 - 4 * i)) & 15];
      out[n + 7] = 0;
      t = _kernel_osfile (17, out, &b);                            /* the type of the object: 0 = nothing of that name */
      if (t == 0) return 0;
      if (t < 0) { errno = ENOENT; return -1; }                    /* the scrap directory is not there: an error, not a name that is taken */
    }
  errno = EEXIST;
  return -1;
}

char *tmpnam (char *s)
{
  static char buf[L_tmpnam];
  char *d = s ? s : buf;
  if (generate (d)) return 0;
  return d;
}

static void tmp_closed (FILE *f)
{
  int i;
  for (i = 0; i < NTMP; i++)
    if (tmps[i].f == f)
      {
        int e = errno;
        tmps[i].f = 0;
        remove (tmps[i].name);
        errno = e;                                                 /* (the close has already told its result) */
      }
}

FILE *tmpfile (void)
{
  char name[L_tmpnam];
  FILE *f;
  int i;
  for (i = 0; i < NTMP && tmps[i].f; i++) ;
  if (i == NTMP) { errno = EMFILE; return 0; }
  if (generate (name)) return 0;
  f = fopen (name, "w+b");
  if (!f) return 0;
  f->bits |= B_TEMP;
  tmps[i].f = f;
  strcpy (tmps[i].name, name);
  __modlib_tmp_hook = tmp_closed;
  return f;
}

const char *__modlib_tmpname (FILE *f)
{
  int i;
  for (i = 0; i < NTMP; i++)
    if (tmps[i].f == f && f) return tmps[i].name;
  return 0;
}
