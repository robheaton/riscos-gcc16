/* RISC OS specific code for gmon profiling
 * Copyright (c) 2010-2011 UnixLib Developers.
 * Written by Lee Noar.
 */

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/cdefs.h>

#include <internal/os.h>
#include <internal/unix.h>
#include <internal/machine-gmon.h>
#include <swis.h>

/*#define DEBUG 1*/
#ifdef DEBUG
#  include <sys/debug.h>
#else
#  define NDEBUG
#endif

#include <assert.h>

#define TIMER_FREQUENCY		100 /* times a second */

/* This is used for the dynamic area name and the WIMP filter name.  */
static const char profiling_name[] = "Unixlib Profiling";

struct profiler_data_t profiler attribute_hidden;

#ifdef __ARM_EABI__
/* EABI: no HAL timer, no interrupt handler, no dynamic area.  The program is sampled by a thread of its own (profil.c), the tables of the
   call graph and the histogram are plain memory, and everything ends with the process.  */
#include <stdlib.h>

extern unsigned int __profil_overflowed attribute_hidden;

attribute_hidden void
gmon_machine_cleanup (void)
{
}

/* Memory for the three tables (the histogram of samples, the call graph arcs and their hash).  */
attribute_hidden char *
gmon_machine_init (int size)
{
  profiler.timer_frequency = PROFIL_RATE;
  return calloc (1, size);
}

attribute_hidden unsigned int
gmon_machine_sample_overflowed (void)
{
  return __profil_overflowed;
}

#else /* !__ARM_EABI__ */

extern void *__profiler_interrupt_handler_start;
extern void *__profiler_interrupt_handler_end;

attribute_hidden void
gmon_machine_cleanup (void)
{
  struct ul_global *gbl = &__ul_global;
  profiler_data_t *prof = &profiler;
  int regs[10];

#ifdef DEBUG
  if (prof->handler_data
      && (prof->handler_data->flags & INT_HANDLER_FLAG_OVERFLOWED))
    debug_printf ("Profiling was halted due to an overflowing sample.\n");
#endif

  if (prof->flags & PROFILER_FLAG_HANDLER_INSTALLED)
    __profile_disable (gbl, prof);

  if (prof->da_number != 0)
    {
      regs[0] = 1;
      regs[1] = prof->da_number;
      if (__os_swi (OS_DynamicArea | XOS_Bit, regs) == NULL)
	prof->da_number = 0;
    }

  if (prof->rma_block != NULL)
    {
      regs[0] = 7;
      regs[2] = (int) prof->rma_block;
      if (__os_swi (OS_Module | XOS_Bit, regs) == NULL)
	prof->rma_block = NULL;
    }
}

static inline unsigned int
page_align (unsigned int addr)
{
  return (addr + 4095) & ~4095;
}

static inline unsigned int
word_align (unsigned int addr)
{
  return (addr + 3) & ~3;
}

static void
cache_hal_routines (profiler_hal_routines *hal_routines)
{
  hal_routines->hal_irq_source =
	__hal_get_routine_address (HAL_IRQSource, (void **)&hal_routines->static_base);
  hal_routines->hal_irq_enable =
	__hal_get_routine_address (HAL_IRQEnable, NULL);
  hal_routines->hal_irq_disable =
	__hal_get_routine_address (HAL_IRQDisable, NULL);
  hal_routines->hal_irq_clear =
	__hal_get_routine_address (HAL_IRQClear, NULL);

  assert (hal_routines->hal_irq_source);
  assert (hal_routines->hal_irq_enable);
  assert (hal_routines->hal_irq_disable);
  assert (hal_routines->hal_irq_clear);
}

static int
sizeof_rma_block (profiler_data_t *profiler)
{
  int handler_size;

  handler_size = __profiler_interrupt_handler_end - __profiler_interrupt_handler_start;

  return handler_size
       + sizeof (struct handler_data_t)
       + word_align (sizeof (profiling_name));
}

