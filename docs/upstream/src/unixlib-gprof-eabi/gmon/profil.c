/*
 * File taken from glibc.
 *  - SCL poison added.
 */
#ifdef __TARGET_SCL__
#  error "SCL build should not use (L)GPL code."
#endif

/* Low-level statistical profiling support function.  RISC OS version.
   Copyright (C) 1995, 1996, 1997 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <internal/machine-gmon.h>
#include <kernel.h>
#include <internal/unix.h>

/*#define DEBUG 1*/
#ifdef DEBUG
#  include <sys/debug.h>
#endif

#ifdef __ARM_EABI__
/* EABI: the program is sampled by a thread of its own.

   The threads of UnixLib are scheduled by a ticker (OS_CallEvery, pthread/_context.s).  At a tick the running thread is interrupted, its registers - the
   program counter among them - are saved in its context, and the next runnable thread gets the processor.  The sampler is a second thread that does
   nothing but read the saved program counter of the thread that was interrupted (__pthread_prev_running_thread, set by the scheduler), count it in the
   histogram and give the processor back with pthread_yield: one sample per tick.  That works wherever threads work, in a Task window and in the desktop
   (an interval timer, setitimer, is refused there), and needs no hardware timer, no interrupt handler and no memory outside the program.

   A tick that falls into a region where UnixLib does not allow a context switch (malloc, stdio: __pthread_disable_ints) is not taken.  A program that has
   threads of its own is sampled only at every other thread's turn: the profile has the right shape but the time is too small.  */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

unsigned int __profil_overflowed attribute_hidden;

extern pthread_t __pthread_prev_running_thread attribute_hidden;

static u_short *prof_buf;		/* the histogram */
static size_t prof_nsamples;		/* the number of its counters */
static size_t prof_offset;		/* the address that counter 0 stands for */
static u_int prof_scale;		/* 65536 = one counter for every 2 bytes of code, as profil () says */
static volatile int prof_on;		/* the sampler should count */
static pthread_t prof_thread;		/* the sampler, made at the first call */
static unsigned long prof_ticks;	/* the times that the sampler looked at the interrupted thread */
static unsigned long prof_hits;		/* ... and found its program counter in the profiled range */
static clock_t prof_start;		/* clock () when profiling began */

static void *
prof_sampler (void *arg)
{
  for (;;)
    {
      if (prof_on)
	{
	  pthread_t t = __pthread_prev_running_thread;

	  if (t != NULL && t != prof_thread && t->magic == PTHREAD_MAGIC && t->saved_context != NULL)
	    {
	      u_int pc = (u_int) t->saved_context->r[15];

	      prof_ticks++;

	      if (pc >= prof_offset)
		{
		  unsigned long long i = (((unsigned long long) ((pc - prof_offset) >> 1)) * prof_scale) >> 16;

		  if (i < prof_nsamples)
		    {
		      prof_hits++;
		      if (prof_buf[i] != 0xffff)
			prof_buf[i]++;
		      else
			__profil_overflowed = 1;
		    }
		}
	    }
	}
      pthread_yield ();
    }
  return NULL;
}

/* Enable statistical profiling, writing samples of the PC into at most
   SIZE bytes of SAMPLE_BUFFER; every tick of the ticker of the threads while
   profiling is enabled, the sampler counts the user PC in
   SAMPLE_BUFFER[((PC - OFFSET) / 2) * SCALE / 65536].  If SCALE is zero,
   disable profiling.  Returns zero on success, -1 on error.  */
int
__profil (u_short *sample_buffer, size_t size, size_t offset, u_int scale)
{
  if (sample_buffer == NULL || scale == 0)
    {
      /* Disable profiling.  GMON_VERBOSE says what was counted.  */
      if (prof_on && getenv ("GMON_VERBOSE") != NULL)
	fprintf (stderr, "gmon: %lu samples, %lu of them in the program, %ld centiseconds since the start of the profile (%d samples a second at most)\n",
		 prof_ticks, prof_hits, (long) (clock () - prof_start), PROFIL_RATE);
      prof_on = 0;
      return 0;
    }

  prof_buf = sample_buffer;
  prof_nsamples = size / sizeof *sample_buffer;
  prof_offset = offset;
  prof_scale = scale;

  if (prof_thread == NULL)
    {
      pthread_attr_t attr;

      pthread_attr_init (&attr);
      pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
      if (pthread_create (&prof_thread, &attr, prof_sampler, NULL) != 0)
	{
	  prof_thread = NULL;
	  return -1;
	}
    }

  prof_start = clock ();
  prof_on = 1;
  return 0;
}
weak_alias (__profil, profil)

#else /* !__ARM_EABI__ */

/* Enable statistical profiling, writing samples of the PC into at most
   SIZE bytes of SAMPLE_BUFFER; every processor clock tick while profiling
   is enabled, the system examines the user PC and increments
   SAMPLE_BUFFER[((PC - OFFSET) / 2) * SCALE / 65536].  If SCALE is zero,
   disable profiling.  Returns zero on success, -1 on error.  */

int
__profil (u_short *sample_buffer, size_t size, size_t offset, u_int scale)
{
  struct ul_global *gbl = &__ul_global;
  profiler_data_t *prof = &profiler;
  _kernel_oserror *err;

  if (sample_buffer == NULL || scale == 0)
    {
      /* Disable profiling.  */
      if (prof->handler_data->flags & INT_HANDLER_FLAG_DISABLED_BY_USER)
	/* Wasn't turned on.  */
	return 0;

      prof->handler_data->flags |= INT_HANDLER_FLAG_DISABLED_BY_USER;

#ifdef DEBUG
      debug_printf("Profiling stopped\n");
#endif

      return 0;
    }

  if (!(prof->handler_data->flags & INT_HANDLER_FLAG_DISABLED_BY_USER))
    {
      /* Was already turned on.  */
      return 0;
    }

  if (prof->flags & PROFILER_FLAG_FIRST_TIME)
    {
      /* Set up the data that the interrupt handler requires.  */
      prof->handler_data->samples = (unsigned int)sample_buffer;
      prof->handler_data->nsamples = size / sizeof *sample_buffer;
      prof->handler_data->pc_offset = offset;
      prof->handler_data->pc_scale = scale;

      if ((err = __profile_enable (gbl, prof)) != NULL)
	{
#ifdef DEBUG
	  debug_printf ("Could not start profiling: %s\n", err->errmess);
#endif
	  return -1;
	}

      prof->flags &= (~PROFILER_FLAG_FIRST_TIME);
    }

  prof->handler_data->flags &= (~INT_HANDLER_FLAG_DISABLED_BY_USER);

#ifdef DEBUG
  debug_printf("Profiling started\n");
#endif

  return 0;
}
weak_alias (__profil, profil)

#endif /* !__ARM_EABI__ */
