/* dirent.c - opendir, readdir, rewinddir, closedir over OS_GBPB 12 (UnixLib 64; after libunixlib/unix/dirent.c of UnixLib, revised BSD licence, licenses/UnixLib-COPYING.txt).
   A name from the directory is turned into a Unix leaf name with __filename_char_map; with __RISCOSIFY_FILETYPE_EXT the file type is added as ",xyz" (unless it is Text).
   The suffix directories are listed as UnixLib does (a directory "c" holding "main" is listed as "main.c" in the parent).  Not done yet: "." and "..".  telldir counts the entries returned; seekdir reads forward (or rewinds first).  */
#include "ul64.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define OS_GBPB 0x0C
#define BUFSIZE 2048

struct ul_dir
{
  char *path;				/* the RISC OS name of the directory */
  char *canon;				/* its canonical name (for d_ino), found at the first readdir */
  long offset;				/* the next entry to ask OS_GBPB for (-1: none left) */
  char *buf;				/* the entries that OS_GBPB gave */
  int count, index;			/* how many, and how many were returned */
  char *p;				/* the next one */
  ino_t ino;
  long pos;				/* how many entries readdir has returned (telldir) */
  struct ul_dir *suffix;		/* the suffix directory that is being listed as name.suffix (see below), or NULL */
  char suffix_name[32];			/* its name: "c" */
  char *upath;				/* the Unix name it was opened with, made full (dirfd opens it) */
  int owns_fd;				/* fdopendir: closedir closes the descriptor */
};

static struct ul_dir *state (DIR *d) { struct ul_dir *u; memcpy (&u, d->buf, sizeof u); return u; }

static struct ul_dir *new_stream (const char *path)
{
  struct ul_dir *u = (struct ul_dir *) calloc (1, sizeof *u);
  if (u) { u->path = strdup (path); u->buf = (char *) malloc (BUFSIZE); }
  if (!u || !u->path || !u->buf) { if (u) { free (u->path); free (u->buf); } free (u); return NULL; }
  return u;
}

static void free_stream (struct ul_dir *u)
{
  if (!u) return;
  free_stream (u->suffix);
  free (u->path); free (u->canon); free (u->buf); free (u->upath); free (u);
}

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
  u = new_stream (path);
  if (!d || !u) { free_stream (u); free (d); errno = ENOMEM; return NULL; }
  d->fd = -1;
  {						/* the Unix name, full, for dirfd */
    char cwd[MAXPATHLEN];
    if (name[0] == '/' || !getcwd (cwd, sizeof cwd)) u->upath = strdup (name);
    else
      {
	const char *n = name;
	while (n[0] == '.' && n[1] == '/') n += 2;
	u->upath = malloc (strlen (cwd) + strlen (n) + 2);
	if (u->upath) { if (!strcmp (n, ".")) strcpy (u->upath, cwd); else sprintf (u->upath, "%s/%s", cwd, n); }
      }
  }
  memcpy (d->buf, &u, sizeof u);
  return d;
}

/* the next entry of a stream: the leaf name as RISC OS has it (valid until the next call), the object type and the file type; 0 at the end (errno is set for an error) */
static int fetch (struct ul_dir *u, const unsigned char **name, unsigned *objtype, unsigned *filetype)
{
  while (u->index >= u->count)
    {
      long r[10];
      if (u->offset == -1) return 0;
      r[0] = 12; r[1] = (long) u->path; r[2] = (long) u->buf; r[3] = 32; r[4] = u->offset; r[5] = BUFSIZE; r[6] = (long) "*";
      if (__os_swi (OS_GBPB, r)) { errno = EIO; return 0; }
      u->count = (int) r[3]; u->index = 0; u->p = u->buf; u->offset = (int) r[4];
      if (u->count == 0 && u->offset == -1) return 0;
    }
  {
    const unsigned *e = (const unsigned *) u->p;			/* load, exec, length, attributes, object type, file type, then the name (as Pyromaniac and RISC OS 5 give it) */
    *objtype = e[4]; *filetype = e[5];
    *name = (const unsigned char *) (e + 6);
    u->p += (24 + strlen ((const char *) *name) + 1 + 3) & ~3UL;
    u->index++;
    return 1;
  }
}

