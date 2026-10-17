/* process.c - running other programs, for RISC OS where a program runs to its end before the one that started it goes on (there is no fork): system, popen/pclose, posix_spawn/posix_spawnp with
   waitpid/wait, exec* (run the program, then end with its result) and fork/vfork (ENOSYS).  A command is run with OS_CLI; the result is the system variable Sys$ReturnCode that RISC OS sets
   when a program ends.  A "process" is therefore finished when posix_spawn returns: its pid is a number to hand to waitpid.  The wait status is the POSIX one (the result in bits 8-15).
   The SDK of RISC OS 64 (Unix file names in the command are not translated: a command is a RISC OS command; UnixLib 64's system () is the same).  */
#include <errno.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern void _exit (int);
extern const char *__riscos_path (const char *name, char *buf, int create_dir);
#define OS_CLI 0x05
#define OS_ReadVarVal 0x23

static int return_code (void)				/* Sys$ReturnCode: the result of the program that ended last (0 if the variable is not there) */
{
  char buf[32];
  unsigned long r[10] = { (unsigned long) "Sys$ReturnCode", (unsigned long) buf, sizeof buf - 1, 0, 3 };
  if (swi_x (OS_ReadVarVal, r) || r[2] >= sizeof buf) return 0;
  buf[r[2]] = 0;
  return atoi (buf);
}

/* run the command; the POSIX wait status of it: (result & 255) << 8, or 127 << 8 when RISC OS could not run it */
static int run (const char *cmd)
{
  unsigned long r[10] = { (unsigned long) cmd };
  unsigned long clear[10] = { (unsigned long) "Sys$ReturnCode" , 0, (unsigned long) -1, 0, 0 };
  swi_x (0x24, clear);					/* OS_SetVarVal with a length of -1: remove the variable, so that an old result is not taken for this one */
  if (swi_x (OS_CLI, r)) return 127 << 8;
  return (return_code () & 255) << 8;
}

int system (const char *cmd)
{
  if (!cmd) return 1;					/* a command processor is there */
  return run (cmd);
}

/* the command line of an argv: the arguments with a space or a quote are put in double quotes (a double quote inside is doubled? no: RISC OS has \" in GSTrans strings, which OS_CLI does not read; a quote in an argument is left) */
static char *command_line (const char *prog, char *const argv[])
{
  size_t n = strlen (prog) + 2;
  size_t i;
  char *cmd, *p;
  for (i = 1; argv && argv[i]; i++) n += strlen (argv[i]) + 3;
  cmd = (char *) malloc (n);
  if (!cmd) return 0;
  p = cmd + sprintf (cmd, "%s", prog);
  for (i = 1; argv && argv[i]; i++)
    {
      int quote = argv[i][0] == 0 || strpbrk (argv[i], " \t") != 0;
      p += sprintf (p, quote ? " \"%s\"" : " %s", argv[i]);
    }
  return cmd;
}

/* the "children": the status of each program that posix_spawn has run, until waitpid asks for it */
#define NCHILD 64
static struct { pid_t pid; int status; } children[NCHILD];
static pid_t next_pid = 100;

int posix_spawn (pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
  char b[1024], *cmd;
  const char *p = __riscos_path (path, b, 0);
  int i, status;
  (void) fa; (void) attr; (void) envp;
  if (!p) return errno;
  cmd = command_line (p, argv);
  if (!cmd) return ENOMEM;
  status = run (cmd);
  free (cmd);
  if (status == (127 << 8)) return ENOENT;
  for (i = 0; i < NCHILD; i++) if (!children[i].pid) break;
  if (i == NCHILD) return EAGAIN;
  children[i].pid = ++next_pid; children[i].status = status;
  if (pid) *pid = children[i].pid;
  return 0;
}

int posix_spawnp (pid_t *pid, const char *file, const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
  return posix_spawn (pid, file, fa, attr, argv, envp);			/* (RISC OS looks along Run$Path itself) */
}

