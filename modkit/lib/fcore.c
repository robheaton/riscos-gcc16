/* fcore.c - the files of <stdio.h>: fopen, freopen, fclose, fflush, setvbuf, setbuf, fseek, ftell, rewind, fgetpos, fsetpos, feof, ferror, clearerr, remove, rename, and the machinery that fread.c and fwrite.c use
   (fileimpl.h says how a stream keeps its buffer and the file pointer).  The standard streams are here too: stdin reads a line at a time with OS_ReadLine, stdout and stderr write to the screen.  Files are named
   the RISC OS way (FileSwitch translates the name: <Var> is expanded, a space ends it, Prefix: uses Prefix$Path); "b" and "t" in a mode do nothing; "x" with "w" fails when the file is there; a file is at most 2 GB
   (the C types are 32 bit: a longer file is not opened, EOVERFLOW).  Every OS error is mapped to errno (set_errno) and stays available as _kernel_last_oserror ().  The files are closed at the end of a program (exit,
   the hook below) and by __modlib_closeall; a module that opens files closes them in its finalisation. */
#pragma GCC optimize ("Os")                       /* not a hot path: the smaller code is the better one in a module */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include "fileimpl.h"
#include "hostptr.h"

#define FIND_IN   0x4F                          /* OS_Find: open for input / output / update; the name as it is (3), an error for a directory (4) and, for input and update, for a file that is not there (8) */
#define FIND_OUT  0x8F
#define FIND_UP   0xCF
#define CARRY     0x20000000u
#define X         0x20000u
#define MAXPOS    0x7FFFFFFFL                   /* the largest file and position */

extern _kernel_oserror *__modlib_xswif (unsigned swi_x, unsigned *regs, unsigned *flags);
extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);
extern _kernel_oserror *__modlib_remember (const _kernel_oserror *e);
extern const _kernel_oserror *__modlib_peek_oserror (void);
extern void (*__modlib_stdio_end_hook) (int);   /* exit.c */
extern int (*__modlib_stdout_hook) (int);       /* scr.c */

static unsigned char kbbuf[260];
FILE __modlib_stdin = { 0, K_KEYBOARD, M_READ, S_IDLE, B_STATIC, -1, kbbuf, sizeof kbbuf, 0, 0, 0, 0, 0 };
FILE __modlib_stdout = { 0, K_SCREEN, M_WRITE, S_IDLE, B_STATIC, -1, 0, 0, 0, 0, 0, 0, 0 };
FILE __modlib_stderr = { 0, K_SCREEN, M_WRITE, S_IDLE, B_STATIC, -1, 0, 0, 0, 0, 0, 0, 0 };
static FILE *files;                             /* the open files */
void (*__modlib_tmp_hook) (FILE *f);              /* tmpfile.c: called when a stream made by tmpfile has been closed (its file is to go) */

/* errno from an OS error (the numbers of FileSwitch, FileCore and the other file systems).  The error itself stays for _kernel_last_oserror. */
static void set_errno (const _kernel_oserror *e)
{
  switch (e ? e->errnum & 0xFF : 0)
    {
    case 0xD6: errno = ENOENT; break;                           /* File not found */
    case 0xA8: errno = EISDIR; break;                           /* Object is a directory */
    case 0xBD: case 0xC3: errno = EACCES; break;                /* Access violation, Locked */
    case 0xC1: case 0xDE: errno = EBADF; break;                 /* Not open for update, Channel */
    case 0xC2: errno = EBUSY; break;                            /* File open */
    case 0xC4: case 0xB0: errno = EEXIST; break;                /* Already exists, Bad rename (the destination is there) */
    case 0xB4: errno = ENOTEMPTY; break;                        /* Dir not empty */
    case 0xC0: errno = EMFILE; break;                           /* Too many open files */
    case 0xC9: errno = EROFS; break;                            /* Disc protected */
    case 0xC6: case 0xB3: case 0x99: errno = ENOSPC; break;     /* Disc full, Dir full, Map full */
    case 0xB7: case 0xCC: case 0xFD: errno = EINVAL; break;     /* Outside file, Bad file name, Wild cards / Bad string */
    default: errno = EIO; break;
    }
}
static void os_errno (void) { set_errno (__modlib_peek_oserror ()); }
static int osfail (FILE *f)                                     /* a kernel call of the stream gave an error */
{
  os_errno ();
  f->bits |= B_ERR;
  return EOF;
}

