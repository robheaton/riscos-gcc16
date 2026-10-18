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
#include <sys/stat.h>
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
  char buf[64];
  unsigned long r[10] = { (unsigned long) "Sys$ReturnCode", (unsigned long) buf, sizeof buf - 1, 0, 0 };
  if (swi_x (OS_ReadVarVal, r)) return 0;
  if ((r[4] & 0xFF) == 1)				/* a number: the value is a word in the buffer (a program's OS_Exit sets it so) */
    {
      int v;
      memcpy (&v, buf, sizeof v);
      return v;
    }
  if (r[2] >= sizeof buf) return 0;
  buf[r[2]] = 0;					/* a string */
  return atoi (buf);
}

/* A program that RISC OS starts sees system variables, not this program's environ: before a command is run the variables that setenv and putenv made are set as system variables (literal strings), as UnixLib
   does for exec, and removed again when the command has ended (a variable that was set before is put back).  They must not stay: the next program started from the command line would find GCC's COMPILER_PATH
   and COLLECT_GCC_OPTIONS and take them for its own. */
extern char **environ;
#define MAXEXPORT 64
static struct { char name[96]; char *old; } exported[MAXEXPORT];
static int nexported;
static char *old_rclimit;					/* Sys$RCLimit before the command: RISC OS turns a return code at or above it into an error, which a command line program that returns a status would trigger */
static int rclimit_set;

static char *sysvar (const char *name)			/* the value of a system variable (malloc'd), or NULL */
{
  char buf[1024];
  unsigned long r[10] = { (unsigned long) name, (unsigned long) buf, sizeof buf - 1, 0, 3 };
  if (swi_x (OS_ReadVarVal, r) || r[2] >= sizeof buf) return 0;
  buf[r[2]] = 0;
  return strdup (buf);
}
static void setsysvar (const char *name, const char *value)	/* value NULL: remove it */
{
  unsigned long r[10] = { (unsigned long) name, (unsigned long) (value ? value : ""), value ? strlen (value) : (unsigned long) -1, 0, 4, 0, 0, 0, 0, 0 };
  swi_x (0x24, r);					/* OS_SetVarVal, type 4: a literal string */
}
static void setsysnum (const char *name, unsigned long v)
{
  unsigned int word = (unsigned int) v;
  unsigned long r[10] = { (unsigned long) name, (unsigned long) &word, 4, 0, 1, 0, 0, 0, 0, 0 };
  swi_x (0x24, r);					/* OS_SetVarVal, type 1: a number (R1 points at the word) */
}
static void export_environ (void)
{
  char **e;
  nexported = 0;
  old_rclimit = sysvar ("Sys$RCLimit");
  setsysnum ("Sys$RCLimit", 0x7FFFFFFFUL);
  rclimit_set = 1;
  if (!environ) return;
  for (e = environ; *e && nexported < MAXEXPORT; e++)
    {
      const char *eq = strchr (*e, '=');
      size_t n;
      if (!eq) continue;
      n = (size_t) (eq - *e);
      if (n == 0 || n >= sizeof exported[0].name) continue;
      memcpy (exported[nexported].name, *e, n); exported[nexported].name[n] = 0;
      exported[nexported].old = sysvar (exported[nexported].name);
      setsysvar (exported[nexported].name, eq + 1);
      nexported++;
    }
}
static void unexport_environ (void)
{
  int i;
  if (rclimit_set)
    {
      if (old_rclimit) setsysnum ("Sys$RCLimit", (unsigned long) atol (old_rclimit));
      else setsysvar ("Sys$RCLimit", 0);
      free (old_rclimit);
      old_rclimit = 0;
      rclimit_set = 0;
    }
  for (i = 0; i < nexported; i++)
    {
      setsysvar (exported[i].name, exported[i].old);
      free (exported[i].old);
    }
  nexported = 0;
}

/* run the command; the POSIX wait status of it: (result & 255) << 8, or 127 << 8 when RISC OS could not run it */
extern int __ul64_run_swap (const char *cmd);
extern char __ul64_run_error[];
/* A command that starts with a Unix path (./sh, /bin/tool, ../x) has the path made a RISC OS one, as UnixLib does for system (); a command of the operating system (Spool, Copy ...) is left alone. */
static char *translate_command (const char *cmd)
{
  const char *e = cmd;
  char b[1024], *r;
  const char *t;
  size_t n;
  if (*cmd == '"' || *cmd == '*') return 0;
  while (*e && *e != ' ') e++;
  n = (size_t) (e - cmd);
  if (!memchr (cmd, '/', n) || n >= sizeof b - 1) return 0;
  memcpy (b, cmd, n); b[n] = 0;
  { char tr[1024]; t = __riscos_path (b, tr, 0); if (!t) return 0; r = malloc (strlen (t) + strlen (e) + 1); if (!r) return 0; strcpy (r, t); strcat (r, e); }
  return r;
}

