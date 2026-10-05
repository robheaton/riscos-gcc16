/* Structures for UnixLib's internal process management.
   Copyright (c) 2002-2013 UnixLib Developers.  */

#ifndef __UNIXLIB_UNIX_H
#define __UNIXLIB_UNIX_H 1

#ifndef __SETJMP_H
#include <setjmp.h>
#endif
#ifndef __SYS_TIME_H
#include <sys/time.h>
#endif
#ifndef __SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifndef __SYS_TYPES_H
#include <sys/types.h>
#endif
#ifndef __LIMITS_H
#include <limits.h>
#endif
#ifndef __KERNEL_H
#include <kernel.h>
#endif

#ifndef __UNIXLIB_FEATURES_H
#include <features.h>
#endif
#ifndef __UNIXLIB_TYPES_H
#include <unixlib/types.h>
#endif

#ifndef __INTERNAL_FD_H
#include <internal/fd.h>
#endif
#ifndef __INTERNAL_SIGSTATE_H
#include <internal/sigstate.h>
#endif
#ifndef __INTERNAL_TTY_H
#include <internal/tty.h>
#endif

__BEGIN_DECLS

struct __process
{
  /* The return code of this process.  */
  unsigned int return_code : 8;
  /* The number of the signal we terminated with.  */
  unsigned int signal : 8;
  /* This process terminated through a signal handler.  */
  unsigned int signal_exit : 1;
  /* This process terminated with a core dump.  */
  unsigned int core_dump : 1;
  /* This process just stopped.  */
  unsigned int stopped : 1;
  /* This process has exited, but has not been reaped by its parent.  */
  unsigned int zombie : 1;
  /* This process has already been reported to wait() whilst it is stopped.   */
  unsigned int reported : 1;
  /* This process was the result of a fork().  */
  unsigned int forked : 1;
  /* This process was the result of exec().  */
  unsigned int execed : 1;
  /* This process is dynamically linked.  */
  unsigned int is_dynamic : 1;
  /* This process is armeabi.  */
  unsigned int is_armeabi : 1;
};

/* This structure must be kept in perfect synchronisation with
   PROC_* definitions in incl-local/internal/asm_dec.s.  */
struct proc
{
  int argc;  /* Command line argument count.  */
  char **argv;  /* Command line arguments.  */
  struct itimerval itimers[__MAX_ITIMERS];  /* Defined interval timers.  */
  struct rlimit limit[RLIMIT_NLIMITS];  /* Process resource limits.  */
  struct rusage usage;  /* Resource usage of the process.  */
  /* The signal states for this process. */
  struct unixlib_sigstate sigstate;
  /* Set to 1 if this process is currently sleeping or stopped.
     These could change at any time.  */
  volatile int sleeping, stopped;
  /* DDEUtils_Prefix value at the beginning of this process.  */
  const char *dde_prefix;
};

/* Beware of backwards compatibility when altering this structure.
   It is referenced by SUL and other processes.  */
struct __sul_process
{
  int reserved1; /* Reserved for use by SUL */
  int reserved2;
  void *(*sul_malloc)(__pid_t pid, size_t size);
  void (*sul_free)(__pid_t pid, void *ptr);
  int (*sul_exit) (__pid_t pid, int status) __attribute__ ((__noreturn__));
  __pid_t pid;   /* Process ID. */
  __pid_t ppid;  /* Parent process ID. */
  __pid_t pgrp;  /* ID of process group that process belongs to.  */
  __uid_t uid;   /* Real UID.  */
  __uid_t euid;  /* Effective UID.  */
  __gid_t gid;   /* Real GID.  */
  __gid_t egid;  /* Effective GID.  */
  int ppri;  /* Process priority.  */
  int gpri;  /* Process group priority.  */
  int upri;  /* User process priority.  */
  __mode_t umask;   /* File creation mode mask.  */
  void *file_descriptors;
  unsigned int maxfd;  /* Number of entries in the file_descriptors array */
  unsigned int fdsize; /* Size of each entry in the file_descriptors array */
  unsigned int fdhandlesize; /* Size of each fdhandle */
  int tty_type;
  struct tty *console;
  struct tty *rs423;
  char **environ;
  struct __process status;
  struct __sul_process *children;   /* Linked list of all children of this process */
  struct __sul_process *next_child;
  __pid_t (*sul_fork) (__pid_t pid, struct __sul_process **proc, void *stacklimit, void *stack);
  unsigned int environ_size;
  void (*sul_exec) (__pid_t pid, char *cli, void *stacklimit, void *stack) __attribute__ ((__noreturn__));
  void *(*sul_wimpslot) (__pid_t pid, void *newslot);
  void *stack_handle; /* Handle of the main stack used by this process, or 0 if not ARMEABI.  */
};