/* the length of the file.  FileSwitch has it as an unsigned number; a file of 2 GB or more is more than the C types (a long) have: EOVERFLOW */
static int get_len (unsigned handle, long *ext)
{
  int r = _kernel_osargs (2, handle, 0);
  if (r == _kernel_ERROR) { os_errno (); return -1; }
  if (r < 0) { errno = EOVERFLOW; return -1; }
  *ext = r;
  return 0;
}
static int file_len (FILE *f, long *ext)
{
  if (get_len (f->handle, ext)) { f->bits |= B_ERR; return -1; }
  return 0;
}

/* the OS file pointer to FPOS; a file is made longer when FPOS is beyond its end (a pointer beyond the end is an error for a file that is not writable, and the zeros are the library's own).  Returns 0, or -1 with errno. */
static int set_ptr (FILE *f)
{
  long ext;
  if (get_len (f->handle, &ext)) return -1;
  if (f->fpos > ext && _kernel_osargs (3, f->handle, (int) f->fpos) == _kernel_ERROR) { os_errno (); return -1; }
  if (_kernel_osargs (1, f->handle, (int) f->fpos) == _kernel_ERROR) { os_errno (); return -1; }
  f->bits |= B_PTR;
  return 0;
}

int __modlib_rawwrite (FILE *f, const void *p, unsigned n)
{
  _kernel_osgbpb_block b;
  if (f->mode & M_APPEND)                                       /* every write is at the end */
    {
      long ext;
      if (file_len (f, &ext)) return EOF;
      f->fpos = ext;
      f->bits &= ~B_PTR;
    }
  if ((unsigned long) f->fpos + n > (unsigned long) MAXPOS) { errno = EFBIG; f->bits |= B_ERR; return EOF; }
  if (!(f->bits & B_PTR) && set_ptr (f)) { f->bits |= B_ERR; return EOF; }
  b.dataptr = (void *) p; b.nbytes = (int) n; b.fileptr = 0; b.buf_len = 0; b.wild_fld = 0;
  if (_kernel_osgbpb (2, f->handle, &b) == _kernel_ERROR) return osfail (f);
  f->fpos += (long) n - b.nbytes;
  if (b.nbytes)                                                 /* (FileSwitch gives an error for a full disc: this is for a file system that does not) */
    {
      errno = ENOSPC;
      f->bits |= B_ERR;
      return EOF;
    }
  return 0;
}

int __modlib_rawread (FILE *f, void *p, unsigned n)
{
  _kernel_osgbpb_block b;
  if (!(f->bits & B_PTR))
    {
      long ext;
      if (file_len (f, &ext)) return -1;
      if (f->fpos >= ext) return 0;                              /* at or beyond the end: the pointer cannot go there */
      if (_kernel_osargs (1, f->handle, (int) f->fpos) == _kernel_ERROR) { osfail (f); return -1; }
      f->bits |= B_PTR;
    }
  b.dataptr = p; b.nbytes = (int) n; b.fileptr = 0; b.buf_len = 0; b.wild_fld = 0;
  if (_kernel_osgbpb (4, f->handle, &b) == _kernel_ERROR) { osfail (f); return -1; }
  return (int) n - b.nbytes;
}

/* the end of a program or a module: __modlib_stdio_end_hook (exit.c) points to stdio_end once a stream has been used */
static void stdio_end (int flush);
void __modlib_register_end (void) { __modlib_stdio_end_hook = stdio_end; }

