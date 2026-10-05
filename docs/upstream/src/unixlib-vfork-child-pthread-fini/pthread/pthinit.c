/* Pthread initialisation.
   Written by Martin Piper and Alex Waugh.
   Copyright (c) 2002-2008, 2010, 2015, 2019 UnixLib Developers.  */

#include <stdlib.h>
#include <pthread.h>
#include <malloc.h>
#include <swis.h>
#include <string.h>

#include <internal/swiparams.h>
#include <internal/os.h>
#include <internal/unix.h>

static struct __pthread_thread mainthread;

static const char filter_name[] = "UnixLib pthread";

static void
__get_main_stack (struct __pthread_thread *thread)
{
#if __UNIXLIB_CHUNKED_STACK
  register unsigned int sl __asm__ ("r10");

  thread->stack = (stack_t)(sl - 536);
  thread->stack_size = PTHREAD_STACK_MIN;
#else
  void *fp = __builtin_frame_address (0);
  stack_t stack;
  unsigned stack_base;
  unsigned guard_size;

  _swix(ARMEABISupport_StackOp, _INR(0,1)|_OUT(1), ARMEABISUPPORT_STACKOP_GET_STACK, fp, &thread->stack);

  _swix(ARMEABISupport_StackOp, _INR(0,1)|_OUT(1), ARMEABISUPPORT_STACKOP_GET_BOUNDS, thread->stack, &stack_base);
  _swix(ARMEABISupport_StackOp, _INR(0,1)|_OUTR(1,2), ARMEABISUPPORT_STACKOP_GET_SIZE, thread->stack, &thread->stack_size, &guard_size);
#endif
}

/* Called once, at program initialisation.  */
void
__pthread_prog_init (void)
{
  struct ul_global *gbl = &__ul_global;
#ifdef PTHREAD_DEBUG
  debug_printf ("-- __pthread_prog_init: Program initialisation\n");
#endif

  /* Create a node for the main program.  Calling this function with a
     valid argument means that we must setup 'saved_context' ourselves.  */
  __pthread_running_thread = __pthread_new_node (&mainthread);
  if (__pthread_running_thread == NULL)
    __unixlib_fatal ("pthreads initialisation error: out of memory");

  mainthread.saved_context =
    malloc_unlocked (gbl->malloc_state,
		     sizeof (struct __pthread_saved_context));
  if (mainthread.saved_context == NULL)
    __unixlib_fatal ("pthreads initialisation error: out of memory");

#if !defined(__SOFTFP__) && defined(__VFP_FP__)
  /* Store the ID of the VFP context that was created earlier on */
  _swi(VFPSupport_ActiveContext, _OUT(0), &mainthread.saved_context->vfpcontext);
  /* Set the bottom bit to flag that it wasn't malloc'd */
  mainthread.saved_context->vfpcontext |= 1;
#endif

  mainthread.magic = PTHREAD_MAGIC;
  __get_main_stack (&mainthread);

  strcpy (gbl->pthread_callevery_rma->filter_name, filter_name);

  __pthread_thread_list = __pthread_running_thread;
  gbl->pthread_num_running_threads = 1;
}

/* Called once, at program finalisation, by the last user of the program image.

   Not by a vfork child that ends without exec.  Such a child (the usual reaction to an exec that failed: vfork (); if (child) { execv (...); _exit (127); })
   shares the program image with its parent - this RMA block, __ul_global and the pthread system included - and its _exit () used to free the block here, under the parent's feet: the
   parent went on with a pointer to freed RMA memory (setjmp/longjmp, the signal code and every PTHREAD_UNSAFE section write into the block) and freed it a second time when it ended.
   With one such child the block was still free when the parent freed it again and nothing showed; if the RMA hands the block out in between, the parent goes on using, and then
   frees, memory that belongs to somebody else.  __dynamic_area_refcount is the number of processes that share the image: the vfork child has added itself in __fork_post () and
   takes itself off in __dynamic_area_exit (), which _exit () calls AFTER this function, so more than one means that the image is not this process's to take down; the last user
   (the parent, when it ends) frees the block, once.  */
void attribute_hidden
__pthread_prog_fini (void)
{
  struct ul_global *gbl = &__ul_global;
#ifdef PTHREAD_DEBUG
  debug_printf ("-- __pthread_prog_fini: Program finalisation\n");
#endif

  if (__dynamic_area_refcount > 1)
    return;

  /* pthread timers must be stopped */
  if (gbl->pthread_system_running)
    {
      __pthread_stop_ticker ();
      gbl->pthread_system_running = 0;
    }

  /* Free the RMA block that was allocated in __pthread_prog_init */
  int regs[10];
  
  regs[0] = 7;
  regs[2] = (int) gbl->pthread_callevery_rma;
  __os_swi (OS_Module, regs);
}
