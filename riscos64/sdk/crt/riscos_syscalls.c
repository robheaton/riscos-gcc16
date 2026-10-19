#define _GNU_SOURCE 1
/* riscos_syscalls.c - the system calls that picolibc wants (open, close, read, write, lseek, fstat, sbrk, _exit, isatty), made with RISC OS 64 SWIs: OS_Write0, OS_NewLine, OS_ReadC, OS_Find, OS_GBPB, OS_Args,
   OS_Exit.  The registers are those of the tests of riscos64/ (t05file passes on RISC OS Pyromaniac).  Descriptors 0, 1 and 2 are the keyboard and the screen; the others are
   RISC OS file handles plus 3 (so that no file is mistaken for the console). */
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

extern int swi_x (unsigned long swi, unsigned long *regs);	/* swi.S: the SWI with the X bit set; 1 if V was set */
int __riscos_tty_lflag = 0x11;				/* termios c_lflag of the console: ECHO | ICANON (termios.c changes it) */
int __riscos_tty_vmin = 1;				/* termios c_cc[VMIN] */
extern void __ul64_poll_signals (void);			/* posix/signal.c: Escape as SIGINT, the alarm () time as SIGALRM */
extern char __heap_start[];
extern char *__riscos_heap_end;

#define OS_WriteC 0x00
#define OS_NewLine 0x03
#define OS_ReadC 0x04
#define OS_Write0 0x02
#define OS_Args 0x09
#define OS_GBPB 0x0C
#define OS_Find 0x0D
#define OS_Exit 0x11
#define OS_File 0x08
#define OS_CLI 0x05
#define OS_Word 0x07
#define OS_Byte 0x06
#define OS_ReadVarVal 0x23
#define OS_ReadMonotonicTime 0x42
#define OS_FSControl 0x29

/* dup: a RISC OS file handle cannot be duplicated, so a copy is a new descriptor number (ALIAS_BASE and up) that stands for the same handle; refs counts the descriptors that use each handle (the handle is closed
   when the last one is closed).  The file position is shared, as it is for a dup'd descriptor.  Handles are 1-255. */
#define ALIAS_BASE 1000
#define NALIAS 256
static int alias_to[NALIAS];				/* the real descriptor that ALIAS_BASE + i stands for, or 0 */
static unsigned char refs[256];				/* descriptors per handle (0 = one: nothing was ever duplicated) */
static int realfd (int fd) { return fd >= ALIAS_BASE && fd < ALIAS_BASE + NALIAS ? alias_to[fd - ALIAS_BASE] : fd; }
static int fdhandle (int fd) { return realfd (fd) - 3; }
static int isfile (int fd) { return realfd (fd) >= 3; }
/* for UnixLib 64 and the other parts of the SDK that work on a descriptor: the RISC OS file handle of a descriptor, or -1 for the console */
int __riscos_handle (int fd) { return isfile (fd) ? fdhandle (fd) : -1; }

/* the screen: OS_Write0 for runs of text (copied to a buffer with a 0 at the end), OS_WriteC for a 0 byte, OS_NewLine for each \n.  (These are the SWIs that every RISC OS has had from the start; OS_WriteN was used before.)  */
/* The VDU drivers cannot clear to the end of a line.  The built-in termcap entry ("riscos") has ce and cd as the bytes 0xFF 0x1B 0x01 (clear to end of line) and 0xFF 0x1B 0x02 (clear to end of screen),
   which are done here, where the cursor position is the true one (a program that buffers its output has not written the text before them yet when it makes them). */
static void vdu (int c) { unsigned long q[10] = { (unsigned long) c & 255 }; swi_x (OS_WriteC, q); }
static void clear_text (int to_end_of_screen)
{
  unsigned long r[10] = { 134 };
  unsigned in32[3] = { 1u, 2u, 0xFFFFFFFFu }, out32[2] = { 79, 24 };
  unsigned long v[10] = { (unsigned long) in32, (unsigned long) out32 };
  unsigned x, y, cols, rows, c, row;
  if (swi_x (6, r)) return;						/* OS_Byte 134: the text cursor, R1 = column, R2 = row */
  x = (unsigned) (r[1] & 255); y = (unsigned) (r[2] & 255);
  swi_x (0x31, v);							/* OS_ReadVduVariables: ScrRCol, ScrBRow (the last column and row) */
  cols = out32[0] + 1; rows = out32[1] + 1;
  for (c = x; c < cols; c++) { if (y + 1 == rows && c + 1 == cols) break; vdu (' '); }		/* (not the last cell of the screen: that would scroll it) */
  if (to_end_of_screen)
    for (row = y + 1; row < rows; row++)
      {
	vdu (31); vdu (0); vdu ((int) row);
	for (c = 0; c < cols; c++) { if (row + 1 == rows && c + 1 == cols) break; vdu (' '); }
      }
  vdu (31); vdu ((int) x); vdu ((int) y);
}
static void put_text (const char *s, size_t n)
{
  size_t i = 0;
  char buf[256];
  while (i < n)
    {
      if ((unsigned char) s[i] == 0xFF && i + 2 < n && s[i + 1] == 0x1B && (s[i + 2] == 1 || s[i + 2] == 2)) { clear_text (s[i + 2] == 2); i += 3; }
      else if (s[i] == '\n') { unsigned long q[10] = { 0 }; swi_x (OS_NewLine, q); i++; }
      else if (s[i] == 0) { unsigned long q[10] = { 0 }; swi_x (OS_WriteC, q); i++; }
      else
	{
	  size_t k = 0;
	  unsigned long r[10] = { 0 };
	  while (i < n && s[i] != '\n' && s[i] != 0 && k < sizeof buf - 1 && !((unsigned char) s[i] == 0xFF && i + 2 < n && s[i + 1] == 0x1B)) buf[k++] = s[i++];
	  if (k == 0) { buf[k++] = s[i++]; }
	  buf[k] = 0;
	  r[0] = (unsigned long) buf;
	  swi_x (OS_Write0, r);
	}
    }
}