/* a line from the keyboard: OS_ReadLine (it echoes and edits; at most 258 characters, and no control characters: R2 = 32), and a line feed at the end; Escape is the end of the input */
static int kbfill (FILE *f)
{
  unsigned r[10] = { 0 }, fl = 0;
  _kernel_oserror *e;
  f->rpos = f->rend = 0;
  f->state = S_READ;
  r[0] = PIN (f->buf); r[1] = f->bufsize - 2; r[2] = 32; r[3] = 255;
  e = __modlib_xswif (0x0E | X, r, &fl);
  if (e)
    {
      __modlib_remember (e);
      errno = EIO;
      f->bits |= B_ERR;
      return -1;
    }
  if (fl & CARRY)
    {
      _kernel_osbyte (126, 0, 0);                                /* acknowledge the Escape */
      return 0;
    }
  f->buf[r[1]] = '\n';
  f->rend = r[1] + 1;
  return 1;
}

int __modlib_fill (FILE *f)
{
  int got;
  if (f->kind == K_KEYBOARD) return kbfill (f);
  if (f->state == S_READ) f->fpos += f->rend;                    /* the bytes of the buffer are used up; the OS pointer is already there */
  f->state = S_READ;
  f->rpos = f->rend = 0;
  got = __modlib_rawread (f, f->buf, (f->bits & B_UNBUF) ? 1 : f->bufsize);
  if (got < 0) return -1;
  f->rend = (unsigned) got;
  return got > 0;
}

int __modlib_drain (FILE *f)
{
  unsigned n = f->wlen;
  if (!n) return 0;
  f->wlen = 0;
  return __modlib_rawwrite (f, f->buf, n);
}
int __modlib_flush (FILE *f)
{
  int r = 0;
  if (f->state == S_WRITE)
    {
      r = __modlib_drain (f);
      f->state = S_IDLE;
    }
  return r;
}

/* the position that the program sees (a character that was pushed back is before the buffer: one less) */
static long logical (const FILE *f)
{
  long p = f->fpos;
  if (f->state == S_READ) p += (long) f->rpos;
  else if (f->state == S_WRITE) p += (long) f->wlen;
  return p - (f->unget >= 0);
}
/* drop what is read ahead: the stream is idle at the position that the program sees */
static void drop_input (FILE *f)
{
  f->fpos = logical (f);
  f->rpos = f->rend = 0;
  f->unget = -1;
  f->state = S_IDLE;
  f->bits &= ~B_PTR;
}

int __modlib_prepread (FILE *f)
{
  __modlib_register_end ();
  if (f->kind == K_CLOSED || !(f->mode & M_READ))
    {
      errno = EBADF;
      f->bits |= B_ERR;
      return EOF;
    }
  if (f->state == S_WRITE && __modlib_flush (f)) return EOF;
  return 0;
}
int __modlib_prepwrite (FILE *f)
{
  if (f->kind == K_CLOSED || !(f->mode & M_WRITE))
    {
      errno = EBADF;
      f->bits |= B_ERR;
      return EOF;
    }
  if (f->kind == K_FILE && (f->state == S_READ || f->unget >= 0)) drop_input (f);
  if (f->kind == K_FILE) f->state = S_WRITE;
  return 0;
}