extern struct proc *__u;	/* current process */

/* This structure must be kept in perfect synchronisation with GBL_*
   definitions in incl-local/internal/asm_dec.s.

   Functions wishing to use this structure can reduce the number of
   load/store operations by defining a local-variable i.e.

      void foo (void)
      {
        struct ul_global *gbl = &__ul_global;
        ...
      }

   Otherwise the compiler will force a lookup on __ul_global for each
   access.  */

struct __pthread_callevery_block;

struct ul_global
{
  const char *cli; /* CLI from OS_GetEnv */
  unsigned int time[2]; /* low word, high byte */
  __off_t rewindpipeoffset;
  int taskwindow; /* Non-zero if executing in a TaskWindow.  This value is
    always up-to-date as we can't become a TaskWindow task if we aren't one
    at startup. So no need for __get_task_window () something similar.  */
  int taskhandle; /* WIMP task handle, or zero if non WIMP task.
    Note that when ul_global::taskwindow == 1 => ul_global::taskhandle != 0
    but not necessary vice-versa.
    Also, if this value is zero it could be that Wimp_InitialiseTask has been
    called since last check so you have to check if this value is still
    up-to-date.  Use __get_taskhandle () to read this value.  */
  int dynamic_num; /* -1 is no DA is used, i.e. heap in application memory.
    Otherwise the DA number used to store our heap.  */
  int changeredir0; /* RISC OS IN process handle before we overrule this
    and exec another binary.  */
  int changeredir1; /* RISC OS OUT process handle before we overrule this
    and exec another binary.  */
  const char *ul_out_pathname; /* When non-NULL, canonicalised pathname of
    UnixLib stdout handle.  Workaround for RISC OS 4/5 bug.  */
  int panic_mode; /* Non-zero when we're panicing.  */
  struct __sul_process *sulproc; /* Process structure returned from SUL.  */
  int pagesize; /* System page size.  */
  void *upcall_handler_addr;
  void *upcall_handler_r12;

  void *pthread_return_address;
  volatile int pthread_worksemaphore_POISONED; /* Zero if the context switcher is
    allowed to switch threads.  */
  volatile int pthread_callback_semaphore_POISONED; /* Prevent a callback being set
    whilst servicing another callback.  */
  int pthread_system_running; /* Global initialisation flag.  UnixLib
    internally uses this to test whether or not to use mutexes for locking
    critical structures.  */
  int pthread_callback_missed; /* Non zero if a callback occured when context
    switching was temporarily disabled.  */
  int pthread_num_running_threads; /* Number of running threads.  */

  int executing_signalhandler; /* Non-zero if we are currently executing a
    signal handler.  */
  void *signalhandler_sl; /* Stack limit for signal handlers.  */
  void *signalhandler_sp; /* Stack pointer for signal handlers.  */

  char **last_environ; /* Non-NULL when we allocated the current environment
    ourselves. Is NULL when we've inherited the environment from our
    parent.  */
  void *malloc_state; /* The global malloc state (opaque type).  */
  int escape_disabled; /* Non-zero if the escape key was disabled on program
    initialisation.  */
  unsigned int fls_lbstm_on_rd; /* When non-zero, flush all line buffered
    stdio stream using __flslbbuf when reading from a stdio stream attached
    to a tty.  */
  struct __pthread_callevery_block *pthread_callevery_rma; /* Pointer to a
    block of RMA that contains several values which require quick and
    easy access from the machine code routines.
    This block is passed to the call_every handler in r12, for example.  */

  unsigned int cpu_flags; /* Flags indicating the capabilities of the CPU.  */

#if !defined(__SOFTFP__) && defined(__VFP_FP__)
  int vfp_regcount; /* Number of doubleword registers we should request for
    VFP contexts */
#endif
};

