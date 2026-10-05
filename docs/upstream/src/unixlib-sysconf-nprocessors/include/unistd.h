/*
 * POSIX Standard 2.10: Symbolic Constants <unistd.h>.
 * Copyright (c) 2000-2013 UnixLib Developers
 */

#ifndef __UNISTD_H
#define __UNISTD_H

#ifndef __UNIXLIB_FEATURES_H
#include <features.h>
#endif

#ifndef __UNIXLIB_TYPES_H
#include <unixlib/types.h>
#endif

__BEGIN_DECLS

/* These may be used to determine what facilities are present at compile time.
   Their values can be obtained at run time from sysconf.  */

/* POSIX Standard approved as IEEE Std 1003.1 as of August, 1988.  */
#define	_POSIX_VERSION	199009L

#define	_POSIX2_C_VERSION	199912L	/* Invalid until 1003.2 is done.  */

/* If defined, the implementation supports the
   C Language Bindings Option.  */
#define	_POSIX2_C_BIND	1

/* If defined, the implementation supports the
   C Language Development Utilities Option.  */
#define	_POSIX2_C_DEV	1

/* If defined, the implementation supports the
   Software Development Utilities Option.  */
#define	_POSIX2_SW_DEV	1

/* Protocol-independent interfaces are supported.  */
#define _POSIX_PII 1

/* Socket protocol-independent interfaces are supported.  */
#define _POSIX_PII_SOCKET 1

/* Internet family of protocols is supported.  */
#define _POSIX_PII_INTERNET 1

/* Connection-mode Internet protocol is supported.  */
#define _POSIX_PII_INTERNET_STREAM 1

/* Connectionless Internet protocol is supported.  */
#define _POSIX_PII_INTERNET_DGRAM 1

/* Library supports the poll function.  */
#define _POSIX_POLL 1

/* Library supports select and pselect.  */
#define _POSIX_SELECT 1

#include <bits/posix_opt.h>

/* Standard file descriptors.  */
#define	STDIN_FILENO	0	/* Standard input.  */
#define	STDOUT_FILENO	1	/* Standard output.  */
#define	STDERR_FILENO	2	/* Standard error output.  */

#ifndef __ssize_t_defined
typedef __ssize_t ssize_t;
#define __ssize_t_defined
#endif

#define __need_size_t
#define __need_NULL
#include <stddef.h>

/* The Single Unix specification says that some more types are
   available here.  */
#ifndef __gid_t_defined
typedef __gid_t gid_t;
#define __gid_t_defined
#endif

#ifndef __uid_t_defined
typedef __uid_t uid_t;
#define __uid_t_defined
#endif

#ifndef __off_t_defined
# ifndef __USE_FILE_OFFSET64
typedef __off_t off_t;
# else
typedef __off64_t off_t;
# endif
#define __off_t_defined
#endif
#if defined __USE_LARGEFILE64 && !defined __off64_t_defined
typedef __off64_t off64_t;
# define __off64_t_defined
#endif

#ifndef __useconds_t_defined
typedef __useconds_t useconds_t;
#define __useconds_t_defined
#endif

#ifndef __pid_t_defined
typedef __pid_t pid_t;
#define __pid_t_defined
#endif

#ifndef __intptr_t_defined
typedef __intptr_t intptr_t;
#define __intptr_t_defined
#endif

#ifndef __socklen_t_defined
typedef __socklen_t socklen_t;
#define __socklen_t_defined
#endif


/* Values for the second argument to access.  */

/* Test for existence. */
#ifndef F_OK
#define F_OK	0
#endif
/* Test for execute permission.  */
#ifndef X_OK
#define X_OK	1
#endif
/* Test for write permission.  */
#ifndef W_OK
#define W_OK	2
#endif
/* Test for read permission.  */
#ifndef R_OK
#define R_OK	4
#endif

/* Test for access to name.  */
#ifndef __TARGET_SCL__
extern int access (const char *__name, int __type)
     __THROW __nonnull ((1)) __wur;
#endif

/* Values for the WHENCE argument to lseek.  */
#ifndef	__STDIO_H		/* <stdio.h> has the same definitions.  */
#define	SEEK_SET	0	/* Seek from beginning of file.  */
#define	SEEK_CUR	1	/* Seek from current position.  */
#define	SEEK_END	2	/* Seek from end of file.  */
#endif