extern int __ul64_is_sock (int) __attribute__ ((weak));		/* posix/sockets.c: descriptors 300 and up that are sockets */
extern ssize_t __ul64_sock_read (int, void *, size_t) __attribute__ ((weak));
extern ssize_t __ul64_sock_write (int, const void *, size_t) __attribute__ ((weak));
extern int __ul64_sock_close (int) __attribute__ ((weak));
extern int __ul64_sock_fstat (int, struct stat *) __attribute__ ((weak));
#define SOCK(fd) (__ul64_is_sock && __ul64_is_sock (fd))
extern int __ul64_is_pipe (int) __attribute__ ((weak));		/* posix/pipe.c: descriptors 500-515, the read ends of pipes */
extern ssize_t __ul64_pipe_read (int, void *, size_t) __attribute__ ((weak));
extern int __ul64_pipe_close (int) __attribute__ ((weak));
extern int __ul64_pipe_defer_close (int) __attribute__ ((weak));
extern int __ul64_pipe_fstat (int, struct stat *) __attribute__ ((weak));
#define PIPE_R(fd) (__ul64_is_pipe && __ul64_is_pipe (fd))

/* The standard streams can be sent to a file or nowhere by the program that started this one (the shell does that for  cmd > file,  cmd < file,  cmd 2>file ...): the system variables Sh$Stdin, Sh$Stdout and
   Sh$Stderr hold the name of a file, or "null" (output thrown away, input empty), or, for Sh$Stderr, "stdout" (with the output).  Each is read once, at the first use of the descriptor; output is appended to
   the file.  The variables are inherited by the programs that a program starts (they are exported with the rest of the environment), as a redirection is. */
static long rfd_lines = -1, rfd_written;		/* Sh$StdoutLines: the shell says the program after the pipe wants only this many lines of standard output */
static struct { int state; int handle; int owned; } rfd[3];		/* owned: the handle was opened here for Sh$Stdout and the like (else it is a file of the program that dup2 put on the descriptor) */		/* state: 0 not looked at, 1 the console, 2 nowhere, 3 a file (handle), 4 (stderr) with standard output */
static int rfd_pos_given;					/* Sh$StdinPos was given: tell the shell where this program stopped reading */
static char rfd_saved[3][512];					/* the file names that Sh$Stdout and Sh$Stderr had when they were read (the variables may be changed afterwards, for a child) */
static void rfd_init (int fd)
{
  static const char *const names[3] = { "Sh$Stdin", "Sh$Stdout", "Sh$Stderr" };
  char buf[512];
  unsigned long r[10] = { (unsigned long) names[fd], (unsigned long) buf, sizeof buf - 1, 0, 3 };
  rfd[fd].state = 1;
  if (fd == 1)
    {
      char lb[24];
      unsigned long q[10] = { (unsigned long) "Sh$StdoutLines", (unsigned long) lb, sizeof lb - 1, 0, 3 };
      if (!swi_x (0x23, q) && q[2] < sizeof lb) { lb[q[2]] = 0; if (lb[0] >= '0' && lb[0] <= '9') rfd_lines = atol (lb); }
    }
  if (swi_x (0x23, r) || r[2] >= sizeof buf) return;		/* OS_ReadVarVal */
  buf[r[2]] = 0;
  memcpy (rfd_saved[fd], buf, r[2] + 1);
  if (!strcmp (buf, "null")) { rfd[fd].state = 2; return; }
  if (fd == 2 && !strcmp (buf, "stdout")) { rfd[fd].state = 4; return; }
  {
    unsigned long o[10] = { fd == 0 ? 0x40UL : 0xC0UL, (unsigned long) buf };	/* OS_Find: open for reading (stdin), or for update, else create (output) */
    if (swi_x (OS_Find, o) || !o[0])
      {
	if (fd == 0) return;
	o[0] = 0x80; o[1] = (unsigned long) buf;
	if (swi_x (OS_Find, o) || !o[0]) return;
      }
    rfd[fd].handle = (int) o[0]; rfd[fd].owned = 1;
    if (fd != 0)
      {
	unsigned long e[10] = { 2, o[0] };			/* the extent, and the pointer to the end of the file */
	if (!swi_x (OS_Args, e)) { unsigned long p[10] = { 1, o[0], e[2] }; swi_x (OS_Args, p); }
      }
    rfd[fd].state = 3;
    if (fd == 0)							/* the shell says where the earlier commands of this redirection stopped reading (they share one file position, as on Unix) */
      {
	char pb[24];
	unsigned long pq[10] = { (unsigned long) "Sh$StdinPos", (unsigned long) pb, sizeof pb - 1, 0, 3 };
	if (!swi_x (0x23, pq) && pq[2] > 0 && pq[2] < sizeof pb)
	  {
	    unsigned long sp[10] = { 1, o[0], (unsigned long) strtoul (pb, 0, 10) };	/* OS_Args 1: set the file pointer */
	    pb[pq[2]] = 0; sp[2] = strtoul (pb, 0, 10);
	    swi_x (OS_Args, sp);
	    rfd_pos_given = 1;
	  }
      }
  }
}
static int rfd_state (int fd) { if (!rfd[fd].state) rfd_init (fd); return rfd[fd].state; }
/* where this program stopped reading standard input (at exit, or when it closes it: head does): the shell gives it to the next command of the group */
static void rfd_report_pos (void)
{
  unsigned long ap[10] = { 0, 0 };
  if (!rfd_pos_given || rfd[0].state != 3) return;
  ap[1] = (unsigned long) rfd[0].handle;						/* OS_Args 0: read the file pointer */
  if (!swi_x (OS_Args, ap))
    {
      char pb[24];
      unsigned long sv[10] = { (unsigned long) "Sh$StdinPosOut", (unsigned long) pb, 0, 0, 4, 0, 0, 0, 0, 0 };
      sv[2] = (unsigned long) snprintf (pb, sizeof pb, "%lu", ap[2]);
      swi_x (0x24, sv);								/* OS_SetVarVal, type 4: a string */
    }
}
/* a standard descriptor stops being a file (close, or dup2 onto it): the handle goes if this was its last user */
static void rfd_drop (int fd)
{
  int h;
  if (rfd[fd].state != 3) return;
  if (fd == 0) rfd_report_pos ();
  h = rfd[fd].handle;
  if (!rfd[fd].owned && refs[h & 255] > 1) refs[h & 255]--;
  else { unsigned long c[10] = { 0, (unsigned long) h }; swi_x (OS_Find, c); if (!rfd[fd].owned) { refs[h & 255] = 0; } }
  rfd[fd].state = 1; rfd[fd].handle = 0; rfd[fd].owned = 0;
}
static void rfd_close (void)
{
  int fd;
  for (fd = 0; fd < 3; fd++) if (rfd[fd].state == 3) { unsigned long c[10] = { 0, (unsigned long) rfd[fd].handle }; swi_x (OS_Find, c); rfd[fd].state = 1; }
}

