/* fileimpl.h - the FILE of the stdio functions (fcore.c, fread.c, fwrite.c, fprintf.c, fscanf.c).  Not installed as a header of the library: the sources that include it are in the same folder.

   A stream is idle, reading or writing.  BUF holds, when reading, the bytes BUF[RPOS .. REND) that are read from the file but not yet used, and the file offset of BUF[0] is FPOS; when writing, the WLEN bytes that
   are to be written at FPOS.  B_PTR says that the file pointer of the OS (OS_Args 0) is where the next transfer of the buffer starts (FPOS + REND when reading, FPOS otherwise), so that no OS_Args call is
   needed; a seek only changes FPOS and clears it.  The OS is called as little as it can be: OS_GBPB 4 / 2 for the buffer (at the file pointer), OS_Args 2 for the length where it matters (the end of the file,
   an append, a position beyond the end), OS_Args 1 / 3 only to move the pointer or to make the file longer. */
#ifndef _FILEIMPL_H
#define _FILEIMPL_H
#include <stddef.h>
#include <stdio.h>

enum { K_FILE, K_KEYBOARD, K_SCREEN, K_CLOSED };
enum { S_IDLE, S_READ, S_WRITE };
#define M_READ    1
#define M_WRITE   2
#define M_APPEND  4
#define B_EOF     1
#define B_ERR     2
#define B_PTR     4                             /* the file pointer of the OS is where the buffer's next transfer is */
#define B_OWNBUF  8                             /* BUF was got with malloc */
#define B_UNBUF   16                            /* _IONBF */
#define B_LINE    32                            /* _IOLBF */
#define B_STATIC  64                            /* the FILE itself is not from malloc (stdin, stdout, stderr) */
#define B_TEMP    128                           /* made by tmpfile: tmpfile.c removes the file when the stream is closed (__modlib_tmp_hook) */

struct __FILE
{
  unsigned handle;                              /* the OS_Find handle */
  unsigned char kind, mode, state, bits;
  int unget;                                    /* a character pushed back that does not fit in the buffer, or -1 */
  unsigned char *buf;
  unsigned bufsize;
  unsigned rpos, rend, wlen;
  long fpos;
  struct __FILE *next;                          /* the open files, for fflush (0) and the exit */
};

extern int __modlib_prepread (FILE *f);         /* the stream is ready for reading (a write buffer is written): 0, or EOF with errno and the error flag */
extern int __modlib_prepwrite (FILE *f);        /* ... for writing (a read buffer is dropped, the position is kept) */
extern int __modlib_fill (FILE *f);             /* fill the read buffer: 1 = there are bytes, 0 = the end of the file, -1 = an error (errno, the error flag) */
extern int __modlib_drain (FILE *f);            /* write the write buffer, stay in write state: 0 or EOF */
extern int __modlib_flush (FILE *f);            /* write the write buffer and go idle: 0 or EOF */
extern int __modlib_rawwrite (FILE *f, const void *p, unsigned n);        /* write N bytes at the position of the stream, with no buffer: 0 or EOF */
extern int __modlib_rawread (FILE *f, void *p, unsigned n);               /* read up to N bytes at the position, with no buffer: the number, or -1 */
extern void __modlib_register_end (void);       /* (fcore.c) the end of a program is to close the files and set the standard streams up again */
extern void __modlib_scrputc (int c);
extern int __modlib_vformat (void (*out) (int, void *), void *ctx, const char *fmt, va_list ap);
extern int __modlib_vscan (int (*get) (void *ctx), void *ctx, void (*putback) (int c, void *ctx), const char *fmt, va_list ap);
#endif