/* Move fd's file position to offset bytes from the:
   beginning of the file (if SEEK_SET),
   the current position (if SEEK_CUR),
   or the end of the file (SEEK_END).
   Return the new file position.  */
#ifndef __USE_FILE_OFFSET64
extern __off_t lseek (int __fd, __off_t __offset, int __whence) __THROW;
#else
# ifdef __REDIRECT_NTH
extern __off64_t __REDIRECT_NTH (lseek,
				 (int __fd, __off64_t __offset, int __whence),
				 lseek64);
# else
#  define lseek lseek64
# endif
#endif
#ifdef __USE_LARGEFILE64
extern __off64_t lseek64 (int __fd, __off64_t __offset, int __whence)
     __THROW;
#endif

/* Close the file descriptor fd.  This is a cancellation point.  */
extern int close (int __fd);

/* Read nbytes into buf from fd.  This is a cancellation point.  */
extern __ssize_t read (int __fd, void *__buf, size_t __nbytes)
     __nonnull ((2)) __wur;

/* Write n bytes of buf to fd.  This is a cancellation point.  */
extern __ssize_t write (int __fd, const void *__buf, size_t __nbytes)
     __nonnull ((2)) __wur;

#ifndef __TARGET_SCL__
/* Create a one way communication channel (pipe). */
extern int pipe (int __pipedes[2]) __THROW __wur;

/* Schedule an alarm.  */
extern unsigned int alarm (unsigned int __seconds) __THROW;

/* Make the process sleep for '__seconds' seconds, or until a signal arrives
   and is not ignored.  The function returns the number of seconds less
   than '__seconds' which it actually slept (zero if it slept the full time).
   If a signal handler does a `longjmp' or modifies the handling of the
   SIGALRM signal while inside `sleep' call, the handling of the SIGALRM
   signal afterwards is undefined.  There is no return value to indicate
   error, but if `sleep' returns '__seconds', it probably didn't work.
   This is a cancellation point.  */
extern unsigned int sleep (unsigned int __seconds);

#if defined __USE_BSD || defined __USE_XOPEN_EXTENDED
/* Set an alarm to go off (generating a SIGALRM signal) in VALUE
   microseconds.  If INTERVAL is nonzero, when the alarm goes off, the
   timer is reset to go off every INTERVAL microseconds thereafter.
   Returns the number of microseconds remaining before the alarm.  */
extern __useconds_t ualarm (__useconds_t __useconds,
			    __useconds_t __interval) __THROW;

/* Make the process sleep for '__usec' microseconds, or until a signal
   arrives that is not blocked or ignored.  */
extern int usleep (__useconds_t __usec);
#endif

/* Suspend the process until a signal arrives.  This is a cancellation
   point.  */
extern int pause (void);

/* Change the owner and group of file.  */
extern int chown (const char *__file, __uid_t __owner,
		  __gid_t __group) __THROW __nonnull ((1)) __wur;

/* Change the owner and group of file.  */
extern int lchown (const char *__file, __uid_t __owner,
		  __gid_t __group) __THROW __nonnull ((1)) __wur;

/* Change the process's working directory to path.  */
extern int chdir (const char *__path) __THROW __nonnull ((1)) __wur;

#if defined __USE_BSD || defined __USE_XOPEN_EXTENDED
/* Change the owner and group of the file associated with 'fd'  */
extern int fchown (int __fd, __uid_t __owner, __gid_t __group) __THROW __wur;

/* Change the process's working directory to the path associated with 'fd'  */
extern int fchdir (int __fd) __THROW __wur;

/* The BSD version of getcwd. This function calls getcwd but does
   not provide any buffer size checking.  */
extern char *getwd (char *__buf) __THROW __nonnull ((1)) __wur;
#endif

/* Get the pathname of the current working directory.  */
extern char *getcwd (char *__buf, size_t __size) __THROW __wur;

/* Duplicate FD, returning a new file descriptor on the same file.  */
extern int dup (int __fd) __THROW __wur;

/* Duplicate fd to fd2, closing fd2 and making it open on the same file.  */
extern int dup2 (int __fd, int __fd2) __THROW;

