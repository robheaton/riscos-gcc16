/* UnixLib process initialisation and finalisation.
   Copyright (c) 2002-2015 UnixLib Developers.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <swis.h>
#include <termios.h>
#include <time.h>
#include <fpu_control.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <unixlib/local.h>
#include <internal/fd.h>
#include <internal/local.h>
#include <internal/os.h>
#include <internal/unix.h>
#include <internal/sigstate.h>
#include <internal/swiparams.h>
#include <internal/machine-gmon.h>
#include <pthread.h>

/*#define DEBUG 1*/
#ifdef DEBUG
#  include <sys/debug.h>
#endif

static void initialise_unix_io (void);
static void check_fd_redirection (const char *filename,
				  unsigned int fd_to_replace);
static int get_fd_redirection (const char *redir);
static void get_io_redir (const char *cli);
static int verify_redirection (const char *redirection);
static const char *find_redirection_type (const char *command_line,
					  char redirection_type);
static void convert_command_line (struct proc *process, const char *cli,
				  int cli_size);

static void __badr (void) __attribute__ ((__noreturn__));

/* Only called externally from here - see comment below */
extern void __dsp_exit(void);

static struct proc ___u;
struct proc *__u = &___u;	/* current process */

char *program_invocation_name, *program_invocation_short_name;

static void
__badr (void)
{
  __unixlib_fatal ("Bad redirection");
}

/* strtoul to avoid pulling in global verion for small executables.
   This version is limited to base 10, the first character is known to be
   a digit and has less error checking.  */
unsigned int
__decstrtoui (const char *nptr, const char **end)
{
  unsigned int result = 0;

  do
    result = result * 10 + (*nptr++ - '0');
  while (isdigit (*nptr));
  if (end)
    *end = nptr;

  return result;
}

/* strtoul to avoid pulling in global verion for small executables.
   This version is limited to exactly 2 hexadecimal characters.  */
static char
__hexstrtochar (const char *nptr)
{
  char result;

  if (*nptr >= 'A')
    result = (*nptr - 'A' + 10) << 4;
  else
    result = (*nptr - '0') << 4;

  nptr++;

  if (*nptr >= 'A')
    result += (*nptr - 'A' + 10);
  else
    result += (*nptr - '0');

  return result;
}

/* Free any remaining memory and file descriptors associated with a process */
void
__free_process (struct __sul_process *process)
{
  struct __sul_process *sulproc = __ul_global.sulproc;

  /* Close all file descriptors.  */
  if (process->file_descriptors)
    {
      unsigned int i;
      for (i = 0; i < process->maxfd; i++)
	{
	  struct __unixlib_fd *file_desc;

	  file_desc = (struct __unixlib_fd *)((char *)(process->file_descriptors) + i * process->fdsize);
	  if (file_desc->devicehandle)
	    __close (file_desc);
	}

      sulproc->sul_free (sulproc->pid, process->file_descriptors);
      process->file_descriptors = NULL;
    }

  if (process->environ)
    {
      sulproc->sul_free (sulproc->pid, process->environ);
      process->environ = NULL;
    }

  if (process->console)
    {
      if (__atomic_modify (&(process->console->refcount), -1) == 0)
	sulproc->sul_free(sulproc->pid, sulproc->console);
      process->console = NULL;
    }

  if (process->rs423)
    {
      if (__atomic_modify (&(process->rs423->refcount), -1) == 0)
	sulproc->sul_free(sulproc->pid, process->rs423);
      process->rs423 = NULL;
    }

  /* Free all zombie children of this process */
  struct __sul_process *child, *next_child;
  for (child = process->children; child != NULL; child = next_child)
    {
      next_child = child->next_child;
      if (child->status.zombie)
	__free_process (child);
      else
	{
	  /* The child is still running, so we can't free it */
	  child->ppid = 1;
	  child->next_child = NULL;
	}
    }
  process->children = NULL;

  /* If this a process struct from a zombie child, then free it */
  if (process->status.zombie)
    sulproc->sul_free (sulproc->pid, process);
}

/* Initialise the UnixLib world.  Create a new process structure, initialise
   the UnixLib library and parse command line arguments.
   This function is called by __main () in sys/_syslib.s.

   No calls to 'brk', 'sbrk' or 'malloc' should occur before calling
   this function.  */