static void
fill_rma_block (profiler_data_t *profiler)
{
  void *rma_ptr;
  int handler_size;

  handler_size = __profiler_interrupt_handler_end - __profiler_interrupt_handler_start;

  profiler->handler_code = profiler->rma_block;

  /* Copy interrupt handler to RMA memory. */
  memcpy (profiler->handler_code,
	  __profiler_interrupt_handler_start,
	  handler_size);

  rma_ptr = profiler->rma_block + handler_size;

  /* Allocate the space for the interrupt handler's data. This must follow on directly
     after the interrupt handler code.  */
  profiler->handler_data = (handler_data_t *)rma_ptr;
  memset (profiler->handler_data, 0, sizeof (handler_data_t));

  rma_ptr += sizeof (handler_data_t);

  int regs[10];

  /* Make sure any old code from this area is flushed from the cache.  */
  regs[0] = 1;
  regs[1] = (int) profiler->rma_block;
  regs[2] = (int) rma_ptr;
  __os_swi (OS_SynchroniseCodeAreas | XOS_Bit, regs);

  /* Filter name needs to be in memory that will not be paged out.
     Place this at the end of the block so subsequent alignment isn't required.  */
  profiler->filter_name = rma_ptr;
  strcpy (profiler->filter_name, profiling_name);

/*  rma_ptr += word_align (strlen (profiling_name) + 1);*/
}

static inline unsigned int
host_is_32bit (void)
{
  unsigned int is32bit;

  __asm__ volatile ("SUBS\t%[is32bit], r0, r0\n\t" /* Set at least one status flag.  */
		    "TEQ\tpc, pc\n\t"
		    "MOVEQ\t%[is32bit], #1\n\t"
		    : [is32bit] "=r" (is32bit)
		    : /* no inputs */
		    : "cc");

  return is32bit;
}

static int
load_hal26_module (void)
{
  int regs[10];

  /* Test for the existance of the HAL26 module.  */
  regs[8] = 1;
  regs[9] = HAL_Timers;
  if (__os_swi (OS_Hardware, regs))
    {
      /* Try to load the module. Ignore any errors.  */
      if (SWI_OS_CLI ("RMEnsure HAL26 0.06 RMLoad System:Modules.HAL26") != NULL
	  || SWI_OS_CLI ("RMEnsure HAL26 0.06 Error Profiling support requires HAL26 0.06 or newer") != NULL)
	return 0;

      if (__os_swi (OS_Hardware, regs))
	return 0;
    }

  return 1;
}

attribute_hidden char *
gmon_machine_init (int da_size)
{
  profiler_data_t *prof = &profiler;
  int regs[10], rma_size;
  _kernel_oserror *err;

  /*
  Profiler object is in .bss section and is zero'd by the loader.
  memset (prof, 0, sizeof (*prof));
  */

  if (!host_is_32bit () && !load_hal26_module ())
    return NULL;

  /* Use the last timer that is available. If there is only 1 timer,
     then assume RISC OS is using it and disable profiling.  */
  prof->timer_number = __hal_timers () - 1;
  if (prof->timer_number == 0)
    {
#ifdef DEBUG
      debug_printf ("Unable to start profiling - only one timer available.\n");
#endif
      return NULL;
    }

  da_size = page_align (da_size);

  /* Create a dynamic area for the profiling samples.  */
  regs[0] = 0;
  regs[1] = -1;
  regs[2] = da_size;
  regs[3] = -1;
  regs[4] = 0x80;
  regs[5] = da_size;
  regs[6] = 0;
  regs[7] = 0;
  regs[8] = (int)profiling_name;
  if ((err = __os_swi (OS_DynamicArea | XOS_Bit, regs)) != NULL)
    goto error;

  prof->da_number = regs[1];
  prof->da_base = (unsigned int)regs[3];
  memset ((void *)prof->da_base, 0, da_size);

  prof->timer_frequency = TIMER_FREQUENCY;
  prof->timer_device = __hal_timer_device (prof->timer_number);
  prof->timer_period = __hal_timer_granularity (prof->timer_number) / prof->timer_frequency;

  rma_size = sizeof_rma_block (prof);

  /* Allocate enough RMA memory to hold all code and data.  */
  regs[0] = 6;
  regs[3] = rma_size;
  if ((__os_swi (OS_Module | XOS_Bit, regs)) != NULL)
    goto error;
  prof->rma_block = (void *)regs[2];

  fill_rma_block (prof);

  prof->handler_data->timer_number = prof->timer_number;
  prof->handler_data->timer_device = prof->timer_device;

  /* Find the pointers of some HAL routines.  */
  cache_hal_routines (&prof->handler_data->hal_routines);

  prof->flags |= PROFILER_FLAG_FIRST_TIME;
  prof->handler_data->flags |= INT_HANDLER_FLAG_DISABLED_BY_USER;

  return (char *)prof->da_base;

error:

#ifdef DEBUG
  debug_printf ("monstartup: %s\n", err->errmess);
#endif

  gmon_machine_cleanup ();

  return NULL;
}

attribute_hidden unsigned int
gmon_machine_sample_overflowed (void)
{
  return profiler.handler_data->flags & INT_HANDLER_FLAG_OVERFLOWED;
}

#endif /* !__ARM_EABI__ */