/* NULL-terminated array of "NAME=VALUE" environment variables.  */
extern char **environ;

/* Replace the current process, executing path with args argv and
   environment envp.  */
extern int execve (const char *__path, char * const __argv[],
		   char *const __envp[]) __THROW __nonnull ((1));

/* Execute PATH with arguments ARGV and environment from `environ'.  */
extern int execv (const char *__path, char *const __argv[])
     __THROW __nonnull ((1));

/* Execute PATH with all arguments after PATH until a NULL pointer,
   and the argument after that for environment.  */
extern int execle (const char *__path, const char *__arg, ...)
     __THROW __nonnull ((1));

/* Execute PATH with all arguments after PATH until
   a NULL pointer and environment from `environ'.  */
extern int execl (const char *__path, const char *__arg, ...)
     __THROW __nonnull ((1));

/* Execute FILE, searching in the `PATH' environment variable if it contains
   no slashes, with arguments ARGV and environment from `environ'.  */
extern int execvp (const char *__file, char *const __argv[])
     __THROW __nonnull ((1));

/* Execute FILE, searching in the `PATH' environment variable if
   it contains no slashes, with all arguments after FILE until a
   NULL pointer and environment from `environ'.  */
extern int execlp (const char *__file, const char *__arg, ...)
     __THROW __nonnull ((1));
#endif /* __TARGET_SCL__ */

/* Terminate program execution with the low-order 8 bits of status.  */
extern void _exit (int __status) __attribute__ ((__noreturn__));

#ifndef __TARGET_SCL__
/* Values for the NAME argument to `pathconf' and `fpathconf'.  */
enum
  {
    _PC_LINK_MAX,
    _PC_MAX_CANON,
    _PC_MAX_INPUT,
    _PC_NAME_MAX,
    _PC_PATH_MAX,
    _PC_PIPE_BUF,
    _PC_CHOWN_RESTRICTED,
    _PC_NO_TRUNC,
    _PC_VDISABLE
  };

/* Get file-specific configuration information about PATH.  */
extern long int pathconf (const char *__path, int __name)
     __THROW __nonnull ((1));

/* Get file-specific configuration about descriptor FD.  */
extern long int fpathconf (int __fd, int __name) __THROW;

/* Values for the argument to `sysconf'.  */
enum
  {
    _SC_ARG_MAX,
    _SC_CHILD_MAX,
    _SC_CLK_TCK,
    _SC_NGROUPS_MAX,
    _SC_OPEN_MAX,
    _SC_STREAM_MAX,
    _SC_TZNAME_MAX,
    _SC_JOB_CONTROL,
    _SC_SAVED_IDS,
    _SC_VERSION,
    _SC_PAGESIZE,

    /* Values for the argument to `sysconf' corresponding to
       _POSIX2_* symbols.  */
    _SC_BC_BASE_MAX,
    _SC_BC_DIM_MAX,
    _SC_BC_SCALE_MAX,
    _SC_BC_STRING_MAX,
    _SC_COLL_WEIGHTS_MAX,
    _SC_EQUIV_CLASS_MAX,
    _SC_EXPR_NEST_MAX,
    _SC_LINE_MAX,
    _SC_RE_DUP_MAX,

    _SC_2_VERSION,
    _SC_2_C_BIND,
    _SC_2_C_DEV,
    _SC_2_FORT_DEV,
    _SC_2_FORT_RUN,
    _SC_2_SW_DEV,
    _SC_2_LOCALEDEF,

    /* The assignment of _SC_PAGE_SIZE restarts the numbering, so anything
       after it shares a value with an earlier name: _SC_PHYS_PAGES used to
       equal _SC_BC_BASE_MAX and _SC_NPROCESSORS_ONLN _SC_BC_DIM_MAX, and
       sysconf (_SC_NPROCESSORS_ONLN) returned 2048.  Keep it last.  */
    _SC_PHYS_PAGES,
    _SC_NPROCESSORS_ONLN,

    _SC_PAGE_SIZE = _SC_PAGESIZE
  };
#define _SC_PHYS_PAGES _SC_PHYS_PAGES
#define _SC_NPROCESSORS_ONLN _SC_NPROCESSORS_ONLN

/* Get the value of the system variable NAME.  */
extern long int sysconf (int __name) __THROW __attribute__ ((__const__));