void
__unixinit (void)
{
  struct ul_global *gbl = &__ul_global;
  struct __sul_process *sulproc = gbl->sulproc;

#ifdef DEBUG
  debug_printf ("-- __unixinit: new process\n");
#endif

  /* Calls to 'brk', 'sbrk' or 'malloc' can occur once the call
     to '__resource_initialise' has completed.  */
  __resource_initialise (__u);

  /* Record the initial escape key status. If escape is disabled (as it
     might be if we are being run as an ANSI task from Nettle) then we
     want to ensure the tty driver does not reenable it. */
  SWI_OS_Byte (0xe5, 0, 0xff, &gbl->escape_disabled, NULL);

  /* Initialise the pthread system */
  __pthread_prog_init ();
  __unixlib_signal_initialise (__u);
  /* Initialise ctype tables to the C locale.  */
  __build_ctype_tables (&__locale_global, -2);
  /* Define and initialise the Unix I/O.  */
  initialise_unix_io ();
  __stdioinit ();

  /* When the DDEUtils module is loaded, we can support chdir() without the
     RISC OS CSD being changed. When not loaded, chdir() will work by changing
     CSD for all processes.

     IMPORTANT NOTE: because of bugs in DDEUtils' path processing we don't set
     DDEUtils_Prefix at the beginning of each process.
     Symptoms of these bugs are "ADFS::HardDisc4.$ is a directory" RISC OS
     error when Font_FindFont is done for a font not yet in the font cache.
     These problems are known to be solved in RISC OS Adjust 1.  */
  __u->dde_prefix = __get_dde_prefix ();
#if __UNIXLIB_SET_DDEPREFIX != 0
  if (__u->dde_prefix == NULL)
    (void) SWI_DDEUtils_Prefix ("@");
#endif

  /* Set up the environment */
  if (sulproc->environ)
    {
      /* We have inherited an environment from our parent */
      environ = sulproc->environ;
    }
  else
    {
      /* We have no parent, so populate our environment with a subset of the
	 RISC OS global environment. */
      char *call = NULL;
      int environlen = 10;
      int environentries = 0;

      environ = malloc (environlen * sizeof (char *));
      if (environ == NULL)
	__unixlib_fatal ("Cannot allocate memory for environ");

      do
	{
	  char *value = __getenv_from_os ("*", NULL, 0, &call);
	  if (value)
	    {
	      if (environentries >= environlen)
		{
		  environlen += 10;
		  environ = realloc (environ, environlen * sizeof (char *));
		  if (environ == NULL)
		    __unixlib_fatal ("Cannot allocate memory for environ");
		}
	      environ[environentries++] = value;
	    }
	}
      while (call);

      /* Terminate the list and shrink it to the exact size needed.  */
      environ = realloc (environ, (environentries + 1) * sizeof (char *));
      if (environ == NULL)
	__unixlib_fatal ("Cannot allocate memory for environ");
      environ[environentries] = NULL;
    }

  /* Get command line.  __ul_global.cli's pointing to the command line block
     returned by OS_GetEnv in __main ().  */
#ifdef DEBUG
  debug_printf ("-- __unixinit (getting cli) __ul_global.cli=%s\n", gbl->cli);
#endif

  /* Since the command line limit of RISC OS is only 255 characters,
     extra data can be passed through a DDEUtils buffer setup by the calling
     application.  If such command line information exists, then obtain
     it and append it to our existing command line.  */
  size_t arg_size;
  if (SWI_DDEUtils_GetCLSize (&arg_size) != NULL)
    arg_size = 0;

  size_t com_size = strlen (gbl->cli);
  size_t cli_size = com_size + arg_size;
  char *cli = malloc (cli_size + 2);
  if (cli != NULL)
    {
      memcpy (cli, gbl->cli, com_size);
      if (arg_size != 0)
	{
	  /* Append DDEUtils argument line.  */
	  cli[com_size] = ' ';
	  if (SWI_DDEUtils_GetCL (cli + com_size + 1) != NULL)
	    __unixlib_fatal ("Cannot get command line");
	  cli[com_size + 1 + arg_size] = '\0';
	}
      else
	cli[com_size] = '\0';
    }

  /* Set command line length to zero otherwise the next process will also get
     it.  */
  (void) SWI_DDEUtils_SetCLSize (0);

  if (cli == NULL)
    __unixlib_fatal ("Command line too long (not enough memory)");

  /* Find any environment variables that may influence parts of UnixLib's
     decision making.  */
  __runtime_features (cli);

#ifdef DEBUG
  debug_printf ("-- __unixinit: create argv: cli_size=%d\n", cli_size);
#endif
  /* Parse the command line, looking for I/O redirection.  */
  get_io_redir (cli);
  /* Convert the command line to a argv block.  */
  convert_command_line (__u, cli, cli_size);
  free (cli);

  /* Determine program_invocation_name and program_invocation_short_name.  */
  program_invocation_name = __u->argv[0];
  if (&__program_name)
    program_invocation_short_name = (char *)__program_name;
  else
    {
      /* argv[0] can be a RISC OS or Unix path.  */
      const int ropath = (__get_riscosify_control () & __RISCOSIFY_NO_PROCESS);
      char *argv0 = __u->argv[0];
      for (char *c = argv0; *c; ++c)
	{
	  if (ropath && (*c == ':' || *c == '.') || !ropath && *c == '/')
	    argv0 = c + 1;
	}
      program_invocation_short_name = argv0;
    }

  /* The libm code requires strict IEEE double precision arithmetic.
     The compiler generates code assuming that the AC bit is turned on.
     See fpu_control.h for further information.  */
  /* _FPU_SETCW (_FPU_IEEE); */
  _FPU_SETCW (_FPU_DEFAULT);

#ifdef DEBUG
  __debug ("__unixinit: process creation complete");
#endif
}