/* This structure must be kept in perfect synchronisation with:

     a) MEM_* definitions in incl-local/internal/asm_dec.s
     b) The __ul_memory structure at the end of sys/_syslib.s

   Functions wishing to use this structure can reduce the number of
   load/store operations by defining a local-variable i.e.

      void foo (void)
      {
        struct ul_memory *mem = &__ul_memory;
        ...
      }
 */
struct ul_memory
{
  /* Serialise access to this structure (opaque type).  */
  void *mutex;

  /* This variable sets the value of what is considered to be the
     maximum value of addressable memory within application space.
     This value can increase during program execution if there is a
     request for more memory.

     The value of 'appspace_himem' will always be less-than or equal-to
     the value of 'appspace_limit'.

     Note that when dynamic areas are in use, the only function that will
     try to increase 'appspace_himem' and 'appspace_limit' is the stack
     extension code.  */
  unsigned int appspace_himem;

  /* Current lowest position of the stack.  It lowers in memory address
     until it reaches 'stack_limit'.  Once reached, attempts to
     increase stack size will happen by increasing the wimpslot.  */
  unsigned int stack;

  /* This points to the start of application memory, usually 0x8000.
     Defined as Image$$RO$$Base for AOF.
     Defined as __executable_start for ELF.  */
  const unsigned int robase;

  /* This points to the end of the executable, including any read/write
     data that has been initialised before program start.

     It is called 'rwlomem' because it refers to the lowest memory address
     that is usable for run-time allocation of memory.

     Defined as Image$$RW$$Limit for AOF.
     Defined as __end__ for ELF.  */
  const unsigned int rwlomem;

  /* This points to the start of read/write data.
     Defined as Image$$RW$$Base for AOF.
     Defined as __data_start for ELF.  */
  const unsigned int rwbase;

  /* As data is requested from the system by 'brk' to satisfy allocation
     and deallocation requests and we're not using a dynamic area to store
     our heap, the value of 'rwbreak' will change.

     Initially it starts off equal to 'rwlomem', meaning that no data
     has been allocated at run-time.

     When a request is made for more space, then 'rwbreak' will increase.  */
  unsigned int rwbreak;

  /* This variable points to the lowest extent to which the stack can
     grow.  Its value increases in line with 'rwbreak' as more memory
     is taken from the system.  */
  unsigned int stack_limit;

  /* Holds the base memory address of the dynamic area.  It is used to
     ensure that any requests by 'brk' or 'sbrk' to change the data segment
     size, cannot reduce it past this value.

     This variable is used only when dynamic areas are in use.  */
  unsigned int dalomem;

  /* This variable is used only when dynamic areas are in use.  It records
     the used space in a dynamic area, which will increase or decrease
     as required according to the user's calls to 'brk'.  */
  unsigned int dabreak;

  /* This variable is used only when dynamic areas are in use.  It records
     the upper limit of the dynamic area and will increase/decrease as
     we claim or release memory.  */
  unsigned int dalimit;

  /* This variable is used by the non-contiguous stack chunk code.
     It records the upper limit of application space (or wimpslot)
     and will increase as more is required in multiples of 32K.  In such
     circumstances, this means that 'appspace_limit' will be larger than
     'appspace_himem'.  */
  unsigned int appspace_limit;

  /* Value of 'appspace_himem' last time the stack was increased.  */
  unsigned int old_himem;

  /* The permitted RAM limit that the program was started with, when that is
     below the application space limit (-1 when it is not).  'appspace_himem'
     is never raised above it: the memory between the two is not spare.  A
     program that was started by vfork () + exec () is told by
     SharedUnixLibrary to stay below the copy of its parent, which
     SharedUnixLibrary keeps between that limit and the end of the Wimp slot.
     Offset 52 (MEM_APPSPACE_HIMEM_MAX in asm_dec.s).  */
  unsigned int appspace_himem_max;
};

extern struct ul_memory __ul_memory;

extern struct ul_global __ul_global;

/* Returns __ul_global.taskhandle and makes sure that this value is still
   up-to-date.  */
extern int __get_taskhandle (void);

extern int __funcall_error (const char *, int, unsigned int);
#if __UNIXLIB_PARANOID
#ifdef PIC
/* In the shared library, function addresses can and will exist outside
   application space.  */