/* Get the process id of the calling process.  */
extern __pid_t getpid (void) __THROW;

/* Get the process id of the calling process's parent.  */
extern __pid_t getppid (void) __THROW;

/* Get the process group id of the calling process.  */
extern __pid_t getpgrp (void) __THROW;

/* Set the process group id of the process matching pid to pgid.  */
extern int setpgrp (__pid_t __pid, __pid_t __pgid) __THROW;
extern int setpgid (__pid_t __pid, __pid_t __pgid) __THROW;

/* Create a new session with the calling process as its leader.  */
extern __pid_t setsid (void) __THROW;

/* Get the real user id of the calling process.  */
extern __uid_t getuid (void) __THROW;
/* Get the effective user id of the calling process.  */
extern __uid_t geteuid (void) __THROW;
/* Get the real group id of the calling process.  */
extern __gid_t getgid (void) __THROW;
/* Get the effective group id of the calling process.  */
extern __gid_t getegid (void) __THROW;

#if 0
/* Return the number of supplementary groups the calling process is in.  */
extern int getgroups (int __size, __gid_t __list[]) __THROW;
#endif

/* Set the user id of the calling process to uid.  */
extern int setuid (__uid_t __uid) __THROW;
/* Set the effective user id of the calling process to uid (from BSD).  */
extern int seteuid (__uid_t __uid) __THROW;
#if 0
/* Combination of setuid and seteuid (from BSD).  */
extern int setreuid (__uid_t __ruid, __uid_t __euid) __THROW;
#endif

/* Set the group id of the calling process to uid.  */
extern int setgid (__gid_t __gid) __THROW;
/* Set the effective group id of the calling process to uid (from BSD).  */
extern int setegid (__gid_t __gid) __THROW;
#if 0
/* Combination of setgid and setegid (from BSD).  */
extern int setregid (__gid_t __rgid, __gid_t __egid) __THROW;
#endif

/* Clone the calling process, creating an exact copy.  */
extern __pid_t fork (void) __THROW __wur;
/* Clone the calling process, but without copying the whole address
   space (from BSD).  */
extern __pid_t vfork (void) __THROW __wur;

/* Return the pathname of the terminal fd is open on, or NULL on errors.
   The returned storage is good only until the next call to this function.  */
extern char *ttyname (int __fd) __THROW;

/* Store at most buflen characters of the pathname of the terminal fd is
   open on in buf.  Return 0 on success, otherwise an error number.  */
extern int ttyname_r (int __fd, char *__buf, size_t __buflen)
     __THROW __nonnull ((2)) __wur;
#endif /* __TARGET_SCL__ */

/* Return 1 if fd is a valid descriptor associated with a terminal.  */
extern int isatty (int __fd) __THROW __wur;

#ifndef __TARGET_SCL__
/* Return 1 if fd is a valid descriptor associated with a pipe.  */
extern int ispipe (int __fd) __THROW __wur;

/* Make a link to from named to.  */
extern int link (const char *__from, const char *__to)
     __THROW __nonnull ((1, 2)) __wur;

/* Make a symbolic to from named to.  */
extern int symlink (const char *__from, const char *__to)
     __THROW __nonnull ((1, 2)) __wur;

/* Read vaue of a symbolic link.  */
extern ssize_t readlink (const char *__restrict __path, char *__restrict __buf,
			 size_t __butsiz) __THROW __nonnull ((1, 2)) __wur;
#endif /* __TARGET_SCL__ */

/* Remove the line name.  */
extern int unlink (const char *__name) __THROW __nonnull ((1));

#ifndef __TARGET_SCL__
/* Remove the directory path.  */
extern int rmdir (const char *__path) __THROW __nonnull ((1));

#define L_cuserid 16

/* Return the login name of the user.  This is a cancellation point.  */
extern char *getlogin (void);

/* Set the foreground process group ID of FD set PGRP_ID.  */
extern int tcsetpgrp (int __fd, __pid_t __pgrp_id) __THROW;

/* Return the foreground process group ID of FD.  */
extern __pid_t tcgetpgrp (int __fd) __THROW;


#if 0
/* Set the login name returned by `getlogin'.  */
extern int setlogin (const char *__name) __THROW;

