#define _GNU_SOURCE 1
/* riscos_syscalls.c - the system calls that picolibc wants (open, close, read, write, lseek, fstat, sbrk, _exit, isatty), made with RISC OS 64 SWIs: OS_WriteN, OS_NewLine, OS_ReadC, OS_Find, OS_GBPB, OS_Args,
   OS_Exit.  The registers are those of the tests of riscos64/ (t05file passes on RISC OS Pyromaniac).  Descriptors 0, 1 and 2 are the keyboard and the screen; the others are
   RISC OS file handles plus 3 (so that no file is mistaken for the console). */
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

extern int swi_x (unsigned long swi, unsigned long *regs);	/* swi.S: the SWI with the X bit set; 1 if V was set */
extern char __heap_start[];
extern char *__riscos_heap_end;

#define OS_WriteC 0x00
#define OS_NewLine 0x03
#define OS_ReadC 0x04
#define OS_WriteN 0x46
#define OS_Args 0x09
#define OS_GBPB 0x0C
#define OS_Find 0x0D
#define OS_Exit 0x11
#define OS_File 0x08
#define OS_CLI 0x05
#define OS_Word 0x07
#define OS_ReadVarVal 0x23
#define OS_ReadMonotonicTime 0x42
#define OS_FSControl 0x29

static int fdhandle (int fd) { return fd - 3; }
static int isfile (int fd) { return fd >= 3; }

/* the screen: OS_WriteN for runs of text, OS_NewLine for each \n */
static void put_text (const char *s, size_t n)
{
  size_t i = 0;
  while (i < n)
    {
      size_t j = i;
      unsigned long r[10] = { 0 };
      while (j < n && s[j] != '\n') j++;
      if (j > i) { r[0] = (unsigned long) (s + i); r[1] = j - i; swi_x (OS_WriteN, r); }
      if (j < n) { unsigned long q[10] = { 0 }; swi_x (OS_NewLine, q); j++; }
      i = j;
    }
}

ssize_t write (int fd, const void *buf, size_t n)
{
  unsigned long r[10] = { 0 };
  if (fd == 1 || fd == 2) { put_text ((const char *) buf, n); return (ssize_t) n; }
  if (!isfile (fd)) { errno = EBADF; return -1; }
  r[0] = 2; r[1] = (unsigned long) fdhandle (fd); r[2] = (unsigned long) buf; r[3] = n;
  if (swi_x (OS_GBPB, r)) { errno = EIO; return -1; }
  return (ssize_t) (n - r[3]);
}

ssize_t read (int fd, void *buf, size_t n)
{
  unsigned long r[10] = { 0 };
  if (fd == 0)						/* the keyboard: one line, echoed, with backspace; Escape ends the input */
    {
      char *p = (char *) buf;
      size_t k = 0;
      while (k < n)
	{
	  unsigned long q[10] = { 0 };
	  int c;
	  if (swi_x (OS_ReadC, q)) { errno = EIO; return -1; }
	  if (q[1] == 1) return (ssize_t) k;		/* Escape */
	  c = (int) (q[0] & 255);
	  if (c == 13 || c == 10) { unsigned long z[10] = { 0 }; swi_x (OS_NewLine, z); p[k++] = '\n'; break; }
	  if ((c == 8 || c == 127) && k > 0) { unsigned long z[10] = { 127 }; swi_x (OS_WriteC, z); k--; continue; }
	  if (c >= 32 && c != 127) { unsigned long z[10] = { (unsigned long) c }; swi_x (OS_WriteC, z); p[k++] = (char) c; }
	}
      return (ssize_t) k;
    }
  if (!isfile (fd)) { errno = EBADF; return -1; }
  r[0] = 4; r[1] = (unsigned long) fdhandle (fd); r[2] = (unsigned long) buf; r[3] = n;
  if (swi_x (OS_GBPB, r)) { errno = EIO; return -1; }
  return (ssize_t) (n - r[3]);				/* r3 = the bytes that were not transferred: 0 bytes read is the end of the file */
}