void
exit (int status)
{
  /* Execute atexit functions in the reverse order of their
     registration.
     Although this accesses globals, we don't want to disable threads
     whilst calling the functions. Anything calling atexit from an already
     registered function is on dodgy ground anyway. */
  __cxa_finalize (NULL);

  _Exit (status);
}

/* ISOC99: Terminate the program with `status' without calling any of
   the functions registered with `atexit'.  */
void
_Exit (int status)
{
  /* Close all open streams, write out buffered output data and
     delete tmpfile() temporary files.  */
  __stdioexit ();

  /* Only return codes between 0 and 127 are valid.  128 to 255 are
     reserved for the run-time library internals.  */
  status = status & 0x7f;

  _exit (__W_EXITCODE (status, 0));
}

#ifdef __ARM_EABI__
/* Give back the signal stack.  _syslib.s takes a one page stack for the signal handlers (and for the exit handler, the fatal error exit and the pthread
   context switcher) from ARMEABISupport at start-up and keeps only its top address in __ul_global.signalhandler_sp; SharedUnixLibrary frees the main stack of a process when it ends
   but not that one, so every process that ended left 4KB of the one range of address space ALL EABI stacks share (the "UnixLib stacks" dynamic area, 128MB with the OS clamp) in use
   until the process at the root of its family (the one without a parent) ended, and a long make starts thousands.  Called when the UnixLib environment handlers have been taken off
   again (nothing can run a signal handler any more), by the process that is the last user of the program image (__dynamic_area_refcount is zero after its own decrement: a vfork child
   shares the image, and with it the signal stack, with its parent until it execs), and never from the signal stack itself (the exit handler, a fatal error and a signal handler that
   ends the program all run on it: those leave the page, as before).  Nothing here can fail the exit: a SWI that fails leaves the page where it was.  */
void
__signalhandler_stack_free (void)
{
  struct ul_global *gbl = &__ul_global;
  char *top = (char *) gbl->signalhandler_sp;
  char *sp;
  void *stack = NULL;

  if (__dynamic_area_refcount != 0 || top == NULL || gbl->executing_signalhandler != 0)
    return;

  __asm__ volatile ("mov %0, sp" : "=r" (sp));
  if (sp >= top - 4096 && sp <= top)		/* the stack is one 4KB page of ARMEABISupport's */
    return;

  if (_swix (ARMEABISupport_StackOp, _INR(0,1)|_OUT(1), ARMEABISUPPORT_STACKOP_GET_STACK, top - 4, &stack) != NULL || stack == NULL)
    return;
  if (_swix (ARMEABISupport_StackOp, _INR(0,1), ARMEABISUPPORT_STACKOP_FREE, stack) == NULL)
    gbl->signalhandler_sp = NULL;
}
#endif

/* Final process termination. 'return_code' is a 16-bit
   encoded quantity as defined by <sys/wait.h>.
   Also called via __unixlib_fatal() so don't assume too much.  */
