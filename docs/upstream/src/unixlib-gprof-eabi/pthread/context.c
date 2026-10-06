/* Context switching/scheduling.
   Written by Martin Piper and Alex Waugh.
   Copyright (c) 2002-2010 UnixLib Developers.  */

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <malloc.h>
#include <swis.h>

#include <internal/os.h>
#include <internal/unix.h>
#include <internal/swiparams.h>

/* #define PTHREAD_DEBUG
#define PTHREAD_DEBUG_CONTEXT

#define DEBUG
#include <sys/debug.h> */

pthread_t __pthread_thread_list = NULL; /* Linked list of all threads */
pthread_t __pthread_running_thread = NULL; /* Currently running thread */
/* The thread that the scheduler last took the processor from (the one that the tick interrupted): what the sampler of gprof (gmon/profil.c) looks at.  */
pthread_t __pthread_prev_running_thread attribute_hidden = NULL;

/* De-allocates an idle node from the list.  */
static void
__pthread_cleanup_idle (pthread_t node)
{
  struct ul_global *gbl = &__ul_global;
  if (node == NULL)
    return;

  if (node->state != STATE_IDLE)
    __pthread_fatal_error ("-- __pthread_cleanup_idle: Not an idle thread!");

#ifdef PTHREAD_DEBUG
  debug_printf ("-- __pthread_cleanup_idle: Marking as STATE_UNALLOCED idle thread node %08x from linked list\n", node);
#endif
  node->state = STATE_UNALLOCED;

  if (node->saved_context)
    {
#if !defined(__SOFTFP__) && defined(__VFP_FP__)
      int context = node->saved_context->vfpcontext;
      /* Paranoia - just in case the context is active */
      _swix(VFPSupport_DestroyContext, _INR(0, 1), context & ~1, 0);
      /* Free the memory if it wasn't stack allocated */
      if (!(context & 1))
        free_unlocked (gbl->malloc_state, (void *)context);
#endif
      free_unlocked (gbl->malloc_state, node->saved_context);
      node->saved_context = NULL;
    }

  if (node->stack)
    {
#if __UNIXLIB_CHUNKED_STACK
      __free_stack_chain (node->stack);
#else
      _swix(ARMEABISupport_StackOp, _INR(0,1), ARMEABISUPPORT_STACKOP_FREE, node->stack);
#endif
      node->stack = NULL;
    }

  /* Free the node if it is detached.  If it is not detached, then we
     cannot do anything until another thread joins with it.  */
  if (node->detachstate == PTHREAD_CREATE_DETACHED)
    {
      node->magic = 0; /* Invalidate magic number */
      free_unlocked (gbl->malloc_state, node);
    }
#ifdef PTHREAD_DEBUG
  debug_printf ("-- __pthread_cleanup_idle: complete\n");
#endif
}

/* Round robin scheduler.
   Occurs under a callback from the call_every interrupt.  */
void
__pthread_context_switch (void)
{
#ifdef PTHREAD_DEBUG_CONTEXT
  debug_printf ("-- __pthread_context_switch: __pthread_running_thread=%08x\n",
		__pthread_running_thread);
#endif

#if ! __UNIXLIB_ERRNO_THREADED
  /* For a thread-based 'errno' we don't need this.  */
  __pthread_running_thread->thread_errno = errno;
#endif

  __pthread_prev_running_thread = __pthread_running_thread;

  pthread_t next = __pthread_running_thread->next;

  /* Loop around the list looking for a thread that is running.  */
  int iter = 0;
  do
    {
#ifdef PTHREAD_DEBUG_CONTEXT
      /* debug_printf ("__pthread_context_switch: Looking for non idle thread\n");*/
#endif
      if (next != NULL)
        {
          __pthread_running_thread = next;
        }
      else
        {
          if (iter++ > 1)
            __pthread_fatal_error ("-- __pthread_context_switch: Deadlocked (no runnable threads to schedule)");

          __pthread_running_thread = __pthread_thread_list;
        }

      next = __pthread_running_thread->next;

      /* Free up any idle threads that are found */
      if (next == NULL)
        {
          if (__pthread_thread_list->state == STATE_IDLE)
            {
              next = __pthread_thread_list;
              __pthread_thread_list = __pthread_thread_list->next;
              __pthread_cleanup_idle (next);
              next = __pthread_running_thread->next;
            }
        }
      else if (next->state == STATE_IDLE)
        {
          __pthread_running_thread->next = next->next;
          __pthread_cleanup_idle (next);
          next = __pthread_running_thread->next;
        }

      if (__pthread_running_thread->suspended)
	continue;

      if (__pthread_running_thread->state == STATE_COND_TIMED_WAIT)
        {
          /* Check that the condition variable wait has not timed out */
          iter = 0; /* Prevent deadlock error ocurring if we are waiting */
          if (clock () > __pthread_running_thread->condtimeout)
            {
              /* Hmm, this will fail if the monotonic timer wraps around.  */
              __pthread_running_thread->state = STATE_RUNNING;

              /* Remove this thread from the list of threads waiting on the
                 condition variable.  */
              if (__pthread_running_thread->cond->waiting == __pthread_running_thread)
		__pthread_running_thread->cond->waiting = __pthread_running_thread->nextwait;
              else
                {
                  pthread_t thread = __pthread_running_thread->cond->waiting;

                  while (thread->nextwait
			 && thread->nextwait != __pthread_running_thread)
                    thread = thread->nextwait;

                  if (thread->nextwait)
                    thread->nextwait = thread->nextwait->nextwait;
                }
            }
        }
#ifdef PTHREAD_DEBUG_CONTEXT
      debug_printf ("-- __pthread_context_switch: thread=%08x, state=%d\n",
		    __pthread_running_thread, __pthread_running_thread->state);
#endif

    }
  while (__pthread_running_thread->state < STATE_RUNNING);

#if ! __UNIXLIB_ERRNO_THREADED
  /* For a thread-based 'errno' we don't need this.  */
  errno = __pthread_running_thread->thread_errno;
#endif

  if (__pthread_running_thread->cancelpending
      && __pthread_running_thread->cancelstate == PTHREAD_CANCEL_ENABLE
      && __pthread_running_thread->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS)
    {
      /* Asynchronously cancel the thread.  */
      __pthread_running_thread->saved_context->r[15] = (int)pthread_testcancel;
    }

#ifdef PTHREAD_DEBUG_CONTEXT
  debug_printf ("-- __pthread_context_switch: New __pthread_running_thread=%08x\n",
		__pthread_running_thread);
#endif
}

