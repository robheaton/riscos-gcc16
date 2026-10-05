/* Execute a new program.
   Copyright (c) 2002-2013 UnixLib Developers.  */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <swis.h>
#include <sys/wait.h>

#include <unixlib/local.h>

#include <internal/dev.h>
#include <internal/local.h>
#include <internal/os.h>
#include <internal/unix.h>
#include <internal/symlinks.h>

/* #define DEBUG 1 */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

/* Setup the extended command line by sending all the arguments to the
   program via DDEUtils module.  */
static int
set_dde_cli (char *cli)
{
  /* Skip program name.  */
  char *temp;
  for (temp = cli; *temp != '\0' && *temp != ' '; ++temp)
    ;
  /* We know there is exactly one space after the program name.  */
  if (*temp == ' ')
    ++temp;

  /* Check that the actual command is not greater than RISC OS's maximum path
     length. If so, there is no way this is going to work.  */
  if (temp - cli >= MAXPATHLEN)
    return __set_errno (E2BIG);

  /* Set the command line size within DDEUtils.  */
  size_t cli_size = strlen (temp) + 1;
  const _kernel_oserror *err;
  if ((err = SWI_DDEUtils_SetCLSize (cli_size)) != NULL
      || (err = SWI_DDEUtils_SetCL (temp)) != NULL)
    return __ul_seterr (err, E2BIG);

  /* As we're now passing the CLI arguments via DDEUtils, the CLI is now
     reduced to the program name only.  */
  if (*temp != '\0')
    temp[-1] = '\0';		/* terminate cli.  */
#ifdef DEBUG
  debug_printf ("-- set_dde_cli\n");
#endif
  return 0;
}

/* Execute program `execname' and pass command line arguments `argv'
   and give it the environment `envp'.  */