/* open NAME with MODE into F, or into a new stream when F is 0; returns the stream or 0 */
static FILE *open_file (FILE *f, const char *name, const char *mode)
{
  int c = mode[0], plus = 0, excl = 0, h;
  long ext = 0;
  const char *p;
  unsigned char kind_mode;
  if (c != 'r' && c != 'w' && c != 'a') { errno = EINVAL; return 0; }
  for (p = mode + 1; *p; p++) { if (*p == '+') plus = 1; else if (*p == 'x') excl = 1; }
  if (excl && c != 'w') { errno = EINVAL; return 0; }
  if (excl || c != 'r' || plus)                                   /* "wx": the file must not be there; a file that is read only or locked is not opened for writing at all (OS_Find for output would give a stream that cannot be written, and may empty the file) */
    {
      _kernel_osfile_block b = { 0, 0, 0, 0 };
      int t = _kernel_osfile (17, name, &b);                      /* OS_File 17: the type of the object and its attributes (bit 1 write, bit 3 locked) */
      if (t == _kernel_ERROR) { os_errno (); return 0; }
      if (t != 0 && excl) { errno = EEXIST; return 0; }
      if (t == 1 && ((b.end & 2) == 0 || (b.end & 8))) { errno = EACCES; return 0; }
    }
  if (c == 'r') h = _kernel_osfind (plus ? FIND_UP : FIND_IN, name);
  else if (c == 'w') h = _kernel_osfind (FIND_OUT, name);
  else
    {
      h = _kernel_osfind (FIND_UP, name);                         /* append: an existing file ... */
      if (h == _kernel_ERROR)
        {
          const _kernel_oserror *e = __modlib_peek_oserror ();
          if (e && (e->errnum & 0xFF) == 0xD6) h = _kernel_osfind (FIND_OUT, name);          /* ... or a new one */
        }
    }
  if (h == _kernel_ERROR) { os_errno (); return 0; }
  if (h <= 0) { errno = ENOENT; return 0; }
  kind_mode = (unsigned char) (c == 'r' ? M_READ : c == 'w' ? M_WRITE : M_WRITE | M_APPEND);
  if (plus) kind_mode |= M_READ | M_WRITE;
  if (get_len ((unsigned) h, &ext)) { _kernel_osfind (0, (const char *) h); return 0; }
  if (kind_mode & M_WRITE)                                        /* FileSwitch opens a file that is read only or locked for update without a word, as a stream that cannot be written (OS_Args 254: bit 7 is the right to write) */
    {
      unsigned r[10] = { 254, (unsigned) h };
      if (!__modlib_xswi (0x09 | X, r) && !(r[0] & 0x80)) { errno = EACCES; _kernel_osfind (0, (const char *) h); return 0; }
    }
  if (c != 'a' || plus) ext = 0;                                  /* "a" starts at the end; "a+" reads from the start (and every write goes to the end) */
  if (!f)
    {
      f = malloc (sizeof (FILE) + BUFSIZ);
      if (!f) { errno = ENOMEM; _kernel_osfind (0, (const char *) h); return 0; }
      f->bits = 0;
      f->buf = (unsigned char *) (f + 1);                         /* the buffer follows the FILE in the same block */
      f->bufsize = BUFSIZ;
    }
  else if (!f->buf)                                               /* a standard stream made a file by freopen */
    {
      f->buf = malloc (BUFSIZ);
      if (!f->buf) { errno = ENOMEM; _kernel_osfind (0, (const char *) h); return 0; }
      f->bufsize = BUFSIZ;
      f->bits |= B_OWNBUF;
    }
  f->handle = (unsigned) h;
  f->kind = K_FILE;
  f->mode = kind_mode;
  f->state = S_IDLE;
  f->bits &= B_STATIC | B_OWNBUF;
  f->bits |= B_PTR;
  f->unget = -1;
  f->rpos = f->rend = f->wlen = 0;
  f->fpos = ext;
  if (ext) f->bits &= ~B_PTR;                                     /* the OS pointer is at 0 */
  f->next = files;
  files = f;
  __modlib_register_end ();
  return f;
}

FILE *fopen (const char *name, const char *mode) { return open_file (0, name, mode); }

static int stdout_put (int c) { return fputc (c, &__modlib_stdout); }
static void sync_stdout (void) { __modlib_stdout_hook = __modlib_stdout.kind == K_SCREEN ? 0 : stdout_put; }       /* printf, puts and putchar follow stdout when freopen or fclose changed it */

