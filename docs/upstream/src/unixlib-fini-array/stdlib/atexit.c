/* atexit (), __cxa_atexit (), __cxa_finalize ()
 * Copyright (c) 2000-2015 UnixLib Developers
 * 
 * For __TARGET_SCL__ we don't have access to the atexit mechanism. Here
 * we maintain our own list of finalizer functions and use the SCL atexit
 * to ensure that they are all called at program exit.
 * For __TARGET_UNIXLIB__, this is the atexit mechanism.
 */

#include <errno.h>
#include <stdlib.h>

#ifndef __TARGET_SCL__
#include <pthread.h>
#include <internal/unix.h>
#endif

#include <internal/linklist.h>

typedef struct atexit_entry
{
  __link_header link;

  void (*func)(void *);
  void *arg;
  void *dso_handle;
  unsigned called : 1;
} atexit_entry;

static __link_list atexit_function_list;

#ifdef __TARGET_SCL__
static void
__scl_finalize (void)
{
  __cxa_finalize (NULL);
}
#endif

int
__cxa_atexit(void (*destructor)(void *),
	     void *arg,
	     void *dso_handle)
{
#ifdef __TARGET_SCL__
  static int __scl_register_finalize;

  if (!__scl_register_finalize)
    {
      atexit (__scl_finalize);
      __scl_register_finalize = 1;
    }
#else
  PTHREAD_UNSAFE
#endif

  if (destructor == NULL)
    return __set_errno (EINVAL);

  atexit_entry *entry = malloc (sizeof (atexit_entry));
  if (entry == NULL)
    return __set_errno (ENOMEM);

  __linklist_init_link (&entry->link);
  __linklist_add_to_front (&atexit_function_list, &entry->link);

  entry->func = destructor;
  entry->arg = arg;
  entry->dso_handle = dso_handle;
  entry->called = 0;

  return 0;
}

/* When __cxa_finalize(d) is called, it shall walk the termination function list,
 * calling each in turn if d matches the handle of the termination function entry.
 * If d is NULL, it shall call all the termination funtions.
 */
void
__cxa_finalize(void *dso_handle)
{
#ifndef __TARGET_SCL__
  PTHREAD_UNSAFE
#endif

  atexit_entry *entry = (atexit_entry *)__linklist_first (&atexit_function_list);

  /* __cxa_atexit adds handlers to the front of the list, so running through
   * it forwards here means that we process the handlers in reverse order as
   * the standard requires.
   * Rather than remove the entries once called, they are simply marked as
   * finished. __cxa_finalize can be called recursively, so removing links
   * from the list can be problematic (the next entry may no longer exist).
   * As __cxa_finalize is called at program exit, the memory loss shouldn't
   * be an issue.
   */
  while (entry)
    {
      atexit_entry *next = (atexit_entry *)__linklist_next (&entry->link);
      if (!entry->called && (dso_handle == NULL || dso_handle == entry->dso_handle))
        {
	  entry->called = 1;
#ifndef __TARGET_SCL__
	  __funcall ((*entry->func), (entry->arg));
#else
	  entry->func (entry->arg);
#endif
	}
      entry = next;
    }
}

#ifndef __TARGET_SCL__
int
atexit (void (*atexit_function) (void))
{
  return __cxa_atexit ((void (*) (void *))atexit_function, NULL, NULL);
}
#endif

#ifndef __TARGET_SCL__
/* The program's .fini_array (EABI).  crt0 hands the bounds of the array to the runtime (CRT1_FINI_ARRAY_START / _END) and __main calls
   __register_fini_array before the program's .init_array functions run, so that the functions of the array are called at exit AFTER the atexit () functions
   and the static destructors that the constructors register, last entry first: what glibc's __libc_csu_fini does.  Before, nothing called them: an
   __attribute__ ((destructor)) function of a C program never ran, and neither did the exit hook of libgcov (no .gcda file was ever written).  */
static void (**fini_array_start) (void);
static void (**fini_array_end) (void);

static void
__run_fini_array (void)
{
  void (**p) (void) = fini_array_end;

  while (p > fini_array_start)
    {
      void (*fn) (void) = *--p;

      if (fn != NULL && fn != (void (*) (void)) -1)
	fn ();
    }
}

void
__register_fini_array (void (**start) (void), void (**end) (void))
{
  if (start == NULL || start == end)
    return;

  fini_array_start = start;
  fini_array_end = end;
  atexit (__run_fini_array);
}
#endif