int open (const char *path, int flags, ...)
{
  unsigned long r[10] = { 0 };
  int acc = flags & O_ACCMODE;
  r[1] = (unsigned long) path;
  r[0] = acc == O_RDONLY ? 0x40 : (acc == O_WRONLY && (flags & O_TRUNC)) ? 0x80 : 0xC0;
  if (acc != O_RDONLY && (flags & O_CREAT) == 0 && r[0] == 0x80) r[0] = 0xC0;		/* no create without O_CREAT: open for update (the file must exist) */
  if (swi_x (OS_Find, r)) { errno = EIO; return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  if (flags & O_APPEND)
    {
      unsigned long q[10] = { 2, r[0] };
      swi_x (OS_Args, q);				/* OS_Args 2: the extent in r2 */
      q[2] = q[2]; q[0] = 1; q[1] = r[0];
      swi_x (OS_Args, q);				/* OS_Args 1: set the pointer */
    }
  return (int) r[0] + 3;
}

int close (int fd)
{
  unsigned long r[10] = { 0 };
  if (!isfile (fd)) return 0;
  r[0] = 0; r[1] = (unsigned long) fdhandle (fd);
  if (swi_x (OS_Find, r)) { errno = EIO; return -1; }
  return 0;
}

off_t lseek (int fd, off_t off, int whence)
{
  unsigned long r[10] = { 0 };
  long pos = off;
  if (!isfile (fd)) { errno = ESPIPE; return -1; }
  if (whence == SEEK_CUR) { r[0] = 0; r[1] = (unsigned long) fdhandle (fd); if (swi_x (OS_Args, r)) { errno = EIO; return -1; } pos = (long) r[2] + off; }
  else if (whence == SEEK_END) { r[0] = 2; r[1] = (unsigned long) fdhandle (fd); if (swi_x (OS_Args, r)) { errno = EIO; return -1; } pos = (long) r[2] + off; }
  { unsigned long q[10] = { 1, (unsigned long) fdhandle (fd), (unsigned long) pos }; if (swi_x (OS_Args, q)) { errno = EIO; return -1; } }
  return (off_t) pos;
}

int fstat (int fd, struct stat *st)
{
  unsigned long r[10] = { 0 };
  char *p = (char *) st;
  size_t i;
  for (i = 0; i < sizeof *st; i++) p[i] = 0;
  if (!isfile (fd)) { st->st_mode = S_IFCHR | 0600; return 0; }
  r[0] = 2; r[1] = (unsigned long) fdhandle (fd);
  if (swi_x (OS_Args, r)) { errno = EIO; return -1; }
  st->st_mode = S_IFREG | 0644;
  st->st_size = (off_t) r[2];
  return 0;
}

int isatty (int fd) { return fd >= 0 && fd < 3; }

void *sbrk (ptrdiff_t incr)
{
  static char *cur;
  char *old;
  if (!cur) cur = __heap_start;
  old = cur;
  if (incr > 0 && (size_t) incr > (size_t) (__riscos_heap_end - cur)) { errno = ENOMEM; return (void *) -1; }
  if (incr < 0 && (size_t) -incr > (size_t) (cur - __heap_start)) { errno = ENOMEM; return (void *) -1; }
  cur += incr;
  return old;
}

void _exit (int code)
{
  static const struct { unsigned num; char msg[20]; } done = { 0, "Program finished" };
  unsigned long r[10] = { 0 };
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
  unsigned long r[10] = { 6, (unsigned long) path };
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }					/* r0 = the type of the object that was deleted: 0 = there was none */
  return 0;
}

int rename (const char *from, const char *to)
{
  unsigned long r[10] = { 25, (unsigned long) from, (unsigned long) to };
  if (swi_x (OS_FSControl, r)) { errno = os_errno_of (r); return -1; }
  return 0;
}

int mkdir (const char *path, mode_t mode)
{
  unsigned long r[10] = { 8, (unsigned long) path, 0, 0, 0 };
  (void) mode;
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  return 0;
}

int stat (const char *path, struct stat *st)
{
  unsigned long r[10] = { 17, (unsigned long) path };
  char *p = (char *) st;
  size_t i;
  for (i = 0; i < sizeof *st; i++) p[i] = 0;
  if (swi_x (OS_File, r)) { errno = os_errno_of (r); return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  if (r[0] == 2) st->st_mode = S_IFDIR | 0755;
  else st->st_mode = S_IFREG | ((r[5] & 1) ? 0444 : 0) | ((r[5] & 2) ? 0222 : 0);
  st->st_size = (off_t) (r[4] & 0xFFFFFFFF);
  st->st_nlink = 1;
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
char *getenv (const char *name)
{
  static char buf[256];
  unsigned long r[10] = { (unsigned long) name, (unsigned long) buf, sizeof buf - 1, 0, 3 };
  if (swi_x (OS_ReadVarVal, r) || r[2] >= sizeof buf) return 0;
  buf[r[2]] = 0;
  return buf;
}
int system (const char *cmd)
{
  unsigned long r[10] = { (unsigned long) cmd };
  if (!cmd) return 1;
  return swi_x (OS_CLI, r) ? -1 : 0;
}