pid_t waitpid (pid_t pid, int *status, int options)
{
  int i;
  (void) options;
  for (i = 0; i < NCHILD; i++)
    if (children[i].pid && (pid <= 0 || children[i].pid == pid))
      {
	pid_t got = children[i].pid;
	if (status) *status = children[i].status;
	children[i].pid = 0;
	return got;
      }
  errno = ECHILD;
  return -1;
}
pid_t wait (int *status) { return waitpid (-1, status, 0); }

int execv (const char *path, char *const argv[])
{
  char b[1024], *cmd;
  const char *p = __riscos_path (path, b, 0);
  int status;
  if (!p) return -1;
  cmd = command_line (p, argv);
  if (!cmd) { errno = ENOMEM; return -1; }
  status = run (cmd);
  if (status == (127 << 8)) { free (cmd); errno = ENOENT; return -1; }
  _exit (status >> 8);						/* exec does not return: the program that was started has run, and this one ends with its result */
}
int execvp (const char *file, char *const argv[]) { return execv (file, argv); }
int execve (const char *path, char *const argv[], char *const envp[]) { (void) envp; return execv (path, argv); }
int execl (const char *path, const char *arg0, ...)
{
  char *argv[64];
  va_list ap;
  int n = 0;
  va_start (ap, arg0);
  argv[n++] = (char *) arg0;
  while (n < 63 && (argv[n] = va_arg (ap, char *)) != 0) n++;
  va_end (ap);
  argv[n] = 0;
  return execv (path, argv);
}
int execlp (const char *file, const char *arg0, ...)
{
  char *argv[64];
  va_list ap;
  int n = 0;
  va_start (ap, arg0);
  argv[n++] = (char *) arg0;
  while (n < 63 && (argv[n] = va_arg (ap, char *)) != 0) n++;
  va_end (ap);
  argv[n] = 0;
  return execv (file, argv);
}

pid_t fork (void) { errno = ENOSYS; return -1; }
pid_t vfork (void) { errno = ENOSYS; return -1; }

/* popen: through a file.  "r": the command runs now with its screen output spooled (*Spool) to a scrap file, which is what the stream reads (OS_CLI does not have the { > file } of the command line); "w": the stream writes
   a scrap file, and pclose runs the command with the file as its keyboard input (*Exec).  A program that the command starts replaces this one (see the comment at the top), so this works for the commands of the
   operating system.  */
#define NPOPEN 16
static struct { FILE *f; char *cmd; char file[64]; int write; int status; } pipes[NPOPEN];

FILE *popen (const char *cmd, const char *type)
{
  int i, write = type && type[0] == 'w';
  char line[1500];
  static unsigned seq;
  if (!cmd || !type || (type[0] != 'r' && type[0] != 'w')) { errno = EINVAL; return 0; }
  for (i = 0; i < NPOPEN; i++) if (!pipes[i].f) break;
  if (i == NPOPEN) { errno = EMFILE; return 0; }
  /* a scrap file: in <Wimp$ScrapDir> (the desktop makes it), else in the current directory */
  if (getenv ("Wimp$ScrapDir")) snprintf (pipes[i].file, sizeof pipes[i].file, "<Wimp$ScrapDir>.pipe%u", ++seq);
  else snprintf (pipes[i].file, sizeof pipes[i].file, "pipe%u", ++seq);
  pipes[i].write = write;
  if (!write)
    {
      snprintf (line, sizeof line, "Spool %s", pipes[i].file);
      run (line);
      pipes[i].status = run (cmd);
      run ("Spool");
    }
  pipes[i].f = fopen (pipes[i].file, write ? "w" : "r");
  if (!pipes[i].f) return 0;
  pipes[i].cmd = strdup (cmd);
  return pipes[i].f;
}

int pclose (FILE *f)
{
  int i, status = 0;
  char line[1500];
  for (i = 0; i < NPOPEN; i++) if (pipes[i].f == f) break;
  if (i == NPOPEN) { errno = ECHILD; return -1; }
  fclose (f);
  if (pipes[i].write)
    {
      snprintf (line, sizeof line, "Exec %s", pipes[i].file);
      run (line);
      status = run (pipes[i].cmd);
      run ("Exec");
    }
  else status = pipes[i].status;
  remove (pipes[i].file);
  free (pipes[i].cmd);
  pipes[i].f = 0;
  return status;
}