/* Invoke system call number 'sysno', passing it the remaining args.  */
extern int syscall (int __sysno, ...) __THROW;
#endif

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED

/* Set the end of accessible data space (aka "the break") to ADDR.
   Returns zero on success and -1 for errors (with errno set).  */
extern int brk (void *__addr) __THROW __nonnull ((1)) __wur;

/* Increase or decrease the end of accessible data space by DELTA bytes.
   If successful, returns the address the previous end of data space
   (i.e. the beginning of the new space, if DELTA > 0);
   returns (void *) -1 for errors (with errno set).  */
extern void *sbrk (intptr_t __delta) __THROW;
#endif

#if (defined __USE_MISC || defined __USE_XOPEN_EXTENDED) && !defined F_LOCK
/* NOTE: These declarations also appear in <fcntl.h>; be sure to keep both
   files consistent.  Some systems have them there and some here, and some
   software depends on the macros being defined without including both.  */

/* `lockf' is a simpler interface to the locking facilities of `fcntl'.
   LEN is always relative to the current file position.
   The CMD argument is one of the following.

   This function is a cancellation point and therefore not marked with
   __THROW.  */

# define F_ULOCK 0	/* Unlock a previously locked region.  */
# define F_LOCK  1	/* Lock a region for exclusive use.  */
# define F_TLOCK 2	/* Test and lock a region for exclusive use.  */
# define F_TEST  3	/* Test a region for other processes locks.  */

# ifndef __USE_FILE_OFFSET64
extern int lockf (int __fd, int __cmd, __off_t __len);
# else
#  ifdef __REDIRECT
extern int __REDIRECT (lockf, (int __fd, int __cmd, __off64_t __len),
		       lockf64);
#  else
#   define lockf lockf64
#  endif
# endif
# ifdef __USE_LARGEFILE64
extern int lockf64 (int __fd, int __cmd, __off64_t __len);
# endif
#endif /* Use misc and F_LOCK not already defined.  */
#endif /* __TARGET_SCL__ */


#ifdef __USE_GNU

/* Evaluate EXPRESSION, and repeat as long as it returns -1 with `errno'
   set to EINTR.  */

# define TEMP_FAILURE_RETRY(expression) \
  (__extension__							      \
    ({ long int __result;						      \
       do __result = (long int) (expression);				      \
       while (__result == -1L && errno == EINTR);			      \
       __result; }))
#endif


#if defined __USE_BSD || defined __USE_UNIX98
/* Put the name of the current host in no more than LEN bytes of NAME.
   The result is null-terminated if LEN is large enough for the full
   name and the terminator.  */
extern int gethostname (char *__name, size_t __len) __THROW __nonnull ((1));
#endif


#if defined __USE_BSD || (defined __USE_XOPEN && !defined __USE_UNIX98)
/* Set the name of the current host to NAME, which is LEN bytes long.
   This call is restricted to the super-user.  */
extern int sethostname (__const char *__name, size_t __len) __THROW;

#if 0
/* Set the current machine's Internet number to ID.
   This call is restricted to the super-user.  */
extern int sethostid (long int __id) __THROW;
#endif


/* Get and set the NIS (aka YP) domain name, if any.
   Called just like `gethostname' and `sethostname'.
   The NIS domain name is usually the empty string when not using NIS.  */
extern int getdomainname (char *__name, size_t __len) __THROW;
extern int setdomainname (__const char *__name, size_t __len) __THROW;


#if 0
/* Revoke access permissions to all processes currently communicating
   with the control terminal, and then send a SIGHUP signal to the process
   group of the control terminal.  */
extern int vhangup (void) __THROW;
#endif

#if 0
/* Revoke the access of all descriptors currently open on FILE.  */
extern int revoke (__const char *__file) __THROW;
#endif


#ifndef __TARGET_SCL__
/* Enable statistical profiling, writing samples of the PC into at most
   SIZE bytes of SAMPLE_BUFFER; every processor clock tick while profiling
   is enabled, the system examines the user PC and increments
   SAMPLE_BUFFER[((PC - OFFSET) / 2) * SCALE / 65536].  If SCALE is zero,
   disable profiling.  Returns zero on success, -1 on error.  */