/* A program that starts another program: the output files that Sh$Stdout and Sh$Stderr named are open here, and the program started will open them too (its Sh$Stdout is the same): RISC OS lets a file be open for
   writing once, so the child would find it busy and write to the screen.  So the handles are closed (the data goes to the file) while the child runs and opened again, at the end of the file, afterwards. */
void __ul64_stdio_suspend (void)
{
  int fd;
  for (fd = 1; fd < 3; fd++)
    {
      if (rfd[fd].state != 3 || !rfd[fd].owned) continue;
      { unsigned long c[10] = { 0, (unsigned long) rfd[fd].handle }; swi_x (OS_Find, c); }
      rfd[fd].state = 6;							/* closed for the time being */
    }
}
void __ul64_stdio_resume (void)
{
  int fd;
  for (fd = 1; fd < 3; fd++)
    {
      unsigned long o[10];
      if (rfd[fd].state != 6) continue;
      rfd[fd].state = 1;
      o[0] = 0xC0; o[1] = (unsigned long) rfd_saved[fd];
      if (swi_x (OS_Find, o) || !o[0]) { o[0] = 0x80; o[1] = (unsigned long) rfd_saved[fd]; if (swi_x (OS_Find, o) || !o[0]) continue; }
      rfd[fd].handle = (int) o[0]; rfd[fd].owned = 1; rfd[fd].state = 3;
      { unsigned long e[10] = { 2, o[0] }; if (!swi_x (OS_Args, e)) { unsigned long q[10] = { 1, o[0], e[2] }; swi_x (OS_Args, q); } }	/* to the end of the file */
    }
}

ssize_t write (int fd, const void *buf, size_t n)
{
  unsigned long r[10] = { 0 };
  if (fd == 1 || fd == 2)
    {
      int st = rfd_state (fd), h = rfd[fd].handle;
      if (fd == 2 && st == 4) { st = rfd_state (1); h = rfd[1].handle; }
      if (st == 2) return (ssize_t) n;
      if (st == 3)
	{
	  unsigned long w[10] = { 2, (unsigned long) h, (unsigned long) buf, n };
	  int last = 0;
	  if (fd == 1 && rfd_lines >= 0)		/* the reader after the pipe wants rfd_lines lines: write up to the end of the last one and stop, as a write to a closed pipe does (SIGPIPE) */
	    {
	      size_t i;
	      for (i = 0; i < n; i++)
		if (((const char *) buf)[i] == '\n' && ++rfd_written >= rfd_lines) { w[3] = i + 1; last = 1; break; }
	      if (!last && rfd_lines == 0) { w[3] = 0; last = 1; }
	    }
	  if (swi_x (OS_GBPB, w)) { errno = EIO; return -1; }
	  if (last) { rfd_close (); _exit (141); }
	  return (ssize_t) (n - w[3]);
	}
    }
  if (fd == 1 || fd == 2) { __ul64_poll_signals (); put_text ((const char *) buf, n); return (ssize_t) n; }
  if (!isfile (fd)) { errno = EBADF; return -1; }
  r[0] = 2; r[1] = (unsigned long) fdhandle (fd); r[2] = (unsigned long) buf; r[3] = n;
  if (swi_x (OS_GBPB, r)) { errno = EIO; return -1; }
  return (ssize_t) (n - r[3]);
}

