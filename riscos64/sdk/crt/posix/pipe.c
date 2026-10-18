/* pipe.c - pipe () for a system where a program runs to its end before the one that started it goes on: the pipe is a scrap file.  fds[1] is a file open for writing; fds[0] is a descriptor of the range
   500-515 that opens the file for reading on the first read (by then the writer has been closed, or the read finds the end), and read, close and fstat know about (riscos_syscalls.c calls the __ul64_pipe_
   functions through weak references).  It is enough for the way that programs use a pipe with a vfork child (write a number to say that exec failed, read it back in the parent: xargs, find), not for two
   programs that run at the same time (nothing does here).  The SDK of RISC OS 64. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PIPEFD_BASE 500
#define NPIPE 16
static struct { int used; char *path; int wfd; int rfd; } pipes[NPIPE];

int pipe (int fds[2])
{
  int i;
  static unsigned seq;
  char name[128];
  for (i = 0; i < NPIPE; i++) if (!pipes[i].used) break;
  if (i == NPIPE) { errno = EMFILE; return -1; }
  snprintf (name, sizeof name, "/tmp/pipe%u_%u", (unsigned) getpid (), ++seq);
  pipes[i].wfd = open (name, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (pipes[i].wfd < 0) return -1;
  pipes[i].path = strdup (name); pipes[i].rfd = -1; pipes[i].used = 1;
  fds[0] = PIPEFD_BASE + i; fds[1] = pipes[i].wfd;
  return 0;
}

/* In a vfork child (the parent and the child are one process here) a close of a pipe end only closes the child's copy: it is ignored, so that the parent can still use the pipe */
extern int __ul64_vfork_depth_get (void);
int __ul64_pipe_defer_close (int fd)
{
  int i;
  if (__ul64_vfork_depth_get () <= 0) return 0;
  for (i = 0; i < NPIPE; i++) if (pipes[i].used && (fd == PIPEFD_BASE + i || fd == pipes[i].wfd)) return 1;
  return 0;
}

int __ul64_is_pipe (int fd) { return fd >= PIPEFD_BASE && fd < PIPEFD_BASE + NPIPE && pipes[fd - PIPEFD_BASE].used; }

ssize_t __ul64_pipe_read (int fd, void *buf, size_t n)
{
  int i = fd - PIPEFD_BASE;
  if (pipes[i].rfd < 0)
    {
      pipes[i].rfd = open (pipes[i].path, O_RDONLY);
      if (pipes[i].rfd < 0) return 0;			/* the writer still has it open: nothing to read yet = the end */
    }
  return read (pipes[i].rfd, buf, n);
}

int __ul64_pipe_close (int fd)
{
  int i = fd - PIPEFD_BASE;
  if (pipes[i].rfd >= 0) close (pipes[i].rfd);
  remove (pipes[i].path);
  free (pipes[i].path);
  pipes[i].used = 0;
  return 0;
}

int __ul64_pipe_fstat (int fd, struct stat *st)
{
  (void) fd;
  memset (st, 0, sizeof *st);
  st->st_mode = S_IFIFO | 0600;
  return 0;
}
