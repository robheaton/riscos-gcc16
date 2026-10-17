/* dirent.c - opendir, readdir, rewinddir, closedir over OS_GBPB 12 (UnixLib 64; after libunixlib/unix/dirent.c of UnixLib, revised BSD licence, licenses/UnixLib-COPYING.txt).
   A name from the directory is turned into a Unix leaf name with __filename_char_map; with __RISCOSIFY_FILETYPE_EXT the file type is added as ",xyz" (unless it is Text).
   Not done yet: the merging of the suffix directories (a directory "c" holding "main" is "main.c" of the parent in UnixLib), "." and "..", seekdir and telldir.  */
#include "ul64.h"
#include <dirent.h>

#define OS_GBPB 0x0C
#define BUFSIZE 2048

struct ul_dir
{
  char *path;				/* the RISC OS name of the directory */
  long offset;				/* the next entry to ask OS_GBPB for (-1: none left) */
  char *buf;				/* the entries that OS_GBPB gave */
  int count, index;			/* how many, and how many were returned */
  char *p;				/* the next one */
  ino_t ino;
};

static struct ul_dir *state (DIR *d) { struct ul_dir *u; memcpy (&u, d->buf, sizeof u); return u; }

DIR *opendir (const char *name)
{
  char b[MAXPATHLEN];
  const char *path = name;
  long r[10];
  DIR *d;
  struct ul_dir *u;
  int ctl = __get_riscosify_control ();
  if (!name || !*name) { errno = ENOENT; return NULL; }
  __set_riscosify_control (ctl | __RISCOSIFY_NO_SUFFIX);
  path = __riscosify_std (name, 0, b, sizeof b, NULL) ? b : NULL;
  __set_riscosify_control (ctl);
  if (!path) { errno = ENAMETOOLONG; return NULL; }
  r[0] = 17; r[1] = (long) path;
  if (__os_swi (OS_File, r)) { errno = ENOENT; return NULL; }
  if (r[0] == 0) { errno = ENOENT; return NULL; }
  if (r[0] == 1) { errno = ENOTDIR; return NULL; }
  d = (DIR *) calloc (1, sizeof (DIR));
  u = (struct ul_dir *) calloc (1, sizeof *u);
  if (d && u) { u->path = strdup (path); u->buf = (char *) malloc (BUFSIZE); }
  if (!d || !u || !u->path || !u->buf) { if (u) { free (u->path); free (u->buf); } free (u); free (d); errno = ENOMEM; return NULL; }
  d->fd = -1;
  memcpy (d->buf, &u, sizeof u);
  return d;
}

struct dirent *readdir (DIR *d)
{
  struct ul_dir *u;
  if (!d) { errno = EBADF; return NULL; }
  u = state (d);
  while (u->index >= u->count)
    {
      long r[10];
      if (u->offset == -1) return NULL;
      r[0] = 12; r[1] = (long) u->path; r[2] = (long) u->buf; r[3] = BUFSIZE / 64 > 0 ? 32 : 1; r[4] = u->offset; r[5] = BUFSIZE; r[6] = (long) "*";
      if (__os_swi (OS_GBPB, r)) { errno = EIO; return NULL; }
      u->count = (int) r[3]; u->index = 0; u->p = u->buf; u->offset = (int) r[4];
      if (u->count == 0 && u->offset == -1) return NULL;
    }
  {
    const unsigned *e = (const unsigned *) u->p;			/* load, exec, length, attributes, object type, file type, then the name (as Pyromaniac and RISC OS 5 give it) */
    unsigned objtype = e[4], filetype = e[5];
    const unsigned char *name = (const unsigned char *) (e + 6);
    char *o = d->dirent.d_name, *end = d->dirent.d_name + sizeof d->dirent.d_name - 1;
    int ctl = __get_riscosify_control ();
    size_t len = strlen ((const char *) name) + 1;
    while (*name && o < end) *o++ = __filename_char_map[*name++];
    /* the file type as ",xyz", for a file whose type is not Text */
    if ((ctl & __RISCOSIFY_FILETYPE_EXT) && objtype == 1 && filetype <= 0xFFF)
      {
	unsigned ft = filetype;
	if ((ft != 0xFFF || (ctl & __RISCOSIFY_FILETYPE_FFF_EXT)) && o + 4 <= end) o += sprintf (o, ",%03x", ft);
      }
    *o = 0;
    u->p += (24 + len + 3) & ~3UL;
    u->index++;
    d->dirent.d_ino = ++u->ino;
    d->dirent.d_type = objtype == 2 ? DT_DIR : DT_REG;
    return &d->dirent;
  }
}

void rewinddir (DIR *d)
{
  struct ul_dir *u;
  if (!d) return;
  u = state (d);
  u->offset = 0; u->count = u->index = 0; u->ino = 0;
}

int closedir (DIR *d)
{
  struct ul_dir *u;
  if (!d) { errno = EBADF; return -1; }
  u = state (d);
  free (u->path); free (u->buf); free (u); free (d);
  return 0;
}