ssize_t read (int fd, void *buf, size_t n)
{
  unsigned long r[10] = { 0 };
  if (SOCK (fd)) return __ul64_sock_read (fd, buf, n);
  if (PIPE_R (fd)) return __ul64_pipe_read (fd, buf, n);
  if (fd == 0)
    {
      int st = rfd_state (0);
      if (st == 2) return 0;
      if (st == 3) { unsigned long q[10] = { 4, (unsigned long) rfd[0].handle, (unsigned long) buf, n }; if (swi_x (OS_GBPB, q)) { errno = EIO; return -1; } return (ssize_t) (n - q[3]); }
    }
  if (fd == 0 && !(__riscos_tty_lflag & 0x10))		/* the keyboard, termios non-canonical: no line editing, returns when VMIN characters have come (0 = whatever is waiting) */
    {
      char *p = (char *) buf;
      size_t k = 0, want = __riscos_tty_vmin > 0 ? (size_t) __riscos_tty_vmin : 0;
      if (want > n) want = n;
      while (k < n)
	{
	  unsigned long q[10] = { 0 };
	  if (k >= want)
	    {
	      q[0] = 128; q[1] = 255;			/* OS_Byte 128: the number of characters waiting in the keyboard buffer */
	      if (swi_x (OS_Byte, q) || (q[1] & 255) == 0) break;
	      q[0] = q[1] = 0;
	    }
	  if (swi_x (OS_ReadC, q)) { errno = EIO; return -1; }
	  if (q[1] == 1) { __ul64_poll_signals (); return k ? (ssize_t) k : 0; }
	  p[k++] = (char) (q[0] & 255);
	  if (__riscos_tty_lflag & 1) { unsigned long z[10] = { q[0] & 255 }; swi_x (OS_WriteC, z); }
	}
      return (ssize_t) k;
    }
  if (fd == 0)						/* the keyboard: one line, echoed, with backspace; Escape ends the input */
    {
      char *p = (char *) buf;
      size_t k = 0;
      while (k < n)
	{
	  unsigned long q[10] = { 0 };
	  int c;
	  if (swi_x (OS_ReadC, q)) { errno = EIO; return -1; }
	  if (q[1] == 1) { __ul64_poll_signals (); return (ssize_t) k; }		/* Escape */
	  c = (int) (q[0] & 255);
	  if (c == 13 || c == 10) { unsigned long z[10] = { 0 }; swi_x (OS_NewLine, z); p[k++] = '\n'; break; }
	  if ((c == 8 || c == 127) && k > 0) { unsigned long z[10] = { 127 }; swi_x (OS_WriteC, z); k--; continue; }
	  if (c >= 32 && c != 127) { unsigned long z[10] = { (unsigned long) c }; swi_x (OS_WriteC, z); p[k++] = (char) c; }
	}
      return (ssize_t) k;
    }
  if (!isfile (fd)) { errno = EBADF; return -1; }
  r[0] = 4; r[1] = (unsigned long) fdhandle (fd); r[2] = (unsigned long) buf; r[3] = n;
  if (swi_x (OS_GBPB, r))
    {
      unsigned long a[10] = { 0 }, b[10] = { 0 };				/* an error at the end of the file (or of an empty one) is the end, not an I/O error */
      a[0] = 0; a[1] = (unsigned long) fdhandle (fd); b[0] = 2; b[1] = a[1];
      if (!swi_x (OS_Args, b)) { unsigned long c[10] = { 0 }; c[0] = 0; c[1] = a[1]; if (!swi_x (OS_Args, c) && c[2] >= b[2]) return 0; }
      errno = EIO; return -1;
    }
  return (ssize_t) (n - r[3]);				/* r3 = the bytes that were not transferred: 0 bytes read is the end of the file */
}

static int os_errno_of (unsigned long *r);

/* UnixLib 64 (riscos64/unixlib, -lunixlib64) translates Unix file names to RISC OS names: it provides __riscosify_std.  Without it the names are used as they are given (the names of RISC OS). */
extern char *__riscosify_std (const char *name, int create_dir, char *buffer, size_t buf_len, int *filetype) __attribute__ ((weak));
#define ROSPATH 1024
static const char *ros_path (const char *name, char *buf, int create_dir, int *filetype)
{
  if (!__riscosify_std) return name;
  if (!__riscosify_std (name, create_dir, buf, ROSPATH, filetype)) { errno = ENAMETOOLONG; return NULL; }
  return buf;
}

/* the RISC OS name of a (Unix or RISC OS) file name: the name itself without UnixLib 64, else its translation in buf (NULL with errno set if it does not fit) */
const char *__riscos_path (const char *name, char *buf, int create_dir) { return ros_path (name, buf, create_dir, NULL); }

/* the name of each open file, for fchdir () (and programs that open a folder to come back to it): the full Unix name at the time of the open */
extern char *getcwd (char *, size_t) __attribute__ ((weak));
extern int chdir (const char *) __attribute__ ((weak));
static char *fd_names[256];
static void remember_name (int handle, const char *unixpath)
{
  char cwd[512];
  size_t n;
  char *full;
  free (fd_names[handle & 255]);
  fd_names[handle & 255] = 0;
  if (!unixpath) return;
  if (unixpath[0] == '/' || !getcwd || !getcwd (cwd, sizeof cwd)) { fd_names[handle & 255] = strdup (unixpath); return; }
  while (unixpath[0] == '.' && unixpath[1] == '/') unixpath += 2;
  n = strlen (cwd) + strlen (unixpath) + 2;
  full = malloc (n);
  if (!full) return;
  if (!strcmp (unixpath, ".")) snprintf (full, n, "%s", cwd);
  else snprintf (full, n, "%s/%s", cwd, unixpath);
  fd_names[handle & 255] = full;
}
const char *__riscos_fd_name (int fd) { return isfile (fd) ? fd_names[fdhandle (fd) & 255] : 0; }	/* the Unix name an open file had (a folder opened for fchdir, fdopendir, the *at calls) */

int fchdir (int fd)
{
  const char *name = isfile (fd) ? fd_names[fdhandle (fd) & 255] : 0;
  if (!name || !chdir) { errno = EBADF; return -1; }
  return chdir (name);
}