/* A directory with the name of a suffix (c, h, o ...) holds the files that Unix calls name.c: they are listed as such, in the place of the directory (UnixLib: "reverse suffix swapping").  */
extern int __ul64_canon (const char *, char *, unsigned);
extern unsigned long __ul64_hash_ino (const char *);
struct dirent *readdir (DIR *d)
{
  struct ul_dir *u;
  int ctl;
  if (!d) { errno = EBADF; return NULL; }
  u = state (d);
  ctl = __get_riscosify_control ();
  for (;;)
    {
      const unsigned char *name;
      unsigned objtype, filetype;
      struct ul_dir *from = u->suffix ? u->suffix : u;
      char *o = d->dirent.d_name, *end = d->dirent.d_name + sizeof d->dirent.d_name - 1;
      const unsigned char *rname;
      if (!fetch (from, &name, &objtype, &filetype))
	{
	  if (u->suffix) { free_stream (u->suffix); u->suffix = NULL; continue; }	/* the suffix directory is finished: go on in the main one */
	  return NULL;
	}
      if (!u->suffix && objtype == 2 && !(ctl & (__RISCOSIFY_NO_SUFFIX | __RISCOSIFY_NO_REVERSE_SUFFIX)) && __sfixfind ((const char *) name, strlen ((const char *) name)))
	{
	  char sub[MAXPATHLEN];
	  if (strlen (u->path) + strlen ((const char *) name) + 2 < sizeof sub)
	    {
	      sprintf (sub, "%s.%s", u->path, name);
	      u->suffix = new_stream (sub);
	      if (u->suffix) { strncpy (u->suffix_name, (const char *) name, sizeof u->suffix_name - 1); continue; }
	    }
	}
      if (u->suffix && objtype == 2) continue;				/* directories inside a suffix directory are not shown */
      rname = name;
      while (*name && o < end) *o++ = __filename_char_map[*name++];
      if (u->suffix) { size_t n = strlen (u->suffix_name); if (o + 1 + n <= end) { *o++ = '.'; memcpy (o, u->suffix_name, n); o += n; } }
      /* the file type as ",xyz", for a file whose type is not Text */
      if ((ctl & __RISCOSIFY_FILETYPE_EXT) && objtype == 1 && filetype <= 0xFFF)
	{
	  if ((filetype != 0xFFF || (ctl & __RISCOSIFY_FILETYPE_FFF_EXT)) && o + 4 <= end) o += sprintf (o, ",%03x", filetype);
	}
      *o = 0;
      { char full[MAXPATHLEN]; (void) ++u->ino;			/* d_ino = the inode that stat gives for the file (a hash of its canonical RISC OS name), not a counter; the directory is canonicalised once per stream */
	if (!from->canon) { char c[MAXPATHLEN]; if (__ul64_canon (from->path, c, sizeof c) == 0) from->canon = strdup (c); else from->canon = strdup (from->path); }
	if (from->canon && strlen (from->canon) + strlen ((const char *) rname) + 2 < sizeof full) { sprintf (full, "%s.%s", from->canon, rname); d->dirent.d_ino = __ul64_hash_ino (full); }
	else d->dirent.d_ino = u->ino; }
      u->pos++;
      d->dirent.d_type = objtype == 2 ? DT_DIR : DT_REG;
      return &d->dirent;
    }
}

void rewinddir (DIR *d)
{
  struct ul_dir *u;
  if (!d) return;
  u = state (d);
  free_stream (u->suffix); u->suffix = NULL;
  u->offset = 0; u->count = u->index = 0; u->ino = 0; u->pos = 0;
}

long telldir (DIR *d)
{
  if (!d) { errno = EBADF; return -1; }
  return state (d)->pos;
}

void seekdir (DIR *d, long loc)
{
  if (!d || loc < 0) return;
  if (loc < state (d)->pos) rewinddir (d);
  while (state (d)->pos < loc && readdir (d)) ;
}

/* a descriptor for the folder (the *at calls of the SDK take it): opened on the first call */
int dirfd (DIR *d)
{
  struct ul_dir *u;
  if (!d) { errno = EBADF; return -1; }
  if (d->fd >= 0) return d->fd;
  u = state (d);
  if (!u->upath) { errno = ENOTSUP; return -1; }
  d->fd = open (u->upath, O_RDONLY);
  return d->fd;
}

extern const char *__riscos_fd_name (int fd);
DIR *fdopendir (int fd)
{
  const char *name = __riscos_fd_name (fd);
  DIR *d;
  if (!name) { errno = EBADF; return NULL; }
  d = opendir (name);
  if (d) { d->fd = fd; state (d)->owns_fd = 1; }
  return d;
}

int closedir (DIR *d)
{
  if (!d) { errno = EBADF; return -1; }
  if (d->fd >= 0) close (d->fd);
  free_stream (state (d));
  free (d);
  return 0;
}
