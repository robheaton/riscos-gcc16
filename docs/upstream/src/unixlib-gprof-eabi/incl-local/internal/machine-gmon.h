/* Machine-dependent definitions for profiling support.  RISC OS version.
   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.
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

/* GCC for the ARM cannot compile __builtin_return_address(N) for N != 0,
   so we must use an assembly stub.  */

#include <kernel.h>
#include <sys/cdefs.h>
#ifndef __ARM_EABI__
#ifndef NO_UNDERSCORES
/* The asm symbols for C functions are `_function'.
   The canonical name for the counter function is `mcount', no _.  */
void _mcount (void) __asm__ ("mcount");
#else
/* The canonical name for the function is `_mcount' in both C and asm,
   but some old asm code might assume it's `mcount'.  */
void _mcount (void);
weak_alias (_mcount, mcount)
#endif
#endif /* !__ARM_EABI__ */

static void mcount_internal (u_long frompc, u_long selfpc) __attribute_used__;

#define _MCOUNT_DECL(frompc, selfpc) \
static void mcount_internal (u_long frompc, u_long selfpc)

#ifndef __ARM_EABI__
/* This macro/func MUST save r0, r1 because the compiler inserts
	blind calls to _mount(), ignoring the fact that _mcount may
	clobber registers; therefore, _mcount may NOT clobber registers */
/* if (this_fp!=0) {
	r0 = this_fp
	r1 = this_lr
	r1 = [r1-4] which is caller's lr
	if (r1!=0)
		r1 = caller's lr
	call mcount_internal(this_lr, caller's_lr)
   }
*/

/* This code works as it is because the compiler does not generate
   a stack frame for it. Be warned that adding printfs for debugging
   will add a stack frame and alter the way the code should work.  */
#define MCOUNT								\
void _mcount (void)							\
{									\
  __asm__("stmdb	sp!, {r0, r1, r2, r3, r14};"			\
	  "movs		fp, fp;"				      	\
	  "beq		0f;"						\
	  "teq		r0, r0;"					\
	  "teq		pc, pc;"					\
	  "ldr		r0, [fp, #-4];"					\
	  "bicne	r0, r0, #0xFC000000;"				\
	  "moveq	r1, r14;"					\
	  "bicne	r1, r14, #0xFC000000;"				\
	  "bl		mcount_internal;"				\
	  "0:"								\
	  "ldmia	sp!, {r0, r1, r2, r3, r14}");			\
}
#else /* __ARM_EABI__ */
/* EABI: gcc -pg puts

	push	{lr}
	bl	__gnu_mcount_nc

   after the prologue of every function.  On entry the return address of the function's caller (frompc) is on the stack and lr is the return
   address into the function (selfpc).  The function's arguments may be in r0-r3, so they are saved, and with them lr, which makes the stack
   8-byte aligned again for the C function (the caller's push {lr} made it 4 off).  Returning pops the caller's return address into lr, as the
   AAPCS profiling convention says, and continues in the function through ip.  (This is what glibc's sysdeps/arm/arm-mcount.S does.)  */
#define MCOUNT								\
__asm__ (".text\n"							\
	 "\t.align\t2\n"						\
	 "\t.global\t__gnu_mcount_nc\n"				\
	 "\t.type\t__gnu_mcount_nc, %function\n"			\
	 "__gnu_mcount_nc:\n"						\
	 "\tpush\t{r0, r1, r2, r3, lr}\n"				\
	 "\tldr\tr0, [sp, #20]\n"					\
	 "\tmov\tr1, lr\n"						\
	 "\tbl\tmcount_internal\n"					\
	 "\tpop\t{r0, r1, r2, r3, ip, lr}\n"				\
	 "\tbx\tip\n"							\
	 "\t.size\t__gnu_mcount_nc, . - __gnu_mcount_nc\n");
#endif /* __ARM_EABI__ */

#ifdef __ARM_EABI__
/* EABI programs are sampled once per tick of the ticker of the pthreads (pthread/_context.s: OS_CallEvery with 1, which is every second centisecond):
   50 samples a second.  This is the "profiling clock" that gmon.out gives gprof.  */
#define PROFIL_RATE 50
#endif

