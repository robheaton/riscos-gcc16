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

/* A program that RISC OS starts sees system variables, not this program's environ: before a command is run the variables that setenv and putenv made are set as system variables (literal strings), as UnixLib
   does for exec.  They stay set afterwards. */
extern char **environ;
static void export_environ (void)
{
  char **e;
  if (!environ) return;
  for (e = environ; *e; e++)
    {
      const char *eq = strchr (*e, '=');
      char name[256];
      size_t n;
      unsigned long r[10];
      if (!eq) continue;
      n = (size_t) (eq - *e);
      if (n == 0 || n >= sizeof name) continue;
      memcpy (name, *e, n); name[n] = 0;
      r[0] = (unsigned long) name; r[1] = (unsigned long) (eq + 1); r[2] = strlen (eq + 1); r[3] = 0; r[4] = 4; r[5] = r[6] = r[7] = r[8] = r[9] = 0;
      swi_x (0x24, r);					/* OS_SetVarVal, type 4: a literal string */
    }
}

/* run the command; the POSIX wait status of it: (result & 255) << 8, or 127 << 8 when RISC OS could not run it */
extern int __ul64_run_swap (const char *cmd);
static int run (const char *cmd)
{
  unsigned long r[10] = { (unsigned long) cmd };
  unsigned long clear[10] = { (unsigned long) "Sys$ReturnCode" , 0, (unsigned long) -1, 0, 0 };
  export_environ ();
  swi_x (0x24, clear);					/* OS_SetVarVal with a length of -1: remove the variable, so that an old result is not taken for this one */
  if (__ul64_run_swap (cmd) == 0) return (return_code () & 255) << 8;	/* UnixEnv$swap=1: the parent is saved and comes back (runswap.c) */
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

extern int __ul64_swap_enabled (void);
extern void __ul64_vfork_return (pid_t pid);		/* process.c asm: back to the vfork that is active, returning pid */
int __ul64_vfork_depth;
#define vfork_depth __ul64_vfork_depth					/* vfork calls waiting for their "child" to exec or _exit */

/* the "child" of a vfork ended (exec has run the program, or _exit): record the status and let vfork return again, in the parent, with the pid of the child */
static void vfork_finish (int status)
{
  int i;
  pid_t pid = ++next_pid;
  for (i = 0; i < NCHILD; i++) if (!children[i].pid) break;
  if (i < NCHILD) { children[i].pid = pid; children[i].status = status; }
  vfork_depth--;
  __ul64_vfork_return (pid);
}
void __ul64_vfork_exit (int code)			/* called by _exit: does not return while a vfork child is running */
{
  if (vfork_depth > 0) vfork_finish ((code & 255) << 8);
}

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
  if (vfork_depth > 0) { free (cmd); vfork_finish (status); }	/* the parent goes on after its vfork */
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

extern void __ul64_nosys (const char *);
pid_t fork (void) { __ul64_nosys ("fork"); return -1; }

/* vfork: returns 0 (the "child", which may only call exec*, posix_spawn, _exit), and once more, in the parent, with the child's pid when the child has exec'd or ended.  The registers the caller keeps (x19-x30,
   sp) are saved here; __ul64_vfork_return puts them back.  Needs the swap run (UnixEnv$swap=1), else -1 and ENOSYS (OS_CLI would replace this program).  The state is a stack, so vfork can nest. */
#define VFORK_MAX 8
unsigned long __ul64_vfork_regs[VFORK_MAX][14];
__asm__ (
  ".text\n.balign 4\n.global vfork\n.type vfork, %function\n"
  "vfork:\n"
  "	stp	x29, x30, [sp, #-32]!\n"
  "	bl	__ul64_vfork_enter\n"			/* x0 = the buffer for this call, or 0 */
  "	ldp	x29, x30, [sp], #32\n"
  "	cbz	x0, 1f\n"
  "	stp	x19, x20, [x0]\n	stp	x21, x22, [x0, #16]\n	stp	x23, x24, [x0, #32]\n	stp	x25, x26, [x0, #48]\n"
  "	stp	x27, x28, [x0, #64]\n	stp	x29, x30, [x0, #80]\n"
  "	mov	x1, sp\n	str	x1, [x0, #96]\n"
  "	mov	x0, #0\n	ret\n"
  "1:	mov	x0, #-1\n	ret\n"
  ".size vfork, .-vfork\n"
  ".global __ul64_vfork_return\n.type __ul64_vfork_return, %function\n"
  "__ul64_vfork_return:\n"			/* x0 = the pid */
  "	adrp	x9, __ul64_vfork_depth\n	ldr	w9, [x9, #:lo12:__ul64_vfork_depth]\n"	/* depth after the decrement = the index of the buffer */
  "	adrp	x10, __ul64_vfork_regs\n	add	x10, x10, #:lo12:__ul64_vfork_regs\n"
  "	mov	x11, #112\n	madd	x10, x9, x11, x10\n"
  "	ldp	x19, x20, [x10]\n	ldp	x21, x22, [x10, #16]\n	ldp	x23, x24, [x10, #32]\n	ldp	x25, x26, [x10, #48]\n"
  "	ldp	x27, x28, [x10, #64]\n	ldp	x29, x30, [x10, #80]\n"
  "	ldr	x1, [x10, #96]\n	mov	sp, x1\n"
  "	ret\n"
  ".size __ul64_vfork_return, .-__ul64_vfork_return\n");
unsigned long *__ul64_vfork_enter (void)
{
  if (!__ul64_swap_enabled () || vfork_depth >= VFORK_MAX) { errno = ENOSYS; return 0; }
  return __ul64_vfork_regs[vfork_depth++];
}
int __ul64_vfork_depth_get (void) { return vfork_depth; }

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