void
_exit (int return_code)
{
  struct ul_global *gbl = &__ul_global;
  struct __sul_process *sulproc = gbl->sulproc;
  int status;

#ifndef __ARM_EABI__
  /* Disable profiling. If profiling was not enabled, then this function
     will have nothing to do.  */
  gmon_machine_cleanup ();
#endif

  /* Interval timers must be stopped.  */
  __stop_itimers ();

  /* pthread timers must be stopped */
  __pthread_prog_fini ();

  /* De-register with DigitalRenderer in case of an exception */
  __dsp_exit();

  /* Convert the 16-bit return code into an 8-bit equivalent
     for compatibility with RISC OS.

     The exit code:
	if bit 7 == 0
	   bits 0..6 = normal exit code
	else
	  {
	     bits 0..5 = signal number that terminated the process.
	     bit 6 == 1 if process core dumped
	  }
  */

  if (WIFSIGNALED (return_code))
    {
      /* Process terminated with a signal.  */
      status = WTERMSIG (return_code);
      sulproc->status.return_code = 0;
      sulproc->status.signal_exit = 1;
      sulproc->status.signal = status;
      status |= (1 << 7);

      if (WCOREDUMP (return_code))
	{
	  status |= (1 << 6);
	  sulproc->status.core_dump = 1;
	}
      else
	sulproc->status.core_dump = 0;
    }
  else
    {
      status = WEXITSTATUS (return_code);
      sulproc->status.return_code = status;
      sulproc->status.core_dump = 0;
      sulproc->status.signal_exit = 0;
    }

  /* Reset the DDEUtils' Prefix variable to the value at startup.  */
  if (__u->dde_prefix)
    (void) SWI_DDEUtils_Prefix (__u->dde_prefix);

  /* Re-enable Escape (in case SIGINT handler fired in ttyicanon) */
  if (!gbl->escape_disabled)
    SWI_OS_Byte (229, 0, 0, NULL, NULL);

  __free_process (sulproc);
  __dynamic_area_exit ();
  __env_riscos ();
#ifdef __ARM_EABI__
  __signalhandler_stack_free ();
#endif

#ifdef DEBUG
  debug_printf ("-- __exit(): Calling sul_exit with return code=%d\n", status);
#endif

  sulproc->sul_exit (sulproc->pid, status);
}

int
__alloc_file_descriptor (int start)
{
  struct ul_global *gbl = &__ul_global;
  struct __sul_process *sulproc = gbl->sulproc;
  unsigned int i;

  PTHREAD_UNSAFE

  /* Look for a spare file descriptor.  */
  for (i = start; i < sulproc->maxfd; i++)
    if (getfd (i)->devicehandle == NULL)
      {
#ifdef DEBUG
	debug_printf ("-- __alloc_file_descriptor: found free descriptor %d\n",
		      i);
#endif
	return i;
      }

  /* Set errno to indicate the unavailability of file handles.  */
  return __set_errno (EMFILE);
}

void
__stop_itimers (void)
{
  struct itimerval new_timer;

  /* Interval timers are not implemented in task windows nor in WIMP
     programs so we don't need to stop them.  */
  if (__get_taskhandle () != 0)
    return;

  /* Stop all interval timers.  */
  new_timer.it_interval.tv_sec = 0;
  new_timer.it_interval.tv_usec = 0;
  new_timer.it_value.tv_sec = 0;
  new_timer.it_value.tv_usec = 0;
  setitimer (ITIMER_REAL, &new_timer, 0);
  setitimer (ITIMER_VIRTUAL, &new_timer, 0);
  setitimer (ITIMER_PROF, &new_timer, 0);
}

