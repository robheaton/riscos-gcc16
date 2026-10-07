/* stdio.h - streams for freestanding modules.
   Streams: stdin (the keyboard: a line at a time with OS_ReadLine, at most 258 characters, no control characters, Escape ends the input for good until clearerr; setvbuf does nothing for it), stdout and stderr
   (the screen: unbuffered, OS_WriteC for each character and OS_NewLine, i.e. LF CR, for a line feed) and files (OS_Find, OS_GBPB, OS_Args).  printf, puts and putchar write to stdout; freopen (name, mode, stdout)
   sends them to the file.  sprintf / snprintf / vsnprintf format into memory.  printf: the conversions of C99 without floating point (%n is there); sscanf / fscanf / scanf: integers, characters, strings and
   sets, no floating point (%f %e %g %a stop the scan) and no wide characters (%lc %ls %l[ stop it).
   Files: the name is given to FileSwitch, which translates it: <Var> is expanded, a space ends it, "/" and a double quote are refused (EINVAL), Prefix: uses Prefix$Path.  "b" and "t" in a mode mean the same;
   "x" with "w" fails with EEXIST when the file is there.  A file that fopen makes has the type Data (&FFD).  A file is at most 2 GB (a longer one is not opened: EOVERFLOW).  A file that is open for writing
   cannot be opened again (EBUSY), nor can it be renamed or removed while it is open.  Opening a file that is read only or locked for writing fails (EACCES).  rename () does not replace a file that is there
   (EEXIST), nor does it cross file systems (EXDEV).  What FileSwitch writes late shows at fflush / fclose: they return EOF with errno set (and ferror) for a disc error.  errno is set from the OS error
   (ENOENT, EISDIR, EACCES, EBADF, EBUSY, EEXIST, ENOTEMPTY, EMFILE, EROFS, ENOSPC, EINVAL, EFBIG, EOVERFLOW, EXDEV, otherwise EIO); the OS error itself stays available as _kernel_last_oserror ().
   Streams: one character can be pushed back with ungetc (a second one is refused); a seek discards it.  exit () and the return from main of a runnable module flush and close the files; _Exit and abort close
   them without writing what is in the buffers.  A module that is killed does not: its finalisation calls __modlib_closeall ().  A FILE is freed by fclose (a second fclose of it is as undefined as in C; of a
   standard stream it is EOF); fclose (NULL) is EOF.  The buffers and the FILEs are malloc blocks, i.e. RMA blocks that a runnable program that does not free them leaves behind.
   Not here: gets, tmpfile, tmpnam, freopen (NULL, ...), fseeko / ftello (use fseek and ftell), the wide character functions. */
#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
#include <stdarg.h>
#ifndef EOF
#define EOF (-1)
#endif
#define BUFSIZ 512
#define FILENAME_MAX 256
#define FOPEN_MAX 32
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef struct __FILE FILE;
typedef long fpos_t;
extern FILE __modlib_stdin, __modlib_stdout, __modlib_stderr;
#define stdin (&__modlib_stdin)
#define stdout (&__modlib_stdout)
#define stderr (&__modlib_stderr)

extern int printf (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
extern int sprintf (char *s, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));
extern int snprintf (char *s, size_t n, const char *fmt, ...) __attribute__ ((format (__printf__, 3, 4)));
extern int vsnprintf (char *s, size_t n, const char *fmt, va_list ap);
extern int vsprintf (char *s, const char *fmt, va_list ap);
extern int vprintf (const char *fmt, va_list ap);
extern int fprintf (FILE *f, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));
extern int vfprintf (FILE *f, const char *fmt, va_list ap);
extern int sscanf (const char *s, const char *fmt, ...) __attribute__ ((format (__scanf__, 2, 3)));
extern int vsscanf (const char *s, const char *fmt, va_list ap);
extern int fscanf (FILE *f, const char *fmt, ...) __attribute__ ((format (__scanf__, 2, 3)));
extern int vfscanf (FILE *f, const char *fmt, va_list ap);
extern int scanf (const char *fmt, ...) __attribute__ ((format (__scanf__, 1, 2)));
extern int vscanf (const char *fmt, va_list ap);
extern int puts (const char *s);
extern int putchar (int c);

extern FILE *fopen (const char *name, const char *mode);
extern FILE *freopen (const char *name, const char *mode, FILE *f);
extern int fclose (FILE *f);
extern int fflush (FILE *f);
extern int setvbuf (FILE *f, char *buf, int mode, size_t size);
extern void setbuf (FILE *f, char *buf);
extern size_t fread (void *p, size_t size, size_t n, FILE *f);
extern size_t fwrite (const void *p, size_t size, size_t n, FILE *f);
extern int fgetc (FILE *f);
extern int getc (FILE *f);
extern int getchar (void);
extern char *fgets (char *s, int n, FILE *f);
extern int ungetc (int c, FILE *f);
extern int fputc (int c, FILE *f);
extern int putc (int c, FILE *f);
extern int fputs (const char *s, FILE *f);
extern int fseek (FILE *f, long offset, int whence);
extern long ftell (FILE *f);
extern void rewind (FILE *f);
extern int fgetpos (FILE *f, fpos_t *pos);
extern int fsetpos (FILE *f, const fpos_t *pos);
extern int feof (FILE *f);
extern int ferror (FILE *f);
extern void clearerr (FILE *f);
extern int remove (const char *name);
extern int rename (const char *from, const char *to);
extern void perror (const char *s);
/* for a module that has files open when it ends: close them all (flush the buffers); returns the number closed */
extern int __modlib_closeall (void);
#endif