extern struct gmonparam _gmonparam attribute_hidden;

#define HAL_IRQEnable		1
#define HAL_IRQDisable		2
#define HAL_IRQClear		3
#define HAL_IRQSource		4
#define HAL_Timers		12
#define HAL_TimerSetPeriod	16
#define HAL_TimerPeriod		17

/* Pointers to HAL functions. These allow the routines to be called
   directly avoiding the SWI mechanism, but they must be called from
   a privileged mode.  */
typedef struct profiler_hal_routines
{
  void *hal_irq_source;
  void *hal_irq_enable;
  void *hal_irq_disable;
  void *hal_irq_clear;
  unsigned int static_base;
} profiler_hal_routines;

/* Flag indicating whether any sample overflowed when the
   interrupt routine incremented it.  */
#define INT_HANDLER_FLAG_OVERFLOWED 1

/* Flag indicating whether the interrupt handler should record
   any samples. Set and cleared by the WIMP filters.  */
#define INT_HANDLER_FLAG_DISABLED_BY_FILTER 2

/* Flag indicating whether the user disabled profiling using
   moncontrol().  */
#define INT_HANDLER_FLAG_DISABLED_BY_USER 4

typedef struct handler_data_t
{
  /* The order of these four members is important as the interrupt
     handler uses an LDMIA to load them.  */
  unsigned int samples;
  unsigned int nsamples;
  unsigned int pc_offset;
  unsigned int pc_scale;

  unsigned int timer_number;
  unsigned int timer_device;

  unsigned int flags;

  profiler_hal_routines hal_routines;
} handler_data_t;

/* Flag indicating whether the WIMP filters were installed.  */
#define PROFILER_FLAG_FILTER_INSTALLED		1

/* Flag indicating whether the interrupt handler is installed
   (using SWI OS_ClaimProcessorVector).  */
#define PROFILER_FLAG_HANDLER_INSTALLED		2

/* Flag indicating whether this is the first time that profiling was enabled.  */
#define PROFILER_FLAG_FIRST_TIME		4

typedef struct profiler_data_t
{
  /* ID number and base address of dynamic area used for recording samples.  */
  int da_number;
  unsigned int da_base;

  /* ID number of hardware timer.  */
  unsigned int timer_number;

  /* Device ID of hardware timer, used by IRQ system.  */
  unsigned int timer_device;

  /* Number of machine ticks between interrupts.  */
  unsigned int timer_period;

  /* How many interrupts per second we require. Currently 100.  */
  unsigned int timer_frequency;

  /* Address of RMA block used to store code and data of interrupt handler
     and filter name.  */
  void *rma_block;

  /* Address in RMA where the interrupt handler code was copied.  */
  void *handler_code;

  /* Address in RMA where the interrupt handler's data was placed.
     This should be directly after the code.  */
  handler_data_t *handler_data;

  /* Address in RMA where filter name is stored.  */
  char *filter_name;

  unsigned int flags;
} profiler_data_t;

extern struct profiler_data_t profiler attribute_hidden;

struct ul_global;

extern _kernel_oserror *
__profile_enable (struct ul_global *,
		  struct profiler_data_t *) attribute_hidden;

extern void
__profile_disable (struct ul_global *,
		   struct profiler_data_t *) attribute_hidden;

int
__profile_frequency (void) attribute_hidden;

extern void *
__hal_get_routine_address (int routine_num,
			   void **sb) attribute_hidden;

/* Given a hardware timer ID number, return IRQ device number.  */
extern int
__hal_timer_device (int timer) attribute_hidden;

/* Return maximum number of ticks timer is capable of producing per second.  */
extern unsigned int
__hal_timer_granularity (int timer) attribute_hidden;

/* Return the number of hardware timers available.  */
extern int
__hal_timers (void) attribute_hidden;

/* Machine specific function to initialise profiling timer and memory.  */
extern char *
gmon_machine_init (int da_size) attribute_hidden;

/* Return non-zero if a sample overflowed during profiling.  */
extern unsigned int
gmon_machine_sample_overflowed (void) attribute_hidden;

/* Low level clean up; removes interrupt handler, dynamic area and WIMP
   filters.  */
extern void
gmon_machine_cleanup (void) attribute_hidden;