/* Define and initialise the Unix I/O.  */
static void
initialise_unix_io (void)
{
  struct __sul_process *sulproc = __ul_global.sulproc;

  if (sulproc->file_descriptors != NULL)
    return;

  sulproc->file_descriptors = sulproc->sul_malloc (sulproc->pid,
						   sulproc->maxfd * sulproc->fdsize);
  if (sulproc->file_descriptors == NULL)
    __unixlib_fatal ("Cannot allocate file descriptor memory");

  /* Set all file descriptors to unallocated status.  */
  for (unsigned int i = 0; i < sulproc->maxfd; i++)
    getfd (i)->devicehandle = NULL;

  /* These are guaranteed to be the first files opened. stdin, stdout and
     stderr will receive file descriptor numbers 0, 1 and 2 respectively.  */

  /* Open a file descriptor for reading from the tty (stdin) and writing to
     the tty (stdout) when there is no CLI redirection done by RISC OS,
     otherwise take RISC OS own files handles as basis for stdin and/or
     stdout.  */
  const _kernel_oserror *err;
  int prev_fh_in, prev_fh_out;
  if ((err = SWI_OS_ChangeRedirection (-1, -1, &prev_fh_in, &prev_fh_out)) != NULL)
    __unixlib_fatal (err->errmess);
  if ((prev_fh_in ? __open_fh (STDIN_FILENO, prev_fh_in, O_RDONLY, 0777)
		  : __open_fn (STDIN_FILENO, "/dev/tty", O_RDONLY, 0777)) < 0)
    __unixlib_fatal ("Cannot open stdin");
  if ((prev_fh_out ? __open_fh (STDOUT_FILENO, prev_fh_out, O_WRONLY | O_CREAT, 0666)
		   : __open_fn (STDOUT_FILENO, "/dev/tty", O_WRONLY | O_CREAT, 0666)) < 0)
    __unixlib_fatal ("Cannot open stdout");

  /* Duplicate the file descriptor for stdout, to create a suitable file
     descriptor for stderr.  */
  if (fcntl (STDOUT_FILENO, F_DUPFD, STDERR_FILENO) < 0)
    __unixlib_fatal ("Cannot open stderr");
}

/* Attempt to re-direct a file descriptor based on the file descriptor
   number that was passed on the command line.  */
static void
check_fd_redirection (const char *filename, unsigned int fd_to_replace)
{
  /* <&- means shut.
     <&[fd] would mean dup this fd.  */

  if (*filename == '-')
    {
      close (fd_to_replace);
      return;
    }

  if (isdigit (*filename))
    {
      unsigned int dup_fd = __decstrtoui (filename, NULL);

      if (dup_fd >= __ul_global.sulproc->maxfd)
	__badr ();

      if (dup_fd != fd_to_replace)
	{
	  /* Duplicate the file descriptor.  */
	  dup2 (dup_fd, fd_to_replace);
	}
    }
  else
    __badr ();
}

/* Translate the file descriptor part of [fd]>filename into a number.  */
static int
get_fd_redirection (const char *redir)
{
  unsigned int fd, multiplier;

  /* On entry, *redir will point to either a left or right chevron.  */
  redir--;

  /* Since we're reading the number backwards, we will use a multiplier
     to correctly position the digit into the number.  */
  multiplier = 1;

  fd = 0;
  while (*redir && isdigit (*redir))
    {
      fd += (*redir - '0') * multiplier;
      multiplier *= 10;
      redir--;
    }
  if (fd >= __ul_global.sulproc->maxfd)
    __badr ();

  return fd;
}

static const char *
check_io_redir (const char *p, int fd, int mode)
{
  if (isdigit (p[-1]))
    fd = get_fd_redirection (p);
  else if (p[-1] == '>' && isdigit (p[-2]))
    fd = get_fd_redirection (p - 1);

#ifdef DEBUG
  debug_printf ("-- check_io_redir: redirecting fd %d\n", fd);
#endif

  /* Skip any whitespace that precedes the filename e.g '< filename'.  */
  p++; /* skip the '<' */
  while (*p && isspace (*p))
    p++;

  char fn[256];
  char *ptr = fn;
  if (*p == '"')
    {
      /* Filename is double quoted, find the matching ending double quote.  */
      ++p;
      while (ptr != fn + sizeof (fn) && *p != '"')
	{
	  switch (*p)
	    {
	      case '\0':
		__unixlib_fatal ("Wrongly quoted redirection filename");
		break;
	      case '\\':
		++p;
		/* Fall through.  */
	      default:
		*ptr++ = *p++;
		break;
	    }
	}
    }
  else
    {
      /* Search through a string and look for a terminator that is one of
	 NUL, space, newline or carriage-return.  */
      while (ptr != fn + sizeof (fn) && *p != '\0' && *p != ' ')
	*ptr++ = *p++;
    }
  if (ptr == fn + sizeof (fn))
    __unixlib_fatal ("Redirection filename too long");
  *ptr = '\0';

#ifdef DEBUG
  debug_printf ("-- check_io_redir: filename='%s', mode=%08x\n",
		fn, mode);
#endif

  /* Check the >& construct.  */
  if (fn[0] == '&')
    check_fd_redirection (fn + 1, fd);
  else
    {
      /* Close the file descriptor, if it was defined and open.  */
      if (!BADF (fd))
	__close (getfd (fd));

      fd = __open_fn (fd, fn, mode, 0666);
      if (fd < 0)
	{
	   char failure[300];
	   snprintf (failure, sizeof (failure),
		     "Cannot open %s for I/O redirection", fn);
	   __unixlib_fatal (failure);
	}

      if (((mode & O_WRONLY) || (mode & O_RDWR)) && ! (mode & O_TRUNC))
	/* Seek to the end of the file, because we are appending.  */
	lseek (fd, 0, SEEK_END);
    }
  return p;
}