static int run_translated (const char *cmd);
static int run (const char *cmd)
{
  char *t = translate_command (cmd);
  int st;
  if (!t) return run_translated (cmd);
  st = run_translated (t);
  free (t);
  return st;
}
static int run_translated (const char *cmd)
{
  unsigned long r[10] = { (unsigned long) cmd };
  export_environ ();
  setsysnum ("Sys$ReturnCode", 0);			/* so that the result of an earlier program is not taken for this one's (removing the variable with a length of -1 did not always work: a command of the operating system does not set it) */
  {
    int sr = __ul64_run_swap (cmd);
    if (sr == 2) { unexport_environ (); fprintf (stderr, "%s\n", __ul64_run_error); return 127 << 8; }	/* the command could not be started */
    if (sr == 0) { unexport_environ (); return (return_code () & 255) << 8; }
  }	/* UnixEnv$swap=1: the parent is saved and comes back (runswap.c) */
  if (swi_x (OS_CLI, r)) { unexport_environ (); fprintf (stderr, "%s\n", (const char *) r[0] + 4); return 127 << 8; }
  unexport_environ ();
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
  for (i = 1; argv && argv[i]; i++) n += 2 * strlen (argv[i]) + 3;
  cmd = (char *) malloc (n);
  if (!cmd) return 0;
  p = cmd + sprintf (cmd, "%s", prog);
  for (i = 1; argv && argv[i]; i++)
    {
      const char *a = argv[i];
      int quote = a[0] == 0 || strpbrk (a, " \t\"") != 0;
      *p++ = ' ';
      if (quote)							/* the program's start-up reads "" inside quotes as one quote */
	{
	  *p++ = '"';
	  for (; *a; a++) { if (*a == '"') *p++ = '"'; *p++ = *a; }
	  *p++ = '"';
	}
      else p += sprintf (p, "%s", a);
    }
  *p = 0;
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
  status = run_translated (cmd);
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
static struct { FILE *f; char *cmd; char file[64]; int write; int status; int sdk; } pipes[NPOPEN];

/* Does OS_CLI take "command { > file }" (output redirection)?  RISC OS 5 does, RISC OS Pyromaniac does not: tried once, with a command whose output is known. */
static int can_redirect (void)
{
  static int known = -1;
  if (known < 0)
    {
      char name[64];
      unsigned long r[10];
      char cmd[128];
      known = 0;
      if (getenv ("Wimp$ScrapDir")) snprintf (name, sizeof name, "<Wimp$ScrapDir>.redir%u", (unsigned) getpid ());
      else snprintf (name, sizeof name, "redir%u", (unsigned) getpid ());
      snprintf (cmd, sizeof cmd, "Set Sh$Probe 1 { > %s }", name);	/* with the redirection the (empty) file is made; without it nothing is written or shown */
      r[0] = (unsigned long) cmd; r[1] = r[2] = r[3] = r[4] = 0;
      if (!swi_x (OS_CLI, r))
	{
	  unsigned long f[10] = { 0x40, (unsigned long) name };		/* OS_Find: does the file exist? */
	  if (!swi_x (0x0D, f) && f[0]) { unsigned long c[10] = { 0, f[0] }; swi_x (0x0D, c); known = 1; }
	  { unsigned long d[10] = { 6, (unsigned long) name }; swi_x (0x08, d); }	/* OS_File 6: delete */
	}
    }
  return known;
}

/* Is the first word of the command a program file (one of the SDK, which reads Sh$Stdin and Sh$Stdout)?  Then the scrap file is given that way and nothing is shown on the screen. */
static int is_program (const char *cmd)
{
  char first[512];
  const char *e = cmd;
  struct stat st;
  size_t n;
  while (*e && *e != ' ') e++;
  n = (size_t) (e - cmd);
  if (n == 0 || n >= sizeof first || cmd[0] == '"') return 0;
  memcpy (first, cmd, n); first[n] = 0;
  return stat (first, &st) == 0 && S_ISREG (st.st_mode);
}

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
  pipes[i].sdk = is_program (cmd);
  if (!write && pipes[i].sdk)
    {
      char ros[1100], tr[1100];
      const char *rp = __riscos_path (pipes[i].file, tr, 0);
      snprintf (ros, sizeof ros, "%s", rp ? rp : pipes[i].file);
      { FILE *t = fopen (pipes[i].file, "w"); if (t) fclose (t); }
      setenv ("Sh$Stdout", ros, 1);
      pipes[i].status = run (cmd);
      unsetenv ("Sh$Stdout");
    }
  else if (!write)
    {
      if (can_redirect ())						/* the operating system redirects the output of the command itself: a Spool file that the program that runs this one has open is left alone */
	{
	  snprintf (line, sizeof line, "%s { > %s }", cmd, pipes[i].file);
	  pipes[i].status = run (line);
	}
      else
	{
	  snprintf (line, sizeof line, "Spool %s", pipes[i].file);
	  run (line);
	  pipes[i].status = run (cmd);
	  run ("Spool");						/* (this ends any Spool file that was open: there is only one) */
	}
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
      if (pipes[i].sdk)
	{
	  char ros[1100], tr[1100];
	  const char *rp = __riscos_path (pipes[i].file, tr, 0);
	  snprintf (ros, sizeof ros, "%s", rp ? rp : pipes[i].file);
	  setenv ("Sh$Stdin", ros, 1);
	  status = run (pipes[i].cmd);
	  unsetenv ("Sh$Stdin");
	}
      else
	{
	  snprintf (line, sizeof line, "Exec %s", pipes[i].file);
	  run (line);
	  status = run (pipes[i].cmd);
	  run ("Exec");
	}
    }
  else status = pipes[i].status;
  remove (pipes[i].file);
  free (pipes[i].cmd);
  pipes[i].f = 0;
  return status;
}