int open (const char *path, int flags, ...)
{
  unsigned long r[10] = { 0 };
  if (path && !strcmp (path, "/dev/null"))		/* the null device: an empty scrap file; readers (any number) see the end at once, each writer has a file of its own */
    {
      static unsigned nullseq;
      static char nullname[40];
      if ((flags & O_ACCMODE) == O_RDONLY)
	{
	  int t = open ("/tmp/devnull", O_WRONLY | O_CREAT, 0666);		/* make it if it is not there (empty) */
	  if (t >= 0) close (t);
	  path = "/tmp/devnull";
	}
      else
	{
	  snprintf (nullname, sizeof nullname, "/tmp/devnull%u", ++nullseq);
	  path = nullname;
	  flags = (flags & ~(O_ACCMODE | O_EXCL)) | O_RDWR | O_CREAT | O_TRUNC;
	}
    }
  int acc = flags & O_ACCMODE;
  char rosbuf[ROSPATH];
  int filetype = -1, isnew = 0, exists = 0;
  const char *rospath = ros_path (path, rosbuf, (flags & O_CREAT) != 0, &filetype);
  const char *unixpath = path;
  if (!rospath) return -1;
  if (acc != O_RDONLY)								/* does it exist?  (OPENUP of a missing file fails on some filing systems and makes it on others, so do not rely on it) */
    {
      unsigned long q[10] = { 17, (unsigned long) rospath };
      if (!swi_x (OS_File, q)) exists = q[0] != 0;
      if (exists && (flags & O_CREAT) && (flags & O_EXCL)) { errno = EEXIST; return -1; }
      if (exists && q[0] == 2) { errno = EISDIR; return -1; }
      if (!exists && !(flags & O_CREAT)) { errno = ENOENT; return -1; }
      isnew = !exists;
    }
  if (flags & O_DIRECTORY)
    {
      unsigned long q[10] = { 17, (unsigned long) rospath };
      if (!swi_x (OS_File, q) && q[0] == 1) { errno = ENOTDIR; return -1; }
    }
  path = rospath;
  r[1] = (unsigned long) path;
  if (acc == O_RDONLY) r[0] = 0x40;						/* OPENIN */
  else if (isnew || (flags & O_TRUNC)) r[0] = 0x80;				/* OPENOUT: makes the file or empties it; it can be read as well */
  else r[0] = 0xC0;								/* OPENUP: the file is there */
  if (swi_x (OS_Find, r)) { errno = os_errno_of (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  if (isnew && __riscosify_std)
    {
      unsigned long q[10] = { 18, (unsigned long) path, filetype < 0 ? 0xFFF : (unsigned long) filetype };	/* OS_File 18: set the file type */
      swi_x (OS_File, q);
    }
  if (flags & O_APPEND)
    {
      unsigned long q[10] = { 2, r[0] };
      swi_x (OS_Args, q);				/* OS_Args 2: the extent in r2 */
      q[2] = q[2]; q[0] = 1; q[1] = r[0];
      swi_x (OS_Args, q);				/* OS_Args 1: set the pointer */
    }
  refs[r[0] & 255] = 1;
  remember_name ((int) r[0], unixpath);
  return (int) r[0] + 3;
}

int close (int fd)
{
  unsigned long r[10] = { 0 };
  int rf, h;
  if (__ul64_pipe_defer_close && __ul64_pipe_defer_close (fd)) return 0;
  if (SOCK (fd)) return __ul64_sock_close (fd);
  if (PIPE_R (fd)) return __ul64_pipe_close (fd);
  if (fd >= 0 && fd < 3) { rfd_drop (fd); return 0; }
  if (!isfile (fd)) return 0;
  rf = realfd (fd);
  h = rf - 3;
  if (fd >= ALIAS_BASE) alias_to[fd - ALIAS_BASE] = 0;
  if (refs[h & 255] > 1) { refs[h & 255]--; return 0; }	/* another descriptor still has the handle */
  refs[h & 255] = 0;
  remember_name (h, 0);
  r[0] = 0; r[1] = (unsigned long) h;
  if (swi_x (OS_Find, r)) { errno = EIO; return -1; }
  return 0;
}

/* dup and dup2 of a file descriptor (not of the console: the screen and keyboard cannot be redirected to a file this way) */
int __riscos_dup (int fd, int want)			/* want < 0: any free number; else that one (dup2) */
{
  int i, rf = realfd (fd), h = rf - 3;
  if (rf < 3 || h < 0 || h > 255) { errno = rf >= 0 && rf < 3 ? ENOSYS : EBADF; return -1; }
  if (want >= 0 && want == fd) return want;
  if (want >= 0 && want < 3)			/* dup2 (file, 0|1|2): the standard stream is now that file (shared handle, shared position) */
    {
      rfd_drop (want);
      rfd[want].state = 3; rfd[want].handle = h; rfd[want].owned = 0;
      if (refs[h & 255] == 0) refs[h & 255] = 1;
      refs[h & 255]++;
      return want;
    }
  if (want >= 0)
    {
      if (want < ALIAS_BASE || want >= ALIAS_BASE + NALIAS) { errno = ENOSYS; return -1; }	/* (only the numbers the SDK hands out for copies) */
      if (alias_to[want - ALIAS_BASE]) close (want);
      i = want - ALIAS_BASE;
    }
  else
    {
      for (i = 0; i < NALIAS && alias_to[i]; i++) ;
      if (i == NALIAS) { errno = EMFILE; return -1; }
    }
  alias_to[i] = rf;
  if (refs[h & 255] == 0) refs[h & 255] = 1;
  refs[h & 255]++;
  return ALIAS_BASE + i;
}

off_t lseek (int fd, off_t off, int whence)
{
  unsigned long r[10] = { 0 };
  long pos = off;
  unsigned long h;
  if (fd >= 0 && fd < 3 && rfd_state (fd) == 3) h = (unsigned long) rfd[fd].handle;	/* a standard stream redirected to a file can be seeked (head, tail and sed -n rely on it) */
  else if (isfile (fd)) h = (unsigned long) fdhandle (fd);
  else { errno = ESPIPE; return -1; }
  if (whence == SEEK_CUR) { r[0] = 0; r[1] = h; if (swi_x (OS_Args, r)) { errno = EIO; return -1; } pos = (long) r[2] + off; }
  else if (whence == SEEK_END) { r[0] = 2; r[1] = h; if (swi_x (OS_Args, r)) { errno = EIO; return -1; } pos = (long) r[2] + off; }
  { unsigned long q[10] = { 1, h, (unsigned long) pos }; if (swi_x (OS_Args, q)) { errno = EIO; return -1; } }
  return (off_t) pos;
}

static unsigned long name_ino (const char *name);
int fstat (int fd, struct stat *st)
{
  unsigned long r[10] = { 0 };
  if (SOCK (fd)) return __ul64_sock_fstat (fd, st);
  if (PIPE_R (fd)) return __ul64_pipe_fstat (fd, st);
  char *p = (char *) st;
  size_t i;
  for (i = 0; i < sizeof *st; i++) p[i] = 0;
  if (fd >= 0 && fd < 3 && rfd_state (fd) == 3)		/* standard stream redirected to a file: a regular file, so stdio buffers it (not a line at a time as for a terminal) */
    {
      r[0] = 2; r[1] = (unsigned long) rfd[fd].handle;
      st->st_mode = S_IFREG | 0644; st->st_dev = 1;
      if (!swi_x (OS_Args, r)) st->st_size = (off_t) r[2];
      {
	char nm[ROSPATH];
	unsigned long n[10] = { 7, (unsigned long) rfd[fd].handle, (unsigned long) nm, 0, 0, sizeof nm };	/* OS_Args 7: the name of the file of a handle (the inode: tools compare standard input with standard output) */
	if (!swi_x (OS_Args, n)) st->st_ino = (ino_t) name_ino (nm);
      }
      return 0;
    }
  if (!isfile (fd)) { st->st_mode = S_IFCHR | 0600; return 0; }
  {
    const char *nm = fd_names[fdhandle (fd) & 255];		/* the name it was opened with: the type of the object (a folder), the date ... */
    struct stat s2;
    if (nm && stat (nm, &s2) == 0) { *st = s2; return 0; }
  }
  r[0] = 2; r[1] = (unsigned long) fdhandle (fd);
  if (swi_x (OS_Args, r)) { errno = EIO; return -1; }
  st->st_mode = S_IFREG | 0644;
  st->st_size = (off_t) r[2];
  st->st_dev = 1;
  {
    char nm[ROSPATH];
    unsigned long n[10] = { 7, (unsigned long) fdhandle (fd), (unsigned long) nm, 0, 0, sizeof nm };	/* OS_Args 7: the name of the file of a handle */
    if (!swi_x (OS_Args, n)) st->st_ino = (ino_t) name_ino (nm);
  }
  return 0;
}

int isatty (int fd) { return fd >= 0 && fd < 3 && rfd_state (fd) == 1; }

/* standard output redirected to a file is buffered in full, as on Unix (picolibc's stdout is line buffered: one OS_GBPB per line, which is slow, most of all under an emulator) */
__attribute__ ((constructor (102))) static void stdout_buffering (void)
{
  if (rfd_state (1) == 3) setvbuf (stdout, NULL, _IOFBF, BUFSIZ);
}

/* The heap is a dynamic area (OS_DynamicArea), created on the first sbrk with room to grow to 1 GB (or the largest of 256 MB and 64 MB that the system allows) and grown a page at a time by OS_ChangeDynamicArea:
   the application space that a program starts with is small (12 MB on Pyromaniac).  If no dynamic area can be made, sbrk uses the application space, from the end of the program to the RAM limit minus the stack.
   _exit removes the area.  (A program that is killed leaves it; a stricter exit handler is part of the UnixLib plan.) */
#define OS_ChangeDynamicArea 0x2A
#define OS_DynamicArea 0x66
static int da_mode;				/* 0 = not tried, 1 = a dynamic area, 2 = the application space */
static unsigned long da_number, da_size, da_max;
static char *da_base, *da_cur;

static void da_create (void)
{
  static const unsigned long max[3] = { 1UL << 30, 256UL << 20, 64UL << 20 };
  int i;
  da_mode = 2;
  for (i = 0; i < 3; i++)
    {
      unsigned long r[10] = { 0 };
      r[0] = 0; r[1] = (unsigned long) -1; r[2] = 4096; r[3] = (unsigned long) -1; r[4] = 0x80; r[5] = max[i]; r[8] = (unsigned long) "SDK heap";
      if (!swi_x (OS_DynamicArea, r))
	{
	  da_number = r[1] & 0xFFFFFFFF; da_base = da_cur = (char *) r[3]; da_size = 4096; da_max = max[i]; da_mode = 1;
	  return;
	}
    }
}

void *sbrk (ptrdiff_t incr)
{
  static char *cur;
  char *old;
  if (da_mode == 0) da_create ();
  if (da_mode == 1)
    {
      char *new_ = da_cur + incr;
      old = da_cur;
      if (new_ < da_base) { errno = ENOMEM; return (void *) -1; }
      if ((size_t) (new_ - da_base) > da_size)
	{
	  unsigned long need = ((size_t) (new_ - da_base) - da_size + 4095) & ~4095UL, grow = need, r[10] = { 0 };
	  if (grow < (256UL << 10)) grow = 256UL << 10;			/* in steps of at least 256 KB or an eighth of the size: a step is a new region of memory for an emulator (Pyromaniac's limit is a few thousand) */
	  if (grow < (da_size >> 3)) grow = (da_size >> 3) & ~4095UL;
	  if (need > da_max - da_size) { errno = ENOMEM; return (void *) -1; }		/* more than the area was made to hold: an error here, not a request that an emulator cannot map */
	  if (grow > da_max - da_size) grow = da_max - da_size;
	  r[0] = da_number; r[1] = grow;
	  if (swi_x (OS_ChangeDynamicArea, r)) { r[0] = da_number; r[1] = need; r[2] = 0; if (need == grow || swi_x (OS_ChangeDynamicArea, r)) { errno = ENOMEM; return (void *) -1; } }
	  da_size += r[1] & 0xFFFFFFFF;
	  if ((size_t) (new_ - da_base) > da_size) { errno = ENOMEM; return (void *) -1; }
	}
      da_cur = new_;
      return old;
    }
  if (!cur) cur = __heap_start;
  old = cur;
  if (incr > 0 && (size_t) incr > (size_t) (__riscos_heap_end - cur)) { errno = ENOMEM; return (void *) -1; }
  if (incr < 0 && (size_t) -incr > (size_t) (cur - __heap_start)) { errno = ENOMEM; return (void *) -1; }
  cur += incr;
  return old;
}

extern void __ul64_vfork_exit (int) __attribute__ ((weak));	/* process.c: when a vfork "child" ends, go back to the parent instead */
void _exit (int code)
{
  static const struct { unsigned num; char msg[20]; } done = { 0, "Program finished" };
  unsigned long r[10] = { 0 };
  if (__ul64_vfork_exit) __ul64_vfork_exit (code);
  rfd_report_pos ();
  rfd_close ();
  { int h; for (h = 1; h < 256; h++) if (fd_names[h]) { unsigned long c[10] = { 0, (unsigned long) h }; swi_x (OS_Find, c); } }	/* the files that were left open (RISC OS does not close them: the next program would find them in use, a pipe () file for one) */
  if (da_mode == 1) { unsigned long q[10] = { 1, da_number }; swi_x (OS_DynamicArea, q); }		/* remove the heap */
  r[0] = (unsigned long) &done;
  r[1] = 0x58454241UL;					/* "ABEX" */
  r[2] = (unsigned long) (long) code;
  swi_x (OS_Exit, r);
  for (;;) ;
}

/* ---- the rest of what a C program expects of the system: files by name, the clock, the environment, system () ---- */
static int os_errno_of (unsigned long *r)					/* the error block that came back in r[0]: an errno */
{
  switch (((const unsigned *) r[0])[0] & 0xFF)
    {
    case 0xD6: return ENOENT;							/* File not found */
    case 0xBD: case 0xC3: return EACCES;					/* access violation, locked */
    case 0xC2: return EBUSY;							/* file open */
    case 0xB0: return EEXIST;							/* bad rename */
    case 0xB4: return ENOTEMPTY;						/* directory not empty */
    case 0xA8: return EISDIR;
    case 0xC6: return ENOSPC;
    default: return EIO;
    }
}

int unlink (const char *path)
{
  char b[ROSPATH];
  unsigned long r[10] = { 6 };
  if (!(path = ros_path (path, b, 0, NULL))) return -1;
  r[1] = (unsigned long) path;
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }					/* r0 = the type of the object that was deleted: 0 = there was none */
  return 0;
}

int rename (const char *from, const char *to)
{
  char b1[ROSPATH], b2[ROSPATH];
  unsigned long r[10] = { 25 };
  if (!(from = ros_path (from, b1, 0, NULL)) || !(to = ros_path (to, b2, 1, NULL))) return -1;
  {							/* POSIX rename replaces a file that is there; RISC OS refuses: delete it first (a directory is not replaced) */
    unsigned long q[10] = { 17, (unsigned long) to };
    if (!swi_x (OS_File, q) && q[0] == 1)
      {
	unsigned long d[10] = { 6, (unsigned long) to };
	swi_x (OS_File, d);
      }
  }
  r[1] = (unsigned long) from; r[2] = (unsigned long) to;
  if (swi_x (OS_FSControl, r)) { errno = os_errno_of (r); return -1; }
  return 0;
}

int mkdir (const char *path, mode_t mode)
{
  char b[ROSPATH];
  unsigned long r[10] = { 8, 0, 0, 0, 0 };
  (void) mode;
  if (!(path = ros_path (path, b, 0, NULL))) return -1;
  r[1] = (unsigned long) path;
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  return 0;
}

/* An inode number does not exist on RISC OS: a hash of the full name of the file stands for it (so that two names of one file are one inode, and two files are two: programs such as sed -i and mv
   compare inodes to see whether a rename would be of a file onto itself). */
static unsigned long name_ino (const char *name)
{
  char full[ROSPATH];
  unsigned long r[10] = { 37, (unsigned long) name, (unsigned long) full, 0, 0, sizeof full };	/* OS_FSControl 37: the canonical name */
  unsigned long long h = 1469598103934665603ULL;
  const unsigned char *p = (const unsigned char *) name;
  if (!swi_x (OS_FSControl, r)) p = (const unsigned char *) full;
  for (; *p; p++) { h ^= (*p >= 'A' && *p <= 'Z') ? (unsigned char) (*p + 32) : *p; h *= 1099511628211ULL; }	/* (the names are not case sensitive) */
  return (unsigned long) (h & 0x7FFFFFFFFFFFFFFFULL) | 1;
}

int stat (const char *path, struct stat *st)
{
  char b[ROSPATH];
  if (path && !strcmp (path, "/dev/null")) { memset (st, 0, sizeof *st); st->st_mode = S_IFCHR | 0666; st->st_dev = 1; st->st_ino = 3; return 0; }
  unsigned long r[10] = { 17 };
  char *p = (char *) st;
  size_t i;
  if (!(path = ros_path (path, b, 0, NULL))) return -1;
  r[1] = (unsigned long) path;
  for (i = 0; i < sizeof *st; i++) p[i] = 0;
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  if (r[0] == 2) st->st_mode = S_IFDIR | 0755;
  else st->st_mode = S_IFREG | ((r[5] & 1) ? 0444 : 0) | ((r[5] & 2) ? 0222 : 0);
  st->st_size = (off_t) (r[4] & 0xFFFFFFFF);
  st->st_nlink = 1;
  st->st_dev = 1;
  st->st_ino = (ino_t) name_ino (path);
  if ((r[2] & 0xFFF00000UL) == 0xFFF00000UL)					/* a date stamp: the load address top 12 bits are &FFF, then 40 bits of centiseconds since 1900 */
    {
      unsigned long long cs = ((unsigned long long) (r[2] & 0xFF) << 32) | (r[3] & 0xFFFFFFFFUL);
      st->st_mtime = (time_t) (cs / 100 - 2208988800ULL);
    }
  return 0;
}
int lstat (const char *path, struct stat *st) { return stat (path, st); }
int access (const char *path, int mode)
{
  struct stat st;
  if (stat (path, &st)) return -1;
  if ((mode & W_OK) && !(st.st_mode & 0222)) { errno = EACCES; return -1; }
  return 0;
}

/* the real time clock: OS_Word 14, 3 gives 5 bytes of centiseconds since 1900 */
static int rtc (long long *sec, int *cs)
{
  unsigned char b[8] = { 3 };
  unsigned long r[10] = { 14, (unsigned long) b };
  unsigned long long v;
  if (swi_x (OS_Word, r)) return -1;
  v = (unsigned long long) b[0] | (unsigned long long) b[1] << 8 | (unsigned long long) b[2] << 16 | (unsigned long long) b[3] << 24 | (unsigned long long) b[4] << 32;
  *sec = (long long) (v / 100) - 2208988800LL;
  *cs = (int) (v % 100);
  return 0;
}
int gettimeofday (struct timeval *tv, void *tz)
{
  long long sec; int cs;
  (void) tz;
  if (rtc (&sec, &cs)) { errno = EIO; return -1; }
  if (tv) { tv->tv_sec = (time_t) sec; tv->tv_usec = cs * 10000; }
  return 0;
}
int clock_gettime (clockid_t id, struct timespec *ts)
{
  if (id == CLOCK_MONOTONIC || id == CLOCK_PROCESS_CPUTIME_ID)
    {
      unsigned long r[10] = { 0 };
      swi_x (OS_ReadMonotonicTime, r);
      ts->tv_sec = (time_t) ((r[0] & 0xFFFFFFFFUL) / 100);
      ts->tv_nsec = (long) ((r[0] & 0xFFFFFFFFUL) % 100) * 10000000L;
      return 0;
    }
  else
    {
      long long sec; int cs;
      if (rtc (&sec, &cs)) { errno = EIO; return -1; }
      ts->tv_sec = (time_t) sec; ts->tv_nsec = cs * 10000000L;
      return 0;
    }
}
clock_t times (struct tms *t)							/* microseconds (picolibc's CLK_TCK is CLOCKS_PER_SEC, 1000000) since the machine started, from the centisecond tick; the user time is the same (there is no other process) */
{
  unsigned long r[10] = { 0 };
  clock_t now;
  swi_x (OS_ReadMonotonicTime, r);
  now = (clock_t) (r[0] & 0xFFFFFFFFUL) * 10000;
  if (t) { t->tms_utime = now; t->tms_stime = 0; t->tms_cutime = 0; t->tms_cstime = 0; }
  return now;
}

/* the environment is the system variables of RISC OS (OS_ReadVarVal) */
/* getenv: the variables that the program set itself (setenv, putenv: picolibc keeps them in environ) first, then the system variables of RISC OS (OS_ReadVarVal, GSTrans'd, type 3).  The value of a system variable
   is kept in a small cache, so that the result of one call is not overwritten by the next call for another name (a name's value is replaced when the name is asked again).  */
extern char **environ;
__attribute__ ((weak)) char *getenv (const char *name)
{
  static struct { char *name, *val; } cache[32];
  static unsigned next;
  char buf[1024];
  unsigned long r[10];
  size_t n, i;
  char **e;
  if (!name || !*name || strchr (name, '=')) return 0;
  n = strlen (name);
  for (e = environ; e && *e; e++) if (strncmp (*e, name, n) == 0 && (*e)[n] == '=') return *e + n + 1;
  r[0] = (unsigned long) name; r[1] = (unsigned long) buf; r[2] = sizeof buf - 1; r[3] = 0; r[4] = 3;
  if (swi_x (OS_ReadVarVal, r) || r[2] >= sizeof buf) return 0;
  buf[r[2]] = 0;
  for (i = 0; i < 32; i++) if (cache[i].name && strcmp (cache[i].name, name) == 0) break;
  if (i == 32) { i = next++ % 32; free (cache[i].name); cache[i].name = strdup (name); cache[i].val = 0; }
  free (cache[i].val);
  cache[i].val = strdup (buf);
  return cache[i].val;
}