static void
get_io_redir (const char *cli)
{
  /* By default, we redirect file descriptor 0 (stdin).  */
#ifdef DEBUG
  debug_printf ("-- get_io_redir: checking <\n");
#endif
  for (const char *p = cli; (p = find_redirection_type (p, '<')) != NULL; ++p)
    {
      /* A redirector `<>' means read/write.  */
      int mode = (p[1] == '>') ? O_RDWR : O_RDONLY;
      p = check_io_redir (p, STDIN_FILENO, mode);
    }
  
  /* By default, we redirect file descriptor 1 (stdout).  */
  const int mode = O_WRONLY | O_CREAT;
#ifdef DEBUG
  debug_printf ("-- get_io_redir: checking >\n");
#endif
  for (const char *p = cli; (p = find_redirection_type (p, '>')) != NULL; ++p)
    {
      if (p[-1] != '<')
	{
	  /* p might point to:
	     1. >
	     2. >>
	     3. 2>
	     4. 2>>
	  */
	  if (isdigit (p[0]))
	    ++p;
	  if (p[0] == '>' && p[1] == '>')
	    p = check_io_redir (++p, STDOUT_FILENO, mode);
	  else if (p[0] == '>')
	    p = check_io_redir (p, STDOUT_FILENO, mode | O_TRUNC);
	}
    }
}

/* Verify that 'redirection' is actually pointing to a redirection
   operator. Return 0 if it is not, return 1 if it is.  */
static int
verify_redirection (const char *redir)
{
#ifdef DEBUG
  debug_printf ("-- verify_redirection: %s\n", redir);
#endif

  /* So we've found a re-direction operator.  We must watch out
     for the filename <spec$dir>.file since these chevrons so not
     refer to re-direction.  */

  /* Check for special 'give away' character sequences that we can
     be sure are re-direction operators:
	1. >>
	2. >&[a number]
	3. <&[a number]
	4. >&-
	5. <&-
	6. <>
	7. >[space]
	8. <[space]
	9. [a number]>
	10. [a number]<
	11. [a number]<>

     Without the [a number] >& can refer to the start of a valid
     RISC OS pathname.  */
  if (redir[0] == '>' && redir[1] == '>')
    return 1;

  if (redir[1] == '&'
      && (redir[2] == '-' || isdigit (redir[2])))
    return 1;


  /* This should match:
     "2>foo"
     "2<foo"
  */
  int x = 0;
  if (isdigit (redir[x]))
    {
      while (redir[x] && isdigit (redir[x]))
	x++;

      if (redir[x] == '<' || redir[x] == '>')
	return 1;
    }

  if (redir[0] == '<')
    {
      const char *t = redir;

      /* Check for a construct like <<spec$dir>.file.  or for `<>'  */
      if (redir[1] == '<' || redir[1] == '>')
	return 1;

      while (*t && *t != ' ' && *t != '>')
	t++;

      /* If there's no right chevron on the rest of the cli, then we
	 must have a proper re-direction operator.  */
      if (*t == '\0' || *t == ' ')
	return 1;

      /* It was a RISC OS filename.  */
      return 0;
    }

  /* The output re-direction operator (right chevron) is very easy
     to check.  The only position in which it can appear is in the
     middle of a RISC OS filename e.g. <spec$dir>.file.  If there
     is a space before this chevron, then it *must* be referring
     to a re-direction operator.

     An array index of -1 is safe here, because it is impossible for
     a right chevron to appear at the start of the command line
     (unless you are the idiot who regularly uses > for a filename). */
  if (redir[0] == '>' && redir[-1] == ' ')
    return 1;

  return 0;
}