int
execve (const char *execname, char *const argv[], char *const envp[])
{
  struct ul_global *gbl = &__ul_global;
  struct __sul_process *sulproc = gbl->sulproc;

  if (!execname)
    return __set_errno (EINVAL);

  char *null_list = NULL;
  if (!argv)
    argv = &null_list;

#ifdef DEBUG
  debug_printf ("-- execve: function arguments\n"
                "   execname: '%s'\n", execname);
  for (int x = 0; argv[x] != NULL; ++x)
    debug_printf ("   argv[%d]: %s\n", x, argv[x]);

  for (int x = 0; envp[x] != NULL; ++x)
    debug_printf ("   envp[%d]: %s\n", x, envp[x]);

  __debug ("-- execve: process structure");
#endif

  /* scenario : number of arguments which we're going to skip.  */
  int scenario;
  if (execname[0] == '*' && execname[1] == '\0' && argv[1] != NULL)
    scenario = 1;
  else if (!strcmp (execname, "/bin/sh")
	   && argv[1] != NULL && !strcmp (argv[1], "-c") && argv[2] != NULL)
    scenario = 2;
  else
    scenario = 0;

#if __UNIXLIB_SYMLINKS
  char respathname[MAXPATHLEN];
#endif
  const char *program_name;
  char pathname[MAXPATHLEN];
  size_t nasty_hack;
  if (scenario == 0)
    {
      nasty_hack = 0;

      /* If the first argument is the same as the execname then make sure we
         don't end up with it twice on the command line.  */

      if (argv[0] && strcmp (argv[0], execname) == 0)
	scenario = 1;

      /* Convert the program name into a RISC OS format filename.  */
      if (!__riscosify_std (execname, 0, pathname, sizeof (pathname), NULL))
	return __set_errno (E2BIG);

#if __UNIXLIB_SYMLINKS
      /* When pathname starts with a % and is not a current library indicator,
	 then we consider this as an alias-disabling prefix.  */
      if (pathname[0] == '%' && pathname[1] != '.')
	program_name = pathname;
      else
	{
	  if (__resolve_symlinks (pathname, respathname,
				  sizeof (respathname), 0) != 0)
	    return -1;
	  program_name = respathname;
	}
#else
      program_name = pathname;
#endif
      if (!scenario)
	{
	  /* Canonicalise the program name  */
	  char canon_exec[MAXPATHLEN];
	  if (SWI_OS_FSControl_Canonicalise (program_name, NULL,
					     canon_exec, sizeof (canon_exec),
					     NULL) == NULL)
	    {
	      char ro_arg[MAXPATHLEN];
	      if (__riscosify_std (argv[0], 0, ro_arg, sizeof (ro_arg), NULL))
		{
		  /* Canonicalise argv[0]  */
		  char canon_arg[MAXPATHLEN];
		  if (SWI_OS_FSControl_Canonicalise (ro_arg, NULL,
						     canon_arg, sizeof (canon_arg),
						     NULL) == NULL
		      && strcmp (canon_exec, canon_arg) == 0)
		    scenario = 1;
		}
	    }
	}
    }
  else
    {
      /* scenario 1 : We've arrived here from a user call to system(),
         with execname == "*" because on RISC OS the '*' is equivalent
         to the SHELL of /bin/bash on Unix. If this is the case, then
         we'll probably find the real program name in argv[1].
         scenario 2 : "/bin/sh -c xyz" is called and we will execute this
         as "xyz" (i.e. argv[2]).  */
      const char *p = argv[scenario];

      while (*p == ' ')
	p++;
      char temp[MAXPATHLEN];
      size_t x;
      for (x = 0; x < sizeof (temp) && *p && *p != ' '; /* */)
	temp[x++] = *p++;
      if (x == sizeof (temp))
	return __set_errno (E2BIG);
      temp[x] = '\0';

      /* Ah. The nasty hack.  Comes into everything somewhere.
         Since argv[1] contains our program name and all arguments,
         we need to stop the command line builder sticking quotes
         around everything and also not include the program name
         twice in the argument vector.
         So nasty_hack contains the length of the program name
         held within argv[1].  We then know (when non-zero) how
         many characters to skip, if at all.  */
      nasty_hack = x;

#ifdef DEBUG
      debug_printf ("-- execve: pathname: '%s'\n", temp);
#endif

      if (!__riscosify_std (temp, 0, pathname, sizeof (pathname), NULL))
	return __set_errno (E2BIG);

#if __UNIXLIB_SYMLINKS
      /* When pathname starts with a % and is not a current library indicator,
	 then we consider this as an alias-disabling prefix.  */
      if (pathname[0] == '%' && pathname[1] != '.')
	program_name = pathname;
      else
	{
	  if (__resolve_symlinks (pathname, respathname,
				  sizeof (respathname), 0) != 0)
	    return -1;
	  program_name = respathname;
	}
#else
      program_name = pathname;
#endif
    }

#ifdef DEBUG
  debug_printf ("-- execve: program_name: %s\n", program_name);
  debug_printf ("-- execve: building command line\n");
#endif

  /* Calculate the length of the command line.  */
  size_t cli_length = strlen (program_name) + 1;
  for (size_t x = scenario; argv[x] != NULL; ++x)
    {
      const char *p;
      int space = 0;
      /* We must add extra characters if the argument contains spaces (wrap
         argument in quotes) and quotes (must add backslash) and
         inverted commas (must add backslash) and control chars
         (change to \xXX).  */
      p = argv[x];
      if (nasty_hack && x == scenario)
	p += nasty_hack;
      for ( /* */ ; *p; ++p)
	{
	  if (isspace (*p))
	    space = 1;
	  if ((!nasty_hack || x != scenario) && (*p == '\"' || *p == '\''))
	    cli_length++;
	  if (*p == 127 || *p < 32)
	    cli_length += 3;
	  cli_length++;
	}

      /* Account for quotes around argument but only if our hack isn't
         in place.  */
      if (space && !nasty_hack)
	cli_length += 2;

      cli_length++;
    }

#ifdef DEBUG
  debug_printf ("-- execve: cli_length = %d\n", cli_length);
#endif

  /* SUL will free cli for us when we call sul_exec.  */
  char *cli = sulproc->sul_malloc (sulproc->pid, cli_length + 1);
  if (cli == NULL)
    return __set_errno (ENOMEM);

  /* Copy program name into cli and terminate with a space, ready for below.
     Should trim leading space between '*' and command in name.  */
  char *command_line = cli;
  while (*program_name == '*' || *program_name == ' ')
    program_name++;

  command_line = stpcpy (command_line, program_name);
  *command_line++ = ' ';

  /* Copy the rest of the arguments into the cli.  */
  if (argv[0])
    {
      for (int x = scenario; argv[x] != NULL; ++x)
	{
	  char *p = argv[x];
	  int contains_space = 0;

	  if (nasty_hack && x == scenario)
	    p += nasty_hack;

	  while (*p != '\0' && !isspace (*p))
	    p++;

	  /* Don't enclose arguments in additional quotes if our nasty hack
	     is in place.  */
	  if (!nasty_hack && *p)
	    contains_space = 1;

	  /* Add quotes, if argument contains a space.  */
	  if (contains_space)
	    *command_line++ = '\"';

	  p = argv[x];
	  if (nasty_hack && x == scenario)
	    p += nasty_hack;
	  for ( /* */ ; *p; ++p)
	    {
	      /* If character is a " or a ' then preceed with a backslash.  */
	      if ((!nasty_hack || x != scenario) && (*p == '\"' || *p == '\''))
		*command_line++ = '\\';

	      if (*p == 127 || *p < 32)
		{
		  sprintf (command_line, "\\x%.2X", *p);
		  command_line += 4;
		}
	      else
		*command_line++ = *p;
	    }
	  if (contains_space)
	    *command_line++ = '\"';
	  *command_line++ = ' ';
	}
    }
  command_line[-1] = '\0';

#ifdef DEBUG
  debug_printf ("-- execve: cli: %s\n", cli);
#endif

  /* If the cli is >= MAXPATHLEN, we will need the aid of DDEUtils. When this
     is done, cli will only contain the program name.  */
  if (cli_length >= MAXPATHLEN && set_dde_cli (cli) < 0)
    {
      sulproc->sul_free (sulproc->pid, cli);
      return -1;
    }

  char **newenviron;
  size_t envlen;
  if (envp)
    {
      envlen = sizeof (char *);

      /* Count new environment variable vector length.  */
      unsigned int x;
      for (x = 0; envp[x] != NULL; x++)
	envlen += sizeof (char *) + strlen (envp[x]) + 1;

      unsigned int enventries = x;

      /* The length must be word aligned */
      envlen = (envlen + 3) & ~3;

      newenviron = sulproc->sul_malloc (sulproc->pid, envlen);
      if (newenviron == NULL)
	{
	  sulproc->sul_free (sulproc->pid, cli);
	  return __set_errno (ENOMEM);
	}

      /* Copy the environment */
      char *offset = (char *) (newenviron + enventries + 1);
      for (x = 0; x < enventries; x++)
	{
	  size_t len = strlen (envp[x]) + 1;
	  newenviron[x] = offset;
	  memcpy (offset, envp[x], len);
	  offset += len;
	}
      newenviron[enventries] = NULL;
    }
  else
    {
      newenviron = NULL;
      envlen = 0;
    }

  /* Setup RISC OS redirection.  This will only be used by non-UL child
     processes.  */
  if (sulproc->ppid != 1)
    {
      int fh_in = (!BADF (0)
		   && (getfd (0)->devicehandle->type == DEV_RISCOS
		       || getfd (0)->devicehandle->type == DEV_PIPE)) ?
		    (int) getfd (0)->devicehandle->handle : -1;
      int fh_out = (!BADF (1)
		    && (getfd (1)->devicehandle->type == DEV_RISCOS
		        || getfd (1)->devicehandle->type == DEV_PIPE)) ?
		     (int) getfd (1)->devicehandle->handle : -1;
      free ((void *)gbl->ul_out_pathname);
      gbl->ul_out_pathname = (fh_out != -1) ? __canonicalise_handle (fh_out) : NULL;

      const _kernel_oserror *err;
      if ((err = SWI_OS_ChangeRedirection (fh_in, fh_out,
					   &gbl->changeredir0,
					   &gbl->changeredir1)) != NULL)
        return __ul_seterr (err, EOPSYS);

      if (!BADF (1) && getfd (1)->devicehandle->type == DEV_PIPE)
        {
          /* Read the current (read) file pointer and seek then to the end.
             When returning in vfork(), we'll reset the file pointer back to
             its read position again.  */
          int handle = (int) getfd (1)->devicehandle->handle;

	  if ((err = SWI_OS_Args_GetFilePtr (handle,
					     &gbl->rewindpipeoffset)) != NULL)
            return __ul_seterr (err, EOPSYS);
	  __off_t extent;
	  if ((err = SWI_OS_Args_GetExtent (handle, &extent)) != NULL
	      || (err = SWI_OS_Args_SetFilePtr (handle, extent)) != NULL)
            return __ul_seterr (err, EOPSYS);
        }
    }

  /* From this point onwards we cannot return an error */

  /* All threads are terminated on an exec call.
     Destructor functions are not called. */
  if (gbl->pthread_system_running)
    {
      __pthread_stop_ticker ();
      gbl->pthread_system_running = 0;
    }

#if __UNIXLIB_FEATURE_ITIMERS
  /* Stop any interval timers that might be running.  Technically
     the new process should inherit the pending alarms.  */
  __stop_itimers ();
#endif

  if (sulproc->ppid == 1)
    {
      /* This process doesn't have a parent. Technically, all file descriptors
         that don't have FD_CLOEXEC set should remain open, but if we are
         calling a non-UnixLib program then it won't know about them and so
         they will never get closed. */
      __free_process (sulproc);
    }
  else
    {
      unsigned int x;
      /* File descriptors open in the existing process image remain open
         in the new process image, unless they have the 'FD_CLOEXEC'
         flag set.  O_EXECCL is an alias for FD_CLOEXEC.  */
      for (x = 0; x < sulproc->maxfd; x++)
	if (getfd (x)->devicehandle && (getfd (x)->dflag & O_EXECCL))
	  close (x);

      if (sulproc->environ)
	sulproc->sul_free (sulproc->pid, sulproc->environ);
    }

  sulproc->environ = newenviron;
  sulproc->environ_size = envlen;

  /* Force a malloc trim to reduce memory usage.  */
  malloc_trim_unlocked (gbl->malloc_state, 0);
#if __UNIXLIB_CHUNKED_STACK
  __stackalloc_trim ();
#endif

  /* If the DAs are being used, then delete the dynamic area. */
  __dynamic_area_exit ();

  /* Restore the RISC OS environment handlers.  This breaks us out
     of the UnixLib world.  */
  __env_riscos ();
#ifdef __ARM_EABI__
  /* The signal stack of this program image goes too, when this process was its last user.  */
  __signalhandler_stack_free ();
#endif

  /* Call SUL to execute command.
     (This function never returns, even for a RISC OS built-in
     command.)  */
#ifdef DEBUG
  debug_printf ("-- execve: about to call: %s\n", cli);
#endif

  /* Record the fact that the new process was the result of exec().  */
  sulproc->status.execed = 1;

#ifdef PIC
  if (sulproc->ppid == 1)
    {
      /* Tell the Shared Object Manager that the current client has exited.
	 We can still access global variables afterwards, because the PIC
	 register and R/W data segment are still valid and are not the
	 responsibilty of the Shared Object Manager.  */
      int regs[10];

      (void) __os_swi (SOM_DeregisterClient, regs);
    }
#endif

  sulproc->sul_exec (sulproc->pid, cli,
		     (void *) __ul_memory.stack_limit,
		     (void *) __ul_memory.stack);

  /* This is never reached.  */
  return -1;
}