#define __funcall(f, p) \
  ((((unsigned int)(f) >= __ul_memory.robase) && !((unsigned int)(f) & 3) \
   ? 0 : __funcall_error(__FILE__, __LINE__, (unsigned int)(f))), (f)p)
#else
#define __funcall(f, p) \
  ((((unsigned int)(f) >= __ul_memory.robase) && !((unsigned int)(f) & 3) && (unsigned int)(f) < __ul_memory.appspace_himem \
   ? 0 : __funcall_error(__FILE__, __LINE__, (unsigned int)(f))), (f)p)
#endif
#else
#define __funcall(f, p) ((f)p)
#endif

extern unsigned int __dynamic_area_refcount;
extern void __dynamic_area_exit (void);

extern void __munmap_all (void);	/* Deallocate all mappings.  */

/* Stop the interval timers.  */
extern void __stop_itimers (void);

/* OS_ChangeEnvironment is used to set up exception handlers. These
   handlers use OS_CallBack to raise signals in the foreground process.
   RTFM (RISC OS PRM - 'The Program Environment') for more info. */

/* Reset handlers, etc. back to original state.  */
extern void __env_riscos (void);
extern void __env_unixlib (void);

/* Return unsigned int from STRING.  END, if non-null, points to character
   after number.  */
extern unsigned int __decstrtoui (const char *__string, const char **__end)
	__nonnull ((1));

/* UnixLib's prefix for global RISC OS environment variables.  */
#define __UNIX_ENV_PREFIX "UnixEnv$"

/* Add NAME=VALUE to the environment. If NAME is already in the environment,
   only add when replace is non-zero.  If string is non-NULL then it is
   used directly, otherwise a copy of name and value is used. */
extern int __addenv_to_env (char *string, const char *__name,
                            const char *__value, int __replace);

/* Get environment value from OS and copy into BUF and return a pointer to BUF.
   If no match found then return NULL.  If BUF is NULL, then allocate space
   as necessary, otherwise BUFLEN is length of supplied buffer.
   If buf is NULL and call is not NULL then an enumeration is taking place.  */
extern char *__getenv_from_os (const char *__name, char *__buf,
			       size_t __buflen, char **call)
	__nonnull ((1)) __wur;

/* Set runtime features according to system variables.  */
extern void __runtime_features (const char *__cmdline)
	__nonnull ((1));

/* Print an error and exit the process. When MESSAGE is NULL, the printed
   error message is based on the current errno value.  */
extern void __unixlib_fatal (const char *__message)
	__attribute__ ((__noreturn__));

/* Atomically increment or decrement a word of memory */
extern unsigned int __atomic_modify (unsigned int *addr, int incr)
	__nonnull ((1));

/* Initialize the signal code.  */
extern void __unixlib_signal_initialise (struct proc *__p)
	__nonnull ((1));

/* Resource limit initialisation */
extern void __resource_initialise (struct proc *__p)
	__nonnull ((1));

#if __UNIXLIB_CHUNKED_STACK
/* Stack manipulation */

#define STACK_CHUNK_MAGIC 0xf60690ff

/* Structure representing the bottom part of a stack chunk */
/* See _syslib.s for more details */
struct __stack_chunk
{
  unsigned int magic;
  struct __stack_chunk *next;
  struct __stack_chunk *prev;
  unsigned int size;
  void (*dealloc) (void *);
  void (*returnaddr) (void);
};

extern void __stackalloc_init (void);
extern void *__stackalloc (size_t __size) __wur;
extern void __stackfree (void *__ptr);
extern int __stackalloc_trim (void);
extern void __free_stack_chain (void *__ptr) __nonnull ((1));
#endif
extern unsigned int __stackalloc_incr_wimpslot (unsigned int __incr) __wur;

/* Initialise the UnixLib world.  */
extern void __unixinit (void);

/* Free any remaining memory and file descriptors associated with a
   process.  */
extern void __free_process (struct __sul_process *process)
	__nonnull ((1));

extern pid_t __fork_post (pid_t pid, int isfork);
extern int __fork_pre (int isfork, void **sul_fork, pid_t *pid);

extern unsigned int __get_cpu_arch (void);

__END_DECLS

#endif