extern int __profil (unsigned short int *__sample_buffer, size_t __size,
		     size_t __offset, unsigned int __scale) __THROW;
#endif


#if 0
/* Turn accounting on if NAME is an existing file.  The system will then write
   a record for each process as it terminates, to this file.  If NAME is NULL,
   turn accounting off.  This call is restricted to the super-user.  */
extern int acct (__const char *__name) __THROW;
#endif


#if 0
/* Successive calls return the shells listed in `/etc/shells'.  */
extern char *getusershell (void) __THROW;
extern void endusershell (void) __THROW; /* Discard cached info.  */
extern void setusershell (void) __THROW; /* Rewind and re-read the file.  */
#endif


#if 0
/* Put the program in the background, and dissociate from the controlling
   terminal.  If NOCHDIR is zero, do `chdir ("/")'.  If NOCLOSE is zero,
   redirects stdin, stdout, and stderr to /dev/null.  */
extern int daemon (int __nochdir, int __noclose) __THROW;
#endif
#endif /* Use BSD || X/Open.  */


#ifndef __TARGET_SCL__
/* Make all changes done to all files actually appear on disk.  */
extern void sync (void) __THROW;

/* Make all changes done to FD actually appear on disk.
   This function is a cancellation point.  */
extern int fsync (int __fd);

/* Truncate FILE to LENGTH bytes.  */
extern int truncate (const char *__file, __off_t __length) __THROW;

/* Truncate the file FD is open on to LENGTH bytes.  */
extern int ftruncate (int __fd, __off_t __length) __THROW;

/* Return the number of bytes in a page.  This is the system's page size,
   which is not necessarily the same as the hardware page size.  */
extern int getpagesize (void) __THROW __attribute__ ((__const__));

/* Return the maximum number of file descriptors
   the current process could possibly have.  */
extern int getdtablesize (void) __THROW;

#if 0
/* Return the current machine's Internet number.  */
extern long int gethostid (void) __THROW;

/* Set the current machine's Internet number to ID.
   This call is restricted to the super-user.  */
extern int sethostid (long int __id) __THROW;


/* Make the block special device PATH available to the system for swapping.
   This call is restricted to the super-user.  */
extern int swapon (const char *__path) __THROW;

/* Reboot or halt the system.  */
extern int reboot (int __howto) __THROW;

#endif

#if defined __USE_BSD || (defined __USE_XOPEN && !defined __USE_XOPEN2K)
/* Make PATH be the root directory (the starting point for absolute paths).
   This call is restricted to the super-user.  */
extern int chroot (__const char *__path) __THROW;

/* Prompt with PROMPT and read a string from the terminal without echoing.
   Uses /dev/tty if possible; otherwise stderr and stdin.  */
extern char *getpass (__const char *__prompt) __THROW;
#endif /* Use BSD || X/Open.  */


/* POSIX 2 extensions.  */


/* Values for the NAME argument to `confstr'.  */
enum
  {
    _CS_PATH			/* The default search path.  */
  };

/* Get the value of the string-valued system variable NAME.  */
extern size_t confstr (int __name, char *__buf, size_t __len) __THROW;

#ifdef __USE_XOPEN
/* Encrypt at most 8 characters from KEY using salt to perturb DES.  */
extern char *crypt (const char *__key, const char *__salt)
     __THROW __nonnull ((1, 2));

/* Encrypt data in BLOCK in place if EDFLAG is zero; otherwise decrypt
   block in place.  */
extern void encrypt (char *__block, int __edflag) __THROW __nonnull ((1));

#ifndef __TARGET_SCL__
/* Swab pairs bytes in the first N bytes of the area pointed to by
   FROM and copy the result to TO.  The value of TO must not be in the
   range [FROM - N + 1, FROM - 1].  If N is odd the first byte in FROM
   is without partner.  */
extern void swab (const void *__restrict  __from, void *__restrict  __to,
		  ssize_t __n) __THROW __nonnull ((1, 2));
#endif
#endif

#if defined __USE_MISC || defined __USE_XOPEN
/* Alter the priority of the current process by 'increment'.  */
extern int nice (int __increment) __THROW __wur;
#endif
#endif /* __TARGET_SCL__ */

#define __need_getopt
#include <getopt.h>

__END_DECLS

#endif