static const char *
find_redirection_type (const char *cmdline, char redirection_type)
{
  const char * const org_cmdline = cmdline;

  while (1)
    {
      /* Look for redirection operator `redirection_type'.  */
      while (*cmdline && *cmdline != redirection_type)
	{
	  while (*cmdline && *cmdline != redirection_type
		 && *cmdline != '"' && *cmdline != '\'')
	    cmdline++;

	  if (*cmdline == '"')
	    {
	      /* Skip over anything in double quotes */
	      cmdline++;
	      while (*cmdline && *cmdline != '"')
		cmdline++;
	      if (*cmdline == '"')
		cmdline++;
	    }
	  else if (*cmdline == '\'')
	    {
	      /* Skip over anything in single quotes, unless it is an
		 apostrophe in a filename */
	      cmdline++;
	      if (cmdline[-2] == ' ')
		{
		  while (*cmdline && *cmdline != '\'')
		    cmdline++;
		  if (*cmdline == '\'')
		    cmdline++;
		}
	    }
	}

      if (*cmdline == '\0')
	/* Operator `redirection_type' doesn't exist.  */
	return NULL;

      /* Cope with "2> foobar" */
      while (cmdline != org_cmdline && isdigit (cmdline[-1]))
	cmdline --;

      if (verify_redirection (cmdline))
	return cmdline;
      /* Skip a character otherwise we loop on <foo$bar>.  */
      cmdline ++;
    }
}

/* Convert a command line that is contained as a single string, with
   arguments delimited by spaces, into a zero terminated string with
   index markers.  */