static void unlink_file (FILE *f)
{
  FILE **pp;
  for (pp = &files; *pp; pp = &(*pp)->next)
    if (*pp == f) { *pp = f->next; return; }
}
static int close_file (FILE *f)
{
  int r = 0;
  if (f->kind == K_FILE)
    {
      if (__modlib_flush (f)) r = EOF;
      if (_kernel_osfind (0, (const char *) f->handle) == _kernel_ERROR) { os_errno (); r = EOF; }
      unlink_file (f);
      if ((f->bits & B_TEMP) && __modlib_tmp_hook) __modlib_tmp_hook (f);
      if (f->bits & B_OWNBUF) { free (f->buf); f->buf = 0; f->bufsize = 0; }
    }
  f->kind = K_CLOSED;                                             /* nothing of the old state is left: reading from it is a bad file, not stale bytes */
  f->state = S_IDLE;
  f->rpos = f->rend = f->wlen = 0;
  f->unget = -1;
  f->bits &= B_STATIC;
  return r;
}
int fclose (FILE *f)
{
  int r;
  if (!f || f->kind == K_CLOSED) { errno = EBADF; return EOF; }    /* (a second fclose of a stream that is not static is a use of freed memory, as in C) */
  r = close_file (f);
  if (f->bits & B_STATIC) { if (f == &__modlib_stdout) sync_stdout (); }
  else free (f);
  return r;
}
/* freopen with a name: the stream is closed and opened again as the same FILE (also stdin, stdout and stderr: printf follows stdout).  freopen (NULL, ...) is not supported. */
FILE *freopen (const char *name, const char *mode, FILE *f)
{
  FILE *g;
  if (!name || !f) { errno = EINVAL; return 0; }
  close_file (f);
  g = open_file (f, name, mode);
  if (!g && !(f->bits & B_STATIC)) free (f);
  if (f == &__modlib_stdout) sync_stdout ();
  return g;
}

int fflush (FILE *f)
{
  int r = 0;
  if (!f)
    {
      FILE *p;
      for (p = files; p; p = p->next) if (fflush (p)) r = EOF;
      return r;
    }
  if (f->kind != K_FILE) return 0;
  if (f->state == S_WRITE) r = __modlib_flush (f);
  else if (f->state == S_READ || f->unget >= 0) drop_input (f);
  if (_kernel_osargs (0xFF, f->handle, 0) == _kernel_ERROR)       /* OS_Args 255: ensure that the file is on the disc: this is where FileSwitch writes, and where a disc error shows */
    {
      os_errno ();
      f->bits |= B_ERR;
      r = EOF;
    }
  return r;
}

/* the standard streams as they are at the start: the end of a program (or of a module's use of them) sets them up again */
static void reset_stream (FILE *f, int kind, int mode, unsigned char *buf, unsigned size)
{
  f->handle = 0; f->kind = (unsigned char) kind; f->mode = (unsigned char) mode; f->state = S_IDLE; f->bits = B_STATIC;
  f->unget = -1; f->buf = buf; f->bufsize = size; f->rpos = f->rend = f->wlen = 0; f->fpos = 0; f->next = 0;
}
static void stdio_end (int flush)
{
  while (files)                                                   /* every open file (a standard stream that freopen made a file too) */
    {
      FILE *f = files;
      if (!flush) { f->wlen = 0; f->state = S_IDLE; }            /* (_Exit, abort: what waits in the buffer is not written) */
      fclose (f);
    }
  reset_stream (&__modlib_stdin, K_KEYBOARD, M_READ, kbbuf, sizeof kbbuf);
  reset_stream (&__modlib_stdout, K_SCREEN, M_WRITE, 0, 0);
  reset_stream (&__modlib_stderr, K_SCREEN, M_WRITE, 0, 0);
  __modlib_stdout_hook = 0;
}
/* for the finalisation of a module that opened files: close them all (a module that is killed does not close its files by itself); the number of files that were open */
int __modlib_closeall (void)
{
  int n = 0;
  while (files) { FILE *f = files; fclose (f); n++; }
  return n;
}

