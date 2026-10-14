/* fileimpl.h - the FILE of the stdio functions (fcore.c, fread.c, fwrite.c, fprintf.c, fscanf.c).  Not installed as a header of the library: the sources that include it are in the same folder.

   A stream is idle, reading or writing.  BUF holds, when reading, the bytes BUF[RPOS .. REND) that are read from the file but not yet used, and the file offset of BUF[0] is FPOS; when writing, the WLEN bytes that
   are to be written at FPOS.  B_PTR says that the file pointer of the OS (OS_Args 0) is where the next transfer of the buffer starts (FPOS + REND when reading, FPOS otherwise), so that no OS_Args call is
   needed; a seek only changes FPOS and clears it.  The OS is called as little as it can be: OS_GBPB 4 / 2 for the buffer (at the file pointer), OS_Args 2 for the length where it matters (the end of the file,
   an append, a position beyond the end), OS_Args 1 / 3 only to move the pointer or to make the file longer. */
#ifndef _FILEIMPL_H
#define _FILEIMPL_H
#include <stddef.h>
#include <stdio.h>

enum { K_FILE, K_KEYBOARD, K_SCREEN, K_CLOSED, K_SOCKET };      /* K_SOCKET: a descriptor (fdio.c) that holds the number of a socket of the Internet module in HANDLE; it is not a stream: it is never in the list of open files and has no buffer */
enum { S_IDLE, S_READ, S_WRITE };
#define M_READ    1
#define M_WRITE   2
#define M_APPEND  4
#define M_NONBLOCK 8                            /* a socket descriptor: non-blocking (FIONBIO / O_NONBLOCK) */
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
  int fd;                                       /* the descriptor of fdio.c (fileno, open ...), or -1 when it has none yet (a standard stream has 0, 1 or 2 by its address) */
  char *name;                                   /* a copy of the name that open () used (a malloc block), or 0 */
};

extern int __modlib_prepread (FILE *f);         /* the stream is ready for reading (a write buffer is written): 0, or EOF with errno and the error flag */
extern int __modlib_prepwrite (FILE *f);        /* ... for writing (a read buffer is dropped, the position is kept) */
extern int __modlib_fill (FILE *f);             /* fill the read buffer: 1 = there are bytes, 0 = the end of the file, -1 = an error (errno, the error flag) */
extern int __modlib_drain (FILE *f);            /* write the write buffer, stay in write state: 0 or EOF */
extern int __modlib_flush (FILE *f);            /* write the write buffer and go idle: 0 or EOF */
extern int __modlib_rawwrite (FILE *f, const void *p, unsigned n);        /* write N bytes at the position of the stream, with no buffer: 0 or EOF */
extern int __modlib_rawread (FILE *f, void *p, unsigned n);               /* read up to N bytes at the position, with no buffer: the number, or -1 */
extern FILE *__modlib_open_gen (FILE *f, const char *name, int rd, int wr, int create, int trunc, int excl, int append, int end);   /* fcore.c: open NAME into F (a new stream if F is 0) for reading and / or writing; a missing file is made if CREATE, an existing one emptied if TRUNC, EXCL: it must not be there; APPEND: every write goes to the end; END: the position starts at the end */
/* sockets as descriptors (fdio.c, sockets.c): socket () and accept () give a descriptor of the same table as open () (K_SOCKET), so that close, read, write, readv, writev, ioctl, fcntl, fstat, dup and select work on it as they
   do in UnixLib; the number of the Internet module's own socket is kept inside.  fdio.c does not call sockets.c directly (a module that has no sockets would take their code with every use of close ()): sockets.c sets
   __modlib_sock_op when the first socket is made. */
enum { SOP_CLOSE, SOP_READ, SOP_WRITE, SOP_READV, SOP_WRITEV, SOP_IOCTL };
extern int (*__modlib_sock_op) (int op, int handle, void *buf, unsigned long n, void *arg);       /* READ / WRITE: BUF, N bytes; READV / WRITEV: BUF is the iovec array, N the count; IOCTL: N the request, ARG the argument */
extern int __modlib_socket_new (int handle);     /* the descriptor for HANDLE (fdio.c); sockets.c's __modlib_socket_fd is this and the setting of __modlib_sock_op */
extern int __modlib_socket_fd (int handle);     /* a descriptor for the Internet module's socket HANDLE: the number, or -1 with errno (EMFILE, ENOMEM); the socket is not closed then */
extern int __modlib_socket_handle (int fd);     /* the Internet module's number of the socket of descriptor FD, or -1 with errno EBADF (not open) or ENOTSOCK */
extern int __modlib_fd_info (int fd, int *handle);   /* 0: FD is not open (errno EBADF); 1: a socket (*HANDLE is its number); 2: anything else (a file, the screen, the keyboard) */
extern int __modlib_sync (FILE *f);             /* the stream is idle at the position the program sees (what is buffered is written or dropped): 0, or EOF with errno */
extern int __modlib_open_length (const char *path, long *ext);   /* fcore.c: the length of PATH when a stream has it open for writing (name compared as given): 1 and *EXT, else 0 */
extern int __modlib_file_len (FILE *f, long *ext);   /* the length of the file: 0, or -1 with errno (EOVERFLOW at 2 GB) */
extern void __modlib_os_errno (void);           /* errno from the OS error that the last kernel call gave */
extern void (*__modlib_fd_hook) (FILE *f);      /* fdio.c: called when a stream that has a descriptor is closed */
extern void __modlib_register_end (void);       /* (fcore.c) the end of a program is to close the files and set the standard streams up again */
extern int (*__modlib_sock_end_hook) (void);     /* (fcore.c) set when the first socket descriptor is made: closes every socket at the end of a program and in __modlib_closeall (); the number of them */
extern int __modlib_sockets_closeall (void);     /* (fdio.c) close () of every socket descriptor: the number of them */
extern void __modlib_scrputc (int c);
extern int __modlib_vformat (void (*out) (int, void *), void *ctx, const char *fmt, va_list ap);
extern int __modlib_vscan (int (*get) (void *ctx), void *ctx, void (*putback) (int c, void *ctx), const char *fmt, va_list ap);
#endif