static void
convert_command_line (struct proc *process, const char *cli, int cli_size)
{
  /* A temporary buffer for the command line arguments, holds a
     particular argument prior to it being added to the argv array.  */
  char *temp = (char *) malloc (cli_size + 1);

  int argc = 0;
  char **argv = (char **) malloc ((argc + 2) * sizeof (char *));
  if (temp == NULL || argv == NULL)
    goto fatal;
  while (*cli)
    {
      char *p;

      /* Set to 1 if we are storing an empty argument, for example
	 -classpath ''  */
      int empty_arg = 0;

      /* Skip any white space.  */
      while (isspace (*cli))
	cli++;

      /* Finish if we have reached the end of the command line.  */
      if (*cli == '\0')
	break;

      p = temp;

      /* We check for *cli != NULL here because of a command line like:
	    main 5>fred

	 verify_redirection will catch this and move cli onto the
	 terminating character (in this case, the null).  NULL is not
	 a space, so we'd start writing garbage into the argv array.  */
      while (*cli && ! isspace (*cli))
	{
	  /* Don't check for a redirection operator in the first argument
	     because that is the program name.  */
	  if (argc != 0 && verify_redirection (cli))
	    {
	      /* We don't add redirection operators to the argument list.
		 So skip all characters up until the next real argument.  */

	      /* We have to watch out for stuff like:
		   1. <> filename
		   2. < filename
		   3. > filename
		   4. >> filename.  */

	      /* Just incrementing cli by one here converts a <> filename
		 into a > filename.  We're not interested in the re-direction
		 stuff anyway.  */
	      while (*cli && isdigit (*cli))
		cli++;

	      if ((cli[0] == '<' || cli[0] == '>') && cli[1] == '>')
		cli++;

	      if (cli[1] == ' ')
		{
		  /* Skip the chevron.  */
		  cli++;

		  /* Skip any initial whitespace after the chevron.  */
		  while (*cli && isspace (*cli))
		    cli++;
		}

	      /* By the time we've reached here, we should have >filename
		 or just filename.  */

	      /* Skip the filename bit after the chevron.  */
	      while (*cli && !isspace (*cli))
		cli++;
	    }
	  else if (*cli == '\\')
	    {
	      /* Escape character.  */
	      switch (*++cli)
		{
		case '\0':
		  break;

		case 'x':
		  if (cli[1] == '\0' || cli[2] == '\0')
		    break;

		  *p++ = __hexstrtochar (cli + 1);
		  cli += 3;
		  break;
		default:
		  *p++ = *cli++;
		}
	    }
	  else if (*cli == '\"')
	    {
	      /* Argument is enclosed in double-quotes.  */

	      /* Skip first quote.  */
	      cli++;
	      /* Keep copying characters into the argv_string until we reach
		 the terminating quotation mark.  */
	      while (*cli != '\0' && *cli != '\"')
		{
		  /* Look out for escape characters.  */
		  if (*cli == '\\')
		    {
		      switch (*++cli)
			{
			case '\0':
			  break;

			case 'x':
			  if (cli[1] == '\0' || cli[2] == '\0')
			    break;

			  *p++ = __hexstrtochar (cli + 1);
			  cli += 3;
			  break;
			default:
			  *p++ = *cli++;
			}
		    }
		  else
		    *p++ = *cli++;
		}
	      /* If we've finished on the quotation mark then skip over it.  */
	      if (*cli == '\"')
		{
		  if (cli[-1] == '\"')
		    empty_arg = 1;
		  cli++;
		}
	    }
	  else if (*cli == '\'' && isspace(cli[-1]))
	    {
	      /* The argument is contained within single quotes. We also
		 hackily check for a space before it - this avoids problems
		 with filenames with apostrophes in them.  */
	      cli++;
	      while (*cli != '\0' && *cli != '\'')
		{
		  if (cli[0] == '\\' && cli[1] == 'x' && cli[2] && cli[3])
		    {
		      /* Sort out escape sequences that are added by UnixLib */
		      *p++ = __hexstrtochar (cli + 2);
		      cli += 4;
		    }
		  else
		    *p++ = *cli++;
		}
	      /* If we've finished on the single quote mark, then skip it.  */
	      if (*cli == '\'')
		{
		  if (cli[-1] == '\'')
		    {
		      /* The argument was given as an empty argument.
			 A circumstance like this occurs with commands like
			    -classpath ''
			 We must therefore store an empty argument.
		      */
		      empty_arg = 1;
		    }
		  cli++;
		}
	    }
	  else
	    *p++ = *cli++;
	}

      *p = '\0';
      /* We have now reached a space delimiter.  Add the argument to
	 the argv list, if one was actually made.  */
      if (p != temp || empty_arg)
	{
	  argc ++;
	  argv = (char **) realloc (argv, (argc + 1) * sizeof (char *));
	  argv[argc - 1] = strdup (temp);
	  if (argv == NULL || argv[argc - 1] == NULL)
	    goto fatal;
	}
    }

  /* Set the last item to NULL */
  argv[argc] = NULL;

  free (temp);

  /* Convert the process filename, either to a RISC OS full path, either
     to an Unix full path.
     Note that there is a fundamental problem in any pre RISC OS 6 version
     when you start a process via a temporary FS selection and that the
     current FS get taken instead.  Use "/ADFS::MyDisc.$.my_risc_os_process"
     or "run ADFS::MyDisc.$.my_risc_os_process" instead of
     "ADFS::MyDisc.$.my_risc_os_process" to avoid this problem.  */
  const char *canon_argv0 = __canonicalise_path (argv[0], "Run$Path");
  if (canon_argv0 == NULL)
    goto fatal;

  const char *input_argv0;
  int filetype;
  int objtype, loadaddr;
  if (SWI_OS_File_ReadCatInfo (canon_argv0, &objtype, &loadaddr, NULL, NULL,
			       NULL) != NULL
      || objtype != 1)
    {
#ifdef DEBUG
      __os_print ("WARNING: cannot stat() process filename\r\nDid you use a temporary FS used to startup? If so, better use '*run' instead.\r\n");
#endif
      filetype = __RISCOSIFY_FILETYPE_NOTFOUND;
      /* Use the uncanonicalised argv[0] as input for unixify.  */
      input_argv0 = argv[0];
    }
  else
    {
      filetype = (loadaddr & 0xfff00000U) == 0xfff00000U ? (loadaddr >> 8) & 0xfff : __RISCOSIFY_FILETYPE_NOTFOUND;
      input_argv0 = canon_argv0;
    }

  /* Convert to Unix full path, if needed.  */
  char *uargv0;
  if ((uargv0 = __unixify_std (input_argv0, NULL, 0, filetype)) == NULL)
    goto fatal;

  free ((void *)canon_argv0);
  free (argv[0]);
  argv[0] = uargv0;

  process->argc = argc;
  process->argv = argv;

  return;

fatal:
  __unixlib_fatal ("Failed to process command line");
}