int setvbuf (FILE *f, char *buf, int mode, size_t size)
{
  (void) buf;                                                     /* the stream keeps its own buffer */
  if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) return -1;
  if (f->kind != K_FILE) return 0;
  if (f->state == S_WRITE) __modlib_flush (f);
  else if (f->state == S_READ || f->unget >= 0) drop_input (f);
  f->bits &= ~(B_UNBUF | B_LINE);
  if (mode == _IONBF) f->bits |= B_UNBUF;
  else if (mode == _IOLBF) f->bits |= B_LINE;
  if (mode != _IONBF && size >= 16 && size <= 0x100000 && size != f->bufsize)
    {
      unsigned char *nb = malloc (size);
      if (nb)
        {
          if (f->bits & B_OWNBUF) free (f->buf);
          f->buf = nb;
          f->bufsize = (unsigned) size;
          f->bits |= B_OWNBUF;
        }
    }
  return 0;
}
void setbuf (FILE *f, char *buf) { setvbuf (f, buf, buf ? _IOFBF : _IONBF, BUFSIZ); }

int fseek (FILE *f, long off, int whence)
{
  long base, target;
  if (f->kind != K_FILE) { errno = f->kind == K_CLOSED ? EBADF : ESPIPE; return -1; }
  if (f->state == S_WRITE && __modlib_flush (f)) return -1;
  if (whence == SEEK_SET) base = 0;
  else if (whence == SEEK_CUR) base = logical (f);
  else if (whence == SEEK_END)
    {
      if (file_len (f, &base)) return -1;
    }
  else { errno = EINVAL; return -1; }
  if (off > 0 && base > MAXPOS - off) { errno = EOVERFLOW; return -1; }                 /* (no sum that overflows: the check comes first) */
  target = base + off;
  if (target < 0) { errno = EINVAL; return -1; }
  if (f->state == S_READ && target >= f->fpos && target <= f->fpos + (long) f->rend)
    {
      f->rpos = (unsigned) (target - f->fpos);                    /* inside the buffer: no OS call */
      f->unget = -1;
    }
  else
    {
      f->state = S_IDLE;
      f->rpos = f->rend = 0;
      f->unget = -1;
      f->fpos = (long) target;
      f->bits &= ~B_PTR;
    }
  f->bits &= ~B_EOF;
  return 0;
}
long ftell (FILE *f)
{
  if (f->kind != K_FILE) { errno = f->kind == K_CLOSED ? EBADF : ESPIPE; return -1; }
  if (f->state == S_WRITE && (f->mode & M_APPEND))                /* what is written next goes to the end of the file: the end plus what waits in the buffer */
    {
      long ext;
      if (get_len (f->handle, &ext) == 0) return ext + (long) f->wlen;
    }
  return logical (f);
}
void rewind (FILE *f)
{
  fseek (f, 0, SEEK_SET);
  f->bits &= ~B_ERR;
}
int fgetpos (FILE *f, fpos_t *pos)
{
  long p = ftell (f);
  if (p < 0) return -1;
  *pos = p;
  return 0;
}
int fsetpos (FILE *f, const fpos_t *pos) { return fseek (f, *pos, SEEK_SET); }

int feof (FILE *f) { return (f->bits & B_EOF) != 0; }
int ferror (FILE *f) { return (f->bits & B_ERR) != 0; }
void clearerr (FILE *f) { f->bits &= ~(B_EOF | B_ERR); }

int remove (const char *name)
{
  _kernel_osfile_block b = { 0, 0, 0, 0 };
  int r = _kernel_osfile (6, name, &b);                          /* OS_File 6: delete; the object type that was there */
  if (r == _kernel_ERROR) { os_errno (); return -1; }
  if (r == 0) { errno = ENOENT; return -1; }
  return 0;
}
int rename (const char *from, const char *to)
{
  unsigned r[10] = { 25, PIN (from), PIN (to) };       /* OS_FSControl 25: it does not replace a file that is there */
  _kernel_oserror *e = __modlib_xswi (0x29 | X, r);
  if (e)
    {
      set_errno (__modlib_remember (e));
      if (errno == EEXIST && (e->errnum & 0xFF) == 0xB0)         /* Bad rename is also the error for another file system (or special field): EXDEV when the destination is not there */
        {
          _kernel_osfile_block b = { 0, 0, 0, 0 };
          if (_kernel_osfile (17, to, &b) <= 0) errno = EXDEV;
        }
      return -1;
    }
  return 0;
}
