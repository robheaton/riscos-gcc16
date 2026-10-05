@ Provide program entry and initialise the UnixLib world
@ Copyright (c) 2002-2019 UnixLib Developers

#include "internal/asm_dec.s"

	@ Keep in sync with error_table.
.set	ERR_NO_MEMORY, 0
.set	ERR_NO_CALLASWI, 1
.set	ERR_NO_SUL, 2
.set	ERR_NO_FPE, 3
.set	ERR_UNRECOVERABLE, 4
.set	ERR_TOO_LOW_CPU_ARCH, 5
.set	ERR_NO_VFP, 6
.set	ERR_BAD_VFP, 7
.set	ERR_NO_STACK, 8

@ Constants for field offsets within a stack chunk
@ The stack is allocated in chunks, each chunk is a multiple of 4KB, and
@ contains a 24 byte header.
@ The next and prev fields form a doubly linked list of allocated chunks.
@ This is fairly similar to how the SCL's stack works
@ (see PRMs, 4-239 and Appendix C)
@
@  ______
@ |	 | +0
@ |Header|
@ |	 |
@ |______|
@ |	 | +24
@ |	 |
@ |	 | +536 <- sl
@ |	 |
@ |	 |
@ ________/
@/
@ |	 |
@ |______|
@	  +4096 * n  <- sp (initially)

.set	MAX_DA_NAME_SIZE, 32

	.syntax unified
	.text


	.weak	__program_name
	.weak	__dynamic_no_da
	.weak	__dynamic_da_name
	.weak	__dynamic_da_max_size
	.weak   __stack_size

	@ RMEnsure the minimum version of the SharedUnixLibrary we need.
	@ Now check System modules first as UnixLib package is deprecated.
#if !defined(__SOFTFP__) && defined(__VFP_FP__)
#ifdef __ARM_EABI__
.set	SUL_MIN_VERSION, 116
#define SUL_VERSION_STRING "1.16"
#else
.set	SUL_MIN_VERSION, 113
#define SUL_VERSION_STRING "1.13"
#endif
#else
.set	SUL_MIN_VERSION, 107
#define SUL_VERSION_STRING "1.07"
#endif
rmensure1:
	.ascii	"RMEnsure SharedUnixLibrary "
	.ascii	SUL_VERSION_STRING
	.asciz	" RMLoad System:Modules.SharedULib"
rmensure2:
	.ascii	"RMEnsure SharedUnixLibrary "
	.ascii	SUL_VERSION_STRING
	.asciz	" RMLoad UnixLib:Modules.SharedULib"
	@ The exact error message is not important as it will get ignored.
rmensure3:
	.ascii	"RMEnsure SharedUnixLibrary "
	.ascii	SUL_VERSION_STRING
	.asciz	" Error XYZ"
	.align

	.global	__main
	NAME	__main

__main:
	@ (g)crt1.o passes several values in memory addressed as an offset
	@ from a1. Offsets 0, 4 and 24 are used by both the static and DSO library,
	@ the others are used by the DSO only.
	@
	@ a1+0  = _init
	@ a1+4  = _fini
	@ a1+8  = __executable_start
	@ a1+12 = start of free memory
	@ a1+16 = __data_start
	@ a1+20 = main
	@ a1+24 = Flags
	@	   bit 0 set = enable profiling
	@ a1+28 = Ptr to shared libraries initialisation function - may be NULL
	@ a1+32 = Ptr to shared libraries finalisation function - may be NULL

	MOV	v1, a1

	PIC_LOAD v4

	@ Read environment parameters
	@ On exit:
	@   a1 = pointer to environment string
	@   a2 = permitted RAM limit (highest address available + 1)
	@   a3 = pointer to real time the program was started (5 bytes)
	SWI	OS_GetEnv		@ When error, call parent error handler.

	@ struct_base is the start of our memory environment variables
	@ See the end of this file.  For the initialisation here, we
	@ will always use ip as the base register.
	LDR	ip, .L0			@=__ul_global
 PICEQ "LDR	ip, [v4, ip]"
	LDR	fp, .L0+4		@=__ul_memory
 PICEQ "LDR	fp, [v4, fp]"

	@ __ul_global.unixlib_cli = pointer to command line
	STR	a1, [ip, #GBL_UNIXLIB_CLI]
	@ __ul_memory.appspace_himem = permitted RAM limit
	STR	a2, [fp, #MEM_APPSPACE_HIMEM]

	LDMIA	a3, {a1, a2}			@ Get time
	STR	a1, [ip, #GBL_TIME_LOW]		@ __ul_global.time (low word)
	STR	a2, [ip, #GBL_TIME_HIGH]	@ __ul_global.time (high word)

	@ For the shared library, fill in the linker generated values that
	@ are passed to us by crt1.o. These are only known at runtime.
 PICEQ "LDR	a1, [v1, #CRT1_EXEC_START]"
 PICEQ "LDR	a2, [v1, #CRT1_FREE_MEM]"
 PICEQ "LDR	a3, [v1, #CRT1_DATA_START]"
 PICEQ "STR	a1, [fp, #MEM_ROBASE]"		@ __executable_start
 PICEQ "STR	a2, [fp, #MEM_RWLOMEM]"
 PICEQ "STR	a3, [fp, #MEM_RWBASE]"		@ __data_start

	@ Save the address of the crt1_data for later
	LDR	a1, .L0+32			@ crt1_data
 PICEQ "LDR	a1, [v4, a1]"
	STR	v1, [a1, #0]

	@ Obtain the application space limit.  Note that this is different
	@ to the permitted RAM limit returned by OS_GetEnv, i.e. MEM_APPSPACE_HIMEM
	MOV	a1, #14
	MOV	a2, #0
	SWI	OS_ChangeEnvironment		@ No X call, if error call parent
	STR	a2, [fp, #MEM_APPSPACE_LIMIT]	@ error handler.

	@ When the permitted RAM limit (appspace_himem) is below the application
	@ space limit, the memory between the two is not ours: a process that was
	@ started by vfork () + exec () is told by SharedUnixLibrary to stay below
	@ the copy of its parent, which is kept there.  Remember the limit, so
	@ that __stackalloc_incr_wimpslot never raises appspace_himem above it
	@ (-1: no such limit).
himem_max_start:
	LDR	a3, [fp, #MEM_APPSPACE_HIMEM]
	CMP	a3, a2
	MVNCS	a3, #0
	STR	a3, [fp, #MEM_APPSPACE_HIMEM_MAX]
himem_max_end:

	@ For a description of the memory layout of a UnixLib application
	@ see sys/brk.c.
	LDR	a2, [fp, #MEM_RWLOMEM]		@ __ul_memory.rwlomem
	STR	a2, [fp, #MEM_RWBREAK]		@ __ul_memory.rwbreak = __ul_memory.rwlomem
	STR	a2, [fp, #MEM_STACK_LIMIT]	@ __ul_memory.stack_limit = __ul_memory.rwlomem
#if __UNIXLIB_CHUNKED_STACK

	LDR	sp, [fp, #MEM_APPSPACE_HIMEM]

	@ The stack is allocated in chunks in the wimpslot, with the first
	@ 4KB chunk immediately below 'appspace_himem'.  We cannot place it
	@ in a dynamic area because GCC might generate trampolines.  In USR32
	@ mode we could however.

	@ Reserve the top 4K for the signal handler stack
	@ 8 bytes are needed above the signal handler stack
	@ for the stackalloc heap
	SUB	sp, sp, #8

	STR	sp, [ip, #GBL_SIGNALHANDLER_SP]
	@ The signal handler stack cannot be extended, so we don't need to
	@ setup the chunk structure.
	SUB	sp, sp, #4096
	ADD	sl, sp, #512
	STR	sl, [ip, #GBL_SIGNALHANDLER_SL]

	@ Reserve a further 8 bytes above the initial chunk
	@ for the stackalloc heap
	SUB	sp, sp, #8

	@ If __stack_size is defined, use its value as the intial stack chunk size.
	@ This can be used to mimic a flat stack of any required size, with the
	@ benefit of stack extension if things do get critical.
	LDR	v1, ___stack_size
 PICEQ "LDR	v1, [v4, v1]"
	TEQ	v1, #0
	LDRNE	v1, [v1]
	MOVEQ	v1, #4096
	CMP	v1, #4096
	MOVLT	v1, #4096

	SUB	a1, sp, v1
	ADD	sl, a1, #512 + CHUNK_OVERHEAD

	SUBS	a3, a1, #8
	MOVMI	a1, #ERR_NO_MEMORY
	BMI	__exit_with_error_num	@ No room for stack, exit.

	@ __stackalloc_init needs a minimum of 8 bytes below the initial
	@ chunk for its heap - check this doesn't overlap the code section
	STR	a3, [fp, #MEM_STACK]	@ __ul_memory.stack = bottom of stack
	CMP	a3, a2
	MOVCC	a1, #ERR_NO_MEMORY
	BCC	__exit_with_error_num	@ No room for stack, exit.

	@ Initialise the first stack chunk
	MOV	a2, #0
	STR	a2, [a1, #CHUNK_PREV]
	STR	a2, [a1, #CHUNK_NEXT]
	STR	a2, [a1, #CHUNK_DEALLOC]
	STR	a2, [a1, #CHUNK_RETURN]
	LDR	a2, __stackchunk_magic_number
	STR	a2, [a1, #CHUNK_MAGIC]
	STR	v1, [a1, #CHUNK_SIZE]
#else
	MOV	a1, #ARMEABISUPPORT_STACKOP_ALLOC
	MOV	a2, #1			@ 4KB (1 page) signal handler stack
	MOV	a3, #0			@ No stack guard for the signal handler stack
	LDR	a4, ___program_name
 PICEQ "LDR	a4, [v4, a4]"
	TEQ	a4, #0
	LDRNE	a4, [a4, #0]
	SWI	XARMEABISupport_StackOp
	TEQ	a3, #0
	MOVEQ	a1, #ERR_NO_STACK
	BEQ	__exit_with_error_num	@ Failed to allocate stack, exit.

	STR	a3, [ip, #GBL_SIGNALHANDLER_SP]

	MOV	a1, #ARMEABISUPPORT_STACKOP_ALLOC
	MOV	a2, #256		@ Allow for 1MB (256 pages) of stack
	MOV	a3, #1			@ 1 Stack guard page
	LDR	a4, ___program_name
 PICEQ "LDR	a4, [v4, a4]"
	TEQ	a4, #0
	LDRNE	a4, [a4, #0]
	SWI	XARMEABISupport_StackOp
	MOVS	sp, a3
	MOVEQ	a1, #ERR_NO_STACK
	BEQ	__exit_with_error_num	@ Failed to allocate stack, exit.
	LDR	a3, [fp, #MEM_APPSPACE_HIMEM]
	SUB	a3, a3, #4
	STR	a3, [fp, #MEM_STACK]	@ __ul_memory.stack = bottom of stack
#endif

	MOV	v1, ip		@ Temporary variable

	@ Check for the existence of CallASWI.  Terminate the process
	@ with an appropriate error message, if it doesn't exist.
	ADRL	a1, error_no_callaswi
	MOV	a2, a1
	MOV	ip, #OS_GenerateError
	ORR	ip, ip, #X_Bit
	SWI	XOS_CallASWIR12
	@ If CallASWI is unknown then R0 will point to a different address,
	@ the error block for Unknown SWI
	TEQ	a2, a1
	MOVNE	a1, #ERR_NO_CALLASWI
	BNE	__exit_with_error_num

	MOV	ip, v1		@ Restore ip.

	LDR	a1, .L0+12	@=rmensure1	; If no SUL or not recent enough, ...
 PICEQ "LDR	a1, [v4, a1]"
	SWI	XOS_CLI			@ load it.
	LDR	a1, .L0+16	@=rmensure2	; Try a second location.
 PICEQ "LDR	a1, [v4, a1]"
	SWI	XOS_CLI			@
	LDRVC	a1, .L0+20	@=rmensure3	; If still not recent enough, ...
 PICEQ "LDRVC	a1, [v4, a1]"
	SWIVC	XOS_CLI			@ complain.
	MOVVS	a1, #ERR_NO_SUL
	BVS	__exit_with_error_num

	@ Checks for minimum ARM CPU architecture.  When it is an unknown CPU
	@ architecture, we continue without error.
	BL	__get_cpu_arch
	TEQ	a1, #0
	BEQ	unknown_cpu_arch
	CMP	a1, #__ARM_ARCH__
	MOVCC	a1, #ERR_TOO_LOW_CPU_ARCH
	BCC	__exit_with_error_num
unknown_cpu_arch:

	BL	__probe_cpu_caps

	@ Use of DAs explicitly overridden if __dynamic_no_da is declared
	MOV	lr, #-1
	STR	lr, [ip, #GBL_DYNAMIC_NUM]

	LDR	a1, ___dynamic_no_da
 PICEQ "LDR	a1, [v4, a1]"
	TEQ	a1, #0
	BNE	no_dynamic_area

	@ Check OS version for RISC OS 3.5 or more recent.
	MOV	a1, #129
	MOV	a2, #0
	MOV	a3, #255
	SWI	XOS_Byte
	BVS	no_dynamic_area
	CMP	a2, #165
	BCC	no_dynamic_area

	@ Area name is <program name> + "$Heap".  Figure now out what
	@ <program name> is.
	LDR	a1, ___program_name
 PICEQ "LDR	a1, [v4, a1]"
	TEQ	a1, #0
	LDREQ	a1, [ip, #GBL_UNIXLIB_CLI]	@ __cli
	LDRNE	a1, [a1, #0]	@ __program_name
	MOV	a2, a1
	@ Search for space or end of cli string
t01:
	LDRB	a3, [a1], #1
	TEQ	a3, #'.'
	MOVEQ	a2, a1
	TEQ	a3, #':'
	MOVEQ	a2, a1
	CMP	a3, #' '
	BGT	t01
	SUB	a1, a1, #1	@ back up to point at terminator char

	@ Use a maximum of (dynamic_area_name_end - dynamic_area_name_begin)
	@ characters from the program name
	LDR	v1, .L0+8	@=dynamic_area_name_end
 PICEQ "LDR	v1, [v4, v1]"
	MOV	a3, #0
	STRB	a3, [v1, #5]	@ Terminate the $Heap

	SUB	a4, a1, a2
	CMP	a4, #MAX_DA_NAME_SIZE
	SUBCS	v6, v1, #MAX_DA_NAME_SIZE
	SUBCC	v6, v1, a4

	MOV	a3, v6
t02:
	LDRB	a4, [a2], #1
	STRB	a4, [a3], #1
	CMP	a3, v1
	BCC	t02
	@ v6 => OS variable to check if Heap or DA needs to be used.

	@ Allow the user the option of setting their own name for the
	@ dynamic area used as a heap.	 If the variable __dynamic_da_name
	@ exists, then it must be a const char pointer (not an array) to an
	@ alternate name.  DAs are always used in this case, and there's
	@ no need to set a $Heap variable.

	@ The main use of this is when the binary is called !RunImage.
	@ e.g.:  const char * const __dynamic_da_name = "Nettle Heap";

	LDR	v5, ___dynamic_da_name
 PICEQ "LDR	v5, [v4, v5]"
	TEQ	v5, #0
	LDRNE	v5, [v5, #0]	@ get the actual string referred to
	BNE	t07

	@ Check environment variable for creating a DA
	MOV	a1, v6
	MOV	a3, #-1
	MOV	a4, #0
	MOV	v1, #0
	SWI	XOS_ReadVarVal
	CMP	a3, #0
	BGE	no_dynamic_area
	MOV	v5, v6

	@ v5 => DA area name
t07:
	@ Default max size for DA is 32MB
	MOV	v2, #32*1024*1024
	@ If __dynamic_da_max_size is defined, use its value as the max size
	LDR	v1, ___dynamic_da_max_size
 PICEQ "LDR	v1, [v4, v1]"
	TEQ	v1, #0
	LDRNE	v2, [v1]

	LDR	a1, .L0+8	@=dynamic_area_name_end
 PICEQ "LDR	a1, [v4, a1]"
	MOV	a3, #'M'
	STRB	a3, [a1, #5]	@ Change back to $HeapMax

	MOV	a1, v6
	MOV	a3, #4
	SUB	sp, sp, a3
	MOV	a2, sp
	MOV	a4, #0
	SWI	XOS_ReadVarVal	@ Read value of progname$HeapMax
	BVS	t08
	TEQ	v1, #1		@ Should be a number variable
	LDREQ	v2, [sp], #4
	MOVEQ	v2, v2, LSL#20	@ Convert MB into bytes

	@ v2 = size of DA area
t08:
	LDR	a1, .L0+8	@=dynamic_area_name_end
 PICEQ "LDR	a1, [v4, a1]"
	MOV	a3, #0
	STRB	a3, [a1, #5]	@ Change back to $Heap

 PICEQ "MOV	v6, v4"		@ Save PIC register
	@ Create dynamic area
	MOV	a1, #0
	MOV	a2, #-1
	MOV	a3, #0		@ Set initial size of dynamic area to 0K
	MOV	a4, #-1
	MOV	v1, #0x80
	@ v2 is already set from above
	MOV	v3, #0
	MOV	v4, #0
	@ v5 is already set from above
	SWI	OS_DynamicArea	@ No X bit, if error -> call parent error handler.
	MOV	v1, a3				@ setup for deletion at exit
	STR	a2, [ip, #GBL_DYNAMIC_NUM]	@ __ul_global.dynamic_num
 PICEQ "MOV	v4, v6"		@ Restore PIC register

	@ v1 is size left in area, a4 is start offset
	ADD	a1, v1, a4
	@ __ul_memory.dalomem = start of dynamic area
	STR	a4, [fp, #MEM_DALOMEM]
	@ __ul_memory.dabreak = end of used part of DA
	STR	a1, [fp, #MEM_DABREAK]
	@ __ul_memory.dalimit = end of used part of DA
	STR	a1, [fp, #MEM_DALIMIT]

no_dynamic_area:
	MOV	fp, #0

	@ Find out whether we are executing within a TaskWindow or not.
	MOV	a1, #0
	SWI	XTaskWindow_TaskInfo
	MOVVS	a1, #0
	STR	a1, [ip, #GBL_TASKWINDOW]	@ __ul_global.taskwindow

	@ Find out whether we are executing as a WIMP program or not.
	@ Code similar to __get_taskhandle.
	MOV	a1, #3		@ Text output mode, or desktop mode ?
	SWI	XWimp_ReadSysInfo
	MOVVS	a1, #0
	TEQ	a1, #0
	MOVNE	a1, #5		@ When desktop mode, get the taskhandle
	SWINE	XWimp_ReadSysInfo
	MOVVS	a1, #0
	STR	a1, [ip, #GBL_TASKHANDLE]	@ __ul_global.taskhandle

	@ Cache the system page size as this call can be slow.
	@ Used by getpagesize ().
	SWI	XOS_ReadMemMapInfo
	STR	a1, [ip, #GBL_PAGESIZE]		@ __ul_global.pagesize

#ifndef __SOFTFP__
#ifdef __VFP_FP__
	@ Check that VFPSupport is available
	SWI	XVFPSupport_Version
	MOVVS	a1, #0
	CMP	a1, #2		@ We need 0.02 or above
	MOVLO	a1, #ERR_NO_VFP
	BLO	__exit_with_error_num
	@ Check that the hardware has the features we expect
	@ Unfortunately GCC doesn't give us any #defines indicating VFP version, register count,
	@ etc., so we can only do a couple of simple generic checks here
	MOV	a1, #0
	SWI	VFPSupport_Features @ No X bit, SWI should be available
	@ a2 & a3 = MVFR registers
	@ MVFR0 checks:
	TST	a2, #0x0000000f @ Check at least some registers exist
	TSTNE	a2, #0x000000f0	@ Single precision floats
	TSTNE	a2, #0x00000f00	@ Double precision floats
	TSTNE	a2, #0x000f0000 @ Divide
	TSTNE	a2, #0x00f00000 @ Square root
	@ MVFR1 checks:
#ifdef __ARM_NEON__
	TSTNE	a3, #0x00000f00 @ NEON load/store
	TSTNE	a3, #0x0000f000 @ NEON integer
	TSTNE	a3, #0x000f0000 @ NEON single precision floats
#endif
	MOVEQ	a1, #ERR_BAD_VFP
	BEQ	__exit_with_error_num
	@ Allocate some stack space for our initial context (since malloc isn't available yet)
	@ We don't know whether we're targeting D16 or D32 hardware, so request a context
	@ of maximum size
	TST	a2, #0xe	@ EQ -> 16 registers, NE -> 32 (or more?)
#ifdef __ARM_EABI__
	MOV	a1, #0x80000001	@ User mode, non application space, activate now
#else
	MOV	a1, #0x80000003	@ User mode, application space, activate now
#endif
	MOVEQ	a2, #16
	MOVNE	a2, #32
	SWI	XVFPSupport_CheckContext
	MOVVS	a1, #ERR_BAD_VFP
	BVS	__exit_with_error_num
	SUB	sp, sp, a1
	BIC	sp, sp, #7	@ AAPCS wants 8 byte alignment
	STR	a2, [ip, #GBL_VFP_REGCOUNT]	@ __ul_global.vfp_regcount
#else
	@ Recognise the Floating Point facility by determining whether
	@ the SWI FPEmulator_Version actually exists (and works).
	@ We insist on having at least version 4.00.
	@ We don't care about this in soft-float builds
	SWI	XFPEmulator_Version
	MOVVS	a1, #0
	CMP	a1, #400	@ We want 4.00 or above so we can use SFM/LFM
	MOVCC	a1, #ERR_NO_FPE
	BCC	__exit_with_error_num
#endif
#endif

	@ We need to create this now so that we have its address to give to
	@ the callback handler, but we can initialise it later.
	MOV	a1, #6
	MOV	a4, #PTHREAD_CALLEVERY_STRUCT_SIZE
	SWI	XOS_Module
	MOVVS	a1, #ERR_NO_MEMORY
	BVS	__exit_with_error_num

	STR	a3, [ip, #GBL_PTH_CALLEVERY_RMA]
	MOV	v1, a3
@	MOV	v2, ip

	@ memset may use VFP/NEON, so we can't use it until the VFP context
	@ has been created further down.
	MOV	a1, #0
	MOV	a2, #PTHREAD_CALLEVERY_STRUCT_SIZE / 4
0:
	STR	a1, [a3], #4
	SUBS	a2, a2, #1
	BNE	0b

	STR	v4, [v1, #PTHREAD_CALLEVERY_RMA_GOT_PTR]

	@ Now we'll initialise the C library, then call the user program.

	MOV	a1, #SUL_MIN_VERSION
	@ SUL version we are expecting. Newer versions should be backwards
	@ compatible, if not they will return an error
	SWI	SharedUnixLibrary_Initialise	@ No X bit, if error -> call parent error handler.
	STR	a1, [ip, #GBL_SULPROC]
	STR	a2, [ip, #GBL_UPCALL_HANDLER_ADDR]
	STR	a3, [ip, #GBL_UPCALL_HANDLER_R12]

	STR	a2, [v1, #PTHREAD_CALLEVERY_RMA_UPCALL_ADDR]
	STR	a3, [v1, #PTHREAD_CALLEVERY_RMA_UPCALL_R12]

#if defined (__ARM_EABI__) || defined (PIC)
	MOV	v1, a1
	LDR	a4, [v1, #SULPROC_STATUS]
#ifdef __ARM_EABI__
	ORR	a4, a4, #SULPROC_STATUS_FLAG_IS_ARMEABI

	MOV	a1, #ARMEABISUPPORT_STACKOP_GET_STACK
	MOV	a2, sp
	SWI	XARMEABISupport_StackOp
	MOVVS	a1, #ERR_NO_STACK		@ This shouldn't happen
	BVS	__exit_with_error_num
	STR	a2, [v1, #SULPROC_STACK]
#endif
#ifdef PIC
	ORR	a4, a4, #SULPROC_STATUS_FLAG_IS_DYNAMIC
#endif
	STR	a4, [v1, #SULPROC_STATUS]
#endif

#if !defined(__SOFTFP__) && defined(__VFP_FP__)
	@ Create and activate the VFP context we allocated space for earlier
	@ This is done after SUL initialisation just so we don't have to worry
	@ about cleaning it up if SUL fails
#ifdef __ARM_EABI__
	MOV	a1, #0x80000001	@ User mode, non application space, activate now
#else
	MOV	a1, #0x80000003
#endif
	LDR	a2, [ip, #GBL_VFP_REGCOUNT]
	MOV	a3, sp
	MOV	a4, #0
	SWI	VFPSupport_CreateContext
#endif

	@ Initialise the malloc memory allocator.
	BL	__ul_malloc_init
	@ Read the current RISC OS environment handler state
	BL	__env_read
	@ Install the UnixLib environment handlers
	BL	__env_unixlib
#if __UNIXLIB_CHUNKED_STACK
	@ Initialise the stack heap in application space
	BL	__stackalloc_init
#endif

	@ Initialise the UnixLib library
	@ NOTE:	 No calls to brk, sbrk, or malloc should occur before
	@ calling this function.
	BL	__unixinit
	LDR	v2, .L0+32		@=crt1_data
 PICEQ "LDR	v2, [v4, v2]"
	LDR	v2, [v2, #0]

#ifndef __ARM_EABI__
	LDR	a1, [v2, #CRT1_FLAGS]
	TST	a1, #1
	BLNE	__gmon_start__
#endif

	@ Normally, the dynamic linker would take care of shared library
	@ initialisation, but UnixLib has to be initialised first so that
	@ the library initialisers can use its resources.

	@ --- Shared Libraries Initialisation ---
	@ Use the dynamic linker to call the _init functions of all
	@ shared libraries. This may also register the _fini functions
	@ with atexit().
 PICEQ "LDR	a1, [v2, #CRT1_LIB_INIT]"
 PICEQ "TEQ	a1, #0"
 PICEQ "MOVNE	lr, pc"
 PICEQ "MOVNE	pc, a1"

	@ --- Shared Libraries Finalisation ---
	@ If the dynamic linker passed a shared library finalisation
	@ function, then register it for calling at program exit. This
	@ may be NULL, in which case we assume the LIB_INIT function
	@ above did the job for us.
 PICEQ "LDR	a1, [v2, #CRT1_LIB_FINI]"
 PICEQ "TEQ	a1, #0"
 PICEQ "BLNE	atexit"

	@ --- Executable Initialisation ---
	@ Call the program's _init function.
	LDR	a1, [v2, #CRT1_EXEC_INIT]
	MOV	lr, pc
	MOV	pc, a1

#ifdef __ARM_EABI__
	@ Call the program's .init_array functions.
	@ FIXME: what about FINI_ARRAY?
	LDR	v1, [v2, #CRT1_INIT_ARRAY_START]
	LDR	v3, [v2, #CRT1_INIT_ARRAY_END]
	TEQ	v1, v3
	BEQ	no_init_array
0:
	LDR	a1, [v1], #4
	MOV	lr, pc
	MOV	pc, a1
	CMP	v1, v3
	BLO	0b

no_init_array:
#endif
	@ --- Executable Finalisation ---
	@ Make sure the _fini function of the executable is called at program
	@ exit.
	LDR	a1, [v2, #CRT1_EXEC_FINI]
	BL	atexit

	@ Check if we have a non-NULL main() routine.
	LDR	v1, [v2, #CRT1_MAIN]
	TEQ	v1, #0
	ADREQ	a1, no_main_routine
	BEQ	__unixlib_fatal

	@ Load in a1, a2 and a3 registers the main() parameters, i.e.
	@ argc, argv and, for Unix systems compatibility, environ.
	LDR	lr, .L0+24		@ = __u
 PICEQ "LDR	lr, [v4, lr]"
	LDR	lr, [lr, #0]		@ Points to ___u structure.
	LDMIA	lr, {a1-a2}		@ PROC_ARGC = 0, PROC_ARGV = 4
	LDR	a3, .L0+28		@ = environ
 PICEQ "LDR	a3, [v4, a3]"
	LDR	a3, [a3, #0]

	MOV	lr, pc			@ Jump to supplied main() program
	MOV	pc, v1

	@ C programs always terminate by calling exit.
	B	exit
.L0:
	WORD	__ul_global		@ offset 0
	WORD	__ul_memory		@ offset 4
	WORD	dynamic_area_name_end	@ offset 8
	WORD	rmensure1		@ offset 12
	WORD	rmensure2		@ offset 16
	WORD	rmensure3		@ offset 20
	WORD	__u			@ offset 24
	WORD	environ			@ offset 28
	WORD	crt1_data		@ offset 32

no_main_routine:
	.asciz	"There is no main function"
	.align
	DECLARE_FUNCTION __main

___program_name:
	WORD	__program_name
	DECLARE_OBJECT ___program_name
___dynamic_da_name:
	WORD	__dynamic_da_name
	DECLARE_OBJECT ___dynamic_da_name
___dynamic_da_max_size:
	WORD	__dynamic_da_max_size
	DECLARE_OBJECT ___dynamic_da_max_size
___dynamic_no_da:
	WORD	__dynamic_no_da
	DECLARE_OBJECT ___dynamic_no_da
___stack_size:
	WORD	__stack_size
	DECLARE_OBJECT ___stack_size

	@ Can only be used to report fatal errors under certain conditions.
	@ Be sure that at this point the UnixLib environment handlers
	@ are not registered (anymore) because otherwise an OS_GenerateError
	@ straight jumps in our code again.
	@ Cfr. PRM 1-294 & 1-295.
	@ a1 contains an index to the error block to report.
	NAME	__exit_with_error_num
__exit_with_error_num:
	ldr	a2, .L7				@ error_table

	PIC_LOAD v4

 PICEQ "LDR	a2, [v4, a2]"

	CMP	a1, #(error_table_end - error_table) >> 2
	MOVCS	a1, #ERR_UNRECOVERABLE
	LDR	a1, [a2, a1, LSL #2]
	@ FIXME this comment no longer correct?: Once we deregister the client, the mapped data segment will cease to
	@ exist, so we need to read the error table first. The errors themselves
	@ are in the text section, so we're OK there.
 PICEQ "SWI	XSOM_DeregisterClient"
#ifdef __ARM_EABI__
	SWI	XARMEABISupport_Cleanup
#endif
	SWI	OS_GenerateError
.L7:
	WORD error_table
	DECLARE_FUNCTION __exit_with_error_num

	.data

	@ Make sure there are no absolute addresses in shared library by
	@ placing error_table in read/write segment and addressing via GOT.
	.globl	error_table
error_table:
	.word	error_no_memory
	.word	error_no_callaswi
	.word	error_no_sharedunixlib
	.word	error_no_fpe
	.word	error_unrecoverable_loop
	.word	error_too_low_cpu_arch
	.word	error_no_vfp
	.word	error_bad_vfp
	.word	error_no_stack
error_table_end:
	DECLARE_OBJECT error_table

	.text

error_no_callaswi:
	.word	SharedUnixLibrary_Error_NoCallASWI
	.asciz	"Module CallASWI is not present"
	.align
	DECLARE_OBJECT error_no_callaswi
error_no_memory:
	.word	SharedUnixLibrary_Error_NotEnoughMem
	.asciz	"Insufficient memory for application"
	.align
	DECLARE_OBJECT error_no_memory
error_no_sharedunixlib:
	.word	SharedUnixLibrary_Error_NotRecentEnough
	.ascii	"This application requires at least version "
	.ascii	SUL_VERSION_STRING
	.asciz	" of the SharedUnixLibrary module"
	.align
	DECLARE_OBJECT error_no_sharedunixlib
error_no_fpe:
#if !defined(__SOFTFP__) && !defined(__VFP_FP__)
	.word	SharedUnixLibrary_Error_NoFPE
	.ascii	"This application requires version 4.00 "
	.asciz	"or later of the FPEmulator module"
	.align
#endif
	DECLARE_OBJECT error_no_fpe
error_unrecoverable_loop:
	.word	SharedUnixLibrary_Error_FatalError
	.asciz	"Unrecoverable fatal error detected"
	.align
	DECLARE_OBJECT error_unrecoverable_loop
error_too_low_cpu_arch:
	.word	SharedUnixLibrary_Error_TooLowCPUArch
	.ascii	"Build settings used to create this program require at least "
	.ascii	"an ARM Architecture "
	.ascii	__ARM_ARCH_STR__
	.asciz	" CPU at runtime."
	.align
	DECLARE_OBJECT error_too_low_cpu_arch
error_no_vfp:
#if !defined(__SOFTFP__) && defined(__VFP_FP__)
	.word	SharedUnixLibrary_Error_NoVFP
	.ascii	"This program requires version 0.02 "
	.asciz	"or later of the VFPSupport module"
	.align
#endif
	DECLARE_OBJECT error_no_vfp
error_bad_vfp:
#if !defined(__SOFTFP__) && defined(__VFP_FP__)
	.word	SharedUnixLibrary_Error_BadVFP
#if defined(__ARM_NEON__)
	.ascii	"VFP/NEON "
#else
	.ascii	"VFP "
#endif
	.asciz	"feature check failure. This program is not compatible with this CPU."
	.align
#endif
	DECLARE_OBJECT error_bad_vfp
error_no_stack:
	.word	SharedUnixLibrary_Error_NoStack
	.asciz	"Unable to allocate stack."
	.align
	DECLARE_OBJECT error_no_stack

	.text

	.global	__dynamic_area_exit
	NAME	__dynamic_area_exit
__dynamic_area_exit:
 PICNE "STMFD	sp!, {lr}"
 PICEQ "STMFD	sp!, {v4, lr}"

	PIC_LOAD v4

	LDR	a1, .L1			@=__dynamic_area_refcount
 PICEQ "LDR	a1, [v4, a1]"
	LDR	a2, [a1]
	SUBS	a2, a2, #1		@ Decrement __dynamic_area_refcount,
	STR	a2, [a1]		@ and only remove the areas when the
 PICNE "LDMFDNE	sp!, {pc}"		@ count reaches zero
 PICEQ "LDMFDNE	sp!, {v4, pc}"

	BL	__munmap_all

	LDR	a2, .L1+4		@=__ul_global
 PICEQ "LDR	a2, [v4, a2]"
	LDR	a2, [a2, #GBL_DYNAMIC_NUM]
	MOV	a1, #1
	CMP	a2, #-1
	SWINE	XOS_DynamicArea
 PICEQ "LDMFD	sp!, {v4, pc}"
 PICNE "LDMFD	sp!, {pc}"
.L1:
	WORD	__dynamic_area_refcount
	WORD	__ul_global
	DECLARE_FUNCTION __dynamic_area_exit

	@ Restore original RISC OS environment handlers
	.global	__env_riscos
	NAME __env_riscos
__env_riscos:
	STMFD	sp!, {v1, v2, lr}

	PIC_LOAD ip

	SWI	XOS_IntOff
	MOV	v1, #0
	LDR	v2, .L2			@=__calling_environment
 PICEQ "LDR	v2, [ip, v2]"
t04:
	MOV	a1, v1
	LDMIA	v2!, {a2, a3, a4}
	TEQ	a1, #0		@ Don't restore the himem limit or the
	TEQNE	a1, #14		@ appspace limit, as SUL handles these for us
	SWINE	XOS_ChangeEnvironment
	ADD	v1, v1, #1
	CMP	v1, #17		@  __ENVIRONMENT_HANDLERS
	BCC	t04
	SWI	XOS_IntOn
	LDMFD	sp!, {v1, v2, pc}
.L2:
	WORD	__calling_environment
	DECLARE_FUNCTION __env_riscos

	@ Get current environment handler setup
	NAME	__env_read
__env_read:
	STMFD	sp!, {a1, a2, a3, a4, v1, v2, lr}

	PIC_LOAD ip

	MOV	v1, #0
	LDR	v2, .L8			@=__calling_environment
 PICEQ "LDR	v2, [ip, v2]"
t05:
	MOV	a1, v1
	MOV	a2, #0
	MOV	a3, #0
	MOV	a4, #0
	SWI	XOS_ChangeEnvironment
	STMIA	v2!, {a2, a3, a4}
	ADD	v1, v1, #1
	CMP	v1, #17
	BCC	t05
	LDMFD	sp!, {a1, a2, a3, a4, v1, v2, pc}
.L8:
	WORD	__calling_environment
	DECLARE_FUNCTION __env_read

	@ Install the UnixLib environment handlers
	.global	__env_unixlib
	NAME	__env_unixlib
__env_unixlib:
	STMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, lr}

	PIC_LOAD ip

	SWI	XOS_IntOff

	MOV	v1, #0
 PICNE "ADR	v2, handlers"

 PICEQ "LDR	v2, .L3+8"		@ handlers
 PICEQ "LDR	v2, [ip, v2]"
	LDR	v3, .L3+4	@=__ul_global
 PICEQ "LDR	v3, [ip, v3]"
t06:
	MOV	a1, v1
	LDR	a2, [v2], #4

	@ Pass a ptr to the RMA block in r12/r0 where possible
	CMP	a2, #0
	LDRNE	a3, [v3, #GBL_PTH_CALLEVERY_RMA]
	MOVEQ	a3, #0

	MOV	a4, #0
	TEQ	v1, #6		@ Error handler ?
	LDREQ	a4, .L3		@=__ul_errbuf
 PICEQ "LDREQ	a4, [ip, a4]"
	TEQ	v1, #7		@ CallBack handler ?
	LDREQ	a4, [v3, #GBL_PTH_CALLEVERY_RMA] @ callback regs are the first thing in the block
	SWI	XOS_ChangeEnvironment
	ADD	v1, v1, #1
	CMP	v1, #16
	BCC	t06

	MOV	a1, #16
	LDR	a2, [v3, #GBL_UPCALL_HANDLER_ADDR]
	LDR	a3, [v3, #GBL_UPCALL_HANDLER_R12]
	MOV	a4, #0
	SWI	XOS_ChangeEnvironment

	SWI	XOS_IntOn
	LDMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, pc}
.L3:
	WORD	__ul_errbuf
	WORD	__ul_global
 PICEQ "WORD	handlers"
	DECLARE_FUNCTION __env_unixlib

#if PIC
	.data
#endif

	@ In the shared library, we can't have absolute addresses in the read only
	@ segment or else we'll end up with text relocations, therefore handlers
	@ has to go in the read/write segment.
handlers:
	.word	0		@ Memory limit
	.word	__h_sigill	@ Undefined instruction
	.word	__h_sigsegv0	@ Prefetch abort
#ifdef __ARM_EABI__
	@ FIXME: If we install an exception handler here, it interferes with
	@ the one in ARMEABISupport. Need to use ARMEABISupport to install a Unixlib one.
	.word	0		@ Data abort
#else
	.word	__h_sigsegv1	@ Data abort
#endif
	.word	__h_sigbus	@ Address exception
	.word	0		@ Other exception
	.word	__h_error	@ Error
	.word	__h_cback	@ CallBack
	.word	0		@ Breakpoint
	.word	__h_sigint	@ Escape
	.word	__h_event	@ Event
	.word	__h_exit	@ Exit
	.word	0		@ Unknown SWI  FIXME: why not __h_sigsys ?
	.word	0		@ Exception registers
	.word	0		@ Application space
	.word	0		@ Currently active object
	.word	0		@ UpCall

#if PIC
	.text
#endif

#if __UNIXLIB_CHUNKED_STACK
	@ Same as the SCL's magic number, for compatibility in libgcc
__stackchunk_magic_number:
	.word	0xF60690FF

	@ Allocate 512 bytes more stack
	@ Round up to 4K as stack is only allocated in multiples of 4K
	.global	__rt_stkovf_split_small
	NAME	__rt_stkovf_split_small
__rt_stkovf_split_small:
	@ This must store the same regs as for __rt_stkovf_split_big
 PICNE "STMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, lr}"
 PICEQ "STMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, v4, lr}"

	MOV	v1, #4096
	B	stack_overflow_common


	@ Allocate stack to below <ip>
	.global	__rt_stkovf_split_big
	NAME	__rt_stkovf_split_big
__rt_stkovf_split_big:
	CMP	ip, sl		@ sanity check
	MOVPL	pc, lr
 PICNE "STMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, lr}"
 PICEQ "STMFD	sp!, {a1, a2, a3, a4, v1, v2, v3, v4, lr}"
	@ This must store the same regs as for __rt_stkovf_split_small
	@ and if changed, also update the 8*4 in the frame size
	@ calculation below

	SUB	v1, sp, ip	@ Get required frame size
 PICNE "ADD	v1, v1, #8*4"	@ Take account of the earlier STMFD (static library)
 PICEQ "ADD	v1, v1, #9*4"	@ Take account of the earlier STMFD (shared library)

	@ Size of chunk header and space for stack extension procedures
	ADD	v1, v1, #512+CHUNK_OVERHEAD
	ADD	v1, v1, #0xFF0	@round up to nearest 4KB
	ADD	v1, v1, #0x00F
	BIC	v1, v1, #0xFF0
	BIC	v1, v1, #0x00F
	@ Fall through !

	@ The rest is common to both split_small and split_big
	@ Thread-safe, other than the __stackalloc/free calls
	@ v1 = extra size needed.
stack_overflow_common:
	PIC_LOAD v4

	@ The signal handler stack chunk can't be extended.
	LDR	a1, .L4			@=__ul_global
 PICEQ "LDR	a1, [v4, a1]"
	LDR	a1, [a1, #GBL_EXECUTING_SIGNALHANDLER]
	TEQ	a1, #0
	BNE	signalhandler_overflow

#if __UNIXLIB_EXTREMELY_PARANOID
	BL	__check_stack
#endif

	@ Find bottom of chunk
	SUB	v2, sl, #512+CHUNK_OVERHEAD
#if __UNIXLIB_STACK_CHECK_MAGIC
	LDR	a1, __stackchunk_magic_number
	LDR	a2, [v2, #CHUNK_MAGIC]
	TEQ	a1, a2
	BNE	stack_corrupt
#endif

	MOV	v3, sp	@ Store old sp
	@ We know there is enough stack to call stackalloc or stackfree, so
	@ sl just needs to be set to a value that won't cause a recursive
	@ stack overflow
	SUB	sl, sp, #512

	LDR	a1, [v2, #CHUNK_NEXT]
	TEQ	a1, #0
	@ There isn't a spare chunk, so we need to alloc a new one
	BEQ	alloc_new_chunk

#if __UNIXLIB_STACK_CHECK_MAGIC
	LDR	a3, __stackchunk_magic_number
	LDR	a4, [a1, #CHUNK_MAGIC]
	TEQ	a3, a4
	BNE	stack_corrupt
#endif
	LDR	a2, [a1, #CHUNK_SIZE]
	CMP	v1, a2
	MOVLS	v1, a2
	BLS	use_existing_chunk	@ Spare chunk is big enough, so use it

	@ Spare chunk is too small, so free it and alloc another one
	BL	__free_stack_chain

alloc_new_chunk:
	MOV	a1, v1
	BL	__stackalloc
	TEQ	a1, #0
	BEQ	raise_sigstak		@ No free memory

	LDR	a3, __stackchunk_magic_number
	STR	a3, [a1, #CHUNK_MAGIC]

	@ Add chunk into linked list
	STR	a1, [v2, #CHUNK_NEXT]
	STR	v2, [a1, #CHUNK_PREV]

use_existing_chunk:
	@ a1 contains the new/existing chunk to be used, v1 contains its size.
	MOV	a2, #0
	STR	a2, [a1, #CHUNK_NEXT]
	STR	v1, [a1, #CHUNK_SIZE]

	ADD	sp, a1, v1	@ New sp is the top of the new chunk
	ADD	sl, a1, #512+CHUNK_OVERHEAD

	LDR	a2, .L4+4	@=__stackfree
 PICEQ "LDR	a2, [v4, a2]"
	STR	a2, [a1, #CHUNK_DEALLOC] @ For compatibility with scl, in libgcc

	LDR	a2, [fp, #-4]	@ Load the return address
	STR	a2, [a1, #CHUNK_RETURN]	@ Remember it

	ADR	a3, __free_stack_chunk
	STR	a3, [fp, #-4]	@ Replace it with our chunk free procedure

 PICNE "LDMFD	v3, {a1, a2, a3, a4, v1, v2, v3, pc}"
 PICEQ "LDMFD	v3, {a1, a2, a3, a4, v1, v2, v3, v4, pc}"
.L4:
	WORD	__ul_global
	WORD	__stackfree

raise_sigstak:
	@ The 256 bytes left on the stack aren't enough for the signal
	@ handler so setup a larger stack
	MOV	v1, fp
	MOV	v2, sp
	MOV	v3, lr
	BL	__setup_signalhandler_stack

	ADR	v4,__rt_stkovf_split_small+12	@ Point at function name
	STMFD	sp!, {v1, v2, v3, v4}	@ Setup an APCS-32 stack frame to
	ADD	fp, sp, #12		@ link to the old stack

	MOV	a1, #0
	MOV	a2, #SIGSTAK
	BL	__unixlib_raise_signal
	B	_exit		@ __unixlib_raise_signal shouldn't return

#if __UNIXLIB_EXTREMELY_PARANOID || __UNIXLIB_STACK_CHECK_MAGIC
stack_corrupt_msg:
	.asciz	"***Fatal error: Stack corruption detected***"
	.align
stack_corrupt:
	ADR	a1, stack_corrupt_msg
	B	__unixlib_fatal
#endif

signalhandler_overflow_msg:
	.asciz	"***Fatal error: Stack overflow in signal handler***"
	.align
signalhandler_overflow:
	ADR	a1, signalhandler_overflow_msg
	B	__unixlib_fatal
	DECLARE_FUNCTION __rt_stkovf_split_small
	DECLARE_FUNCTION __rt_stkovf_split_big

#if __UNIXLIB_EXTREMELY_PARANOID
	@ Check every stack chunk in the chain to ensure it contains
	@ sensible values.
	.global	__check_stack
	.hidden __check_stack
	NAME	__check_stack
__check_stack:
	MOV	ip, sp
 PICNE "STMFD	sp!, {a1, a2, a3, a4, v1, v2, fp, ip, lr, pc}"
 PICEQ "STMFD	sp!, {a1, a2, a3, a4, v1, v2, v4, fp, ip, lr, pc}"
	SUB	fp, ip, #4

	PIC_LOAD v4

	LDR	a1, .L5				@=__ul_global
 PICEQ "LDR	a1, [v4, a1]"
	LDR	a1, [a1, #GBL_PTH_SYSTEM_RUNNING]
	TEQ	a1, #0
	BLNE	__pthread_disable_ints
	LDR	a1, .L5+4			@=__ul_memory
 PICEQ "LDR	a1, [v4, a1]"
	LDR	a2, [a1, #MEM_STACK]		@ __ul_memory.stack
	LDR	a3, [a1, #MEM_APPSPACE_HIMEM]	@ __ul_memory.appspace_limit
	LDR	a4, [a1, #MEM_ROBASE]		@ __ul_memory.robase

	SUB	a1, sl, #512+CHUNK_OVERHEAD
__check_stack_l1:
	CMP	a1, #0
	BEQ	__check_stack_l2
	BL	__check_chunk
	LDR	a1, [a1, #CHUNK_NEXT]
	B	__check_stack_l1

__check_stack_l2:
	SUB	a1, sl, #512+CHUNK_OVERHEAD
__check_stack_l3:
	CMP	a1, #0
	BNE	__check_stack_l4
	LDR	a1, .L5				@=__ul_global
 PICEQ "LDR	a1, [v4, a1]"
	LDR	a1, [a1, #GBL_PTH_SYSTEM_RUNNING]
	CMP	a1, #0
	BLNE	__pthread_enable_ints
 PICNE "LDMEA	fp, {a1, a2, a3, a4, v1, v2, fp, sp, pc}"
 PICEQ "LDMEA	fp, {a1, a2, a3, a4, v1, v2, v4, fp, sp, pc}"
.L5:
	WORD	__ul_global
	WORD	__ul_memory
__check_stack_l4:
	BL	__check_chunk
	LDR	a1, [a1, #CHUNK_PREV]
	B	__check_stack_l3

	@ a1 = chunk
	@ a2 = __ul_memory.stack
	@ a3 = __ul_memory.appspace_himem
	@ a4 = __ul_memory.robase
__check_chunk:
	CMP	a1, a2
	BCC	stack_corrupt
	CMP	a1, a3
	BCS	stack_corrupt

	LDR	v1, [a1, #CHUNK_MAGIC]
	LDR	v2, __stackchunk_magic_number
	TEQ	v1, v2
	BNE	stack_corrupt

	LDR	v1, [a1, #CHUNK_SIZE]
	CMP	v1, #0x1000
	BCC	stack_corrupt
	SUB	v2, a3, a2
	CMP	v1, v2
	BHI	stack_corrupt

	LDR	v1, [a1, #CHUNK_NEXT]
	CMP	v1, #0
	BEQ	__check_chunk_l1
	CMP	v1, a2
	BCC	stack_corrupt
	CMP	v1, a3
	BCS	stack_corrupt
__check_chunk_l1:
	LDR	v1, [a1, #CHUNK_PREV]
	TEQ	v1, #0
	@ If the previous chunk is zero then dealloc and return
	@ may also be zero
	BEQ	__check_chunk_l2

	CMP	v1, a2
	BCC	stack_corrupt
	CMP	v1, a3
	BCS	stack_corrupt

	LDR	v1, [a1, #CHUNK_DEALLOC]
	TEQ	a1, a1
	TEQ	pc, pc	@ EQ if in a 32-bit mode, NE if 26-bit
	BICNE	v1, v1, #0xfc000003
	CMP	v1, a4
	BCC	stack_corrupt

	@ In the shared library, the address of the dealloc function
	@ can be higher than application space.
 PICNE "CMP	v1, a3"
 PICNE "BCS	stack_corrupt"

	LDR	v1, [a1, #CHUNK_RETURN]
	TEQ	a1, a1
	TEQ	pc, pc	@ EQ if in a 32-bit mode, NE if 26-bit
	BICNE	v1, v1, #0xfc000003
	CMP	v1, a4
	BCC	stack_corrupt

	@ In the shared library, the chunk return address can be
	@ higher than application space.
 PICNE "CMP	v1, a3"
 PICNE "BCS	stack_corrupt"

__check_chunk_l2:
	MOV	pc, lr
	DECLARE_FUNCTION __check_stack
#endif


	@ Remove any unused stack chunks
	.global	__trim_stack
__trim_stack:
	SUB	a2, sl, #512 + CHUNK_OVERHEAD	@ Find bottom of chunk
	@ FIXME: a2/sl value come from a jmpbuf buffer so it would be a good
	@ idea to validate this somehow.
	LDR	a1, [a2, #CHUNK_NEXT]
	TEQ	a1, #0
	MOVEQ	pc, lr
	MOV	a3, #0
	STR	a3, [a2, #CHUNK_NEXT]
	@ Falls through to __free_stack_chain

	@ Free a chain of stack chunks pointer to by a1
	.global	__free_stack_chain
__free_stack_chain:
	STMFD	sp!, {v1, lr}
__free_stack_chain_l1:
	@ Spare chunk is too small, so free it and alloc another one
#if __UNIXLIB_STACK_CHECK_MAGIC
	MOV	a2, #0	@ Invalidate the magic number
	STR	a2, [a1, #CHUNK_MAGIC]
#endif
	LDR	v1, [a1, #CHUNK_NEXT]
	BL	__stackfree
	MOVS	a1, v1
	BNE	__free_stack_chain_l1
	LDMFD	sp!, {v1, pc}
	DECLARE_FUNCTION __free_stack_chain
	DECLARE_FUNCTION __trim_stack


	.global	__free_stack_chunk
__free_stack_chunk:
	@ Called on exit from a function that caused stack extension
	@ a3-a4, ip, sl and lr can be corrupted, all others (including sp)
	@ must be preserved. GCC uses a1 & a2 to return 64bit results.
	@ Stack chunks are freed with one chunk of latency,
	@ so we need to free the previously unused chunk, not the chunk that
	@ has just been finished with. Therefore the chunk just finished
	@ with can be used as a stack for this function
#if __UNIXLIB_EXTREMELY_PARANOID
	BL	__check_stack
#endif
	SUB	sl, sl, #512+CHUNK_OVERHEAD	@ sl = start of current chunk
	LDR	ip, [sl, #CHUNK_SIZE]
	ADD	ip, sl, ip		@ ip = top of chunk
	STMFD	ip!, {a1, a2, fp, sp}	@ stack important regs
	MOV	sp, ip	@ set up sp
	@ sl remains at the bottom of the chunk, but there's 4K of space and
	@ __stackfree won't need more than 256 bytes of it so this is ok

#if __UNIXLIB_STACK_CHECK_MAGIC
	LDR	a1, __stackchunk_magic_number
	LDR	a2, [sl, #CHUNK_MAGIC]
	TEQ	a1, a2
	BNE	stack_corrupt
#endif

	LDR	a1, [sl, #CHUNK_NEXT]
	CMP	a1, #0
	BEQ	no_chunk_to_free

#if __UNIXLIB_STACK_CHECK_MAGIC
	MOV	a2, #0	@ Invalidate the magic number
	STR	a2, [a1, #CHUNK_MAGIC]
#endif
	BL	__stackfree
	MOV	a1, #0
	STR	a1, [sl, #CHUNK_NEXT]

no_chunk_to_free:
	LDMFD	sp, {a1, a2, fp, sp}	@ Restore important regs

	LDR	lr, [sl, #CHUNK_RETURN]	@ Get the real return address

	@ Set sl up correctly for the old stack chunk
	LDR	sl, [sl, #CHUNK_PREV]
	ADD	sl, sl, #512+CHUNK_OVERHEAD
	MOV	pc, lr
	DECLARE_FUNCTION __free_stack_chunk
#elif defined(__ARM_EABI__)

	@ FIXME: We can get rid of these all together?
	.global	__rt_stkovf_split_small
	NAME	__rt_stkovf_split_small
__rt_stkovf_split_small:
	MOV	pc, lr
	DECLARE_FUNCTION __rt_stkovf_split_small

	.global	__rt_stkovf_split_big
	NAME	__rt_stkovf_split_big
__rt_stkovf_split_big:
	MOV	pc, lr
	DECLARE_FUNCTION __rt_stkovf_split_big
#endif

	@ Globally used panic button.
	@ void __unixlib_fatal(const char *message) __attribute__ ((__noreturn__))
	@ Protects itself from recursive calling.
	.global	__unixlib_fatal
	NAME	__unixlib_fatal
__unixlib_fatal:
	@ We don't want to assume anything about the stack as the stack
	@ corruption detection routines will call this routine in case
	@ something is wrong.
	PIC_LOAD ip

	LDR	a4, .L6			@=__ul_global
 PICEQ "LDR	a4, [ip, a4]"
	LDR	sp, [a4, #GBL_SIGNALHANDLER_SP]
	MOV	fp, #0
#if __UNIXLIB_CHUNKED_STACK
	LDR	sl, [a4, #GBL_SIGNALHANDLER_SL]
#endif

#ifdef __ARM_EABI__
	STMDB	sp!, {v1, fp, lr}

#ifdef __clang__
	ADD	fp, sp, #8	@ TODO: check this offset is correct
#else
	ADD	fp, sp, #12	@ TODO: check this offset is correct
#endif
#else
	MOV	ip, sp
	STMDB	sp!, {v1, fp, ip, lr, pc}

	SUB	fp, ip, #4
#endif

	@ We've been here before ? If so, we're looping in our fatal
	@ error handling.  As last resort to avoid an infinite loop
	@ we go for a straight OS_Exit scenario.  Anything better we
	@ can do ?
	ADD	a3, a4, #GBL_PANIC_MODE
#ifndef __ARM_EABI__
	LDR	a2, [a4, #GBL_CPU_FLAGS]
	TST	a2, #__CPUCAP_HAVE_SWP
	BEQ	0f

	MOV	a2, #1
	SWP	a2, a2, [a3]
	B	1f
#endif
0:
	LDREX	a2, [a3]
	MOV	ip, #1
	STREX	lr, ip, [a3]
	TEQ	lr, #1
	BEQ	0b
1:
	TEQ	a2, #0
	BEQ	__unixlib_fatal_cont1

	BL	__env_riscos
	MOV	a1, #ERR_UNRECOVERABLE
	B	__exit_with_error_num

	@ a1 => NULL or NUL terminated message to print
__unixlib_fatal_cont1:
	MOVS	v1, a1
	BNE	__unixlib_fatal_got_msg
	__get_errno	v1, a1
	BL	strerror
	MOV	v1, a1

	@ a1 => NUL terminated message to print
__unixlib_fatal_got_msg:
	@ FIXME: in Wimp environment but not when in TaskWindow environment, we could pop
	@ up a Wimp_ReportError box.  Might help the "my program vanishes from the screen
	@ without any trace" case.
	SWI	XOS_NewLine
	MOV	a1, v1
	SWI	XOS_Write0
	SWI	XOS_NewLine
	MOV	a1, #1
	BL	_exit
	@ Should never return
#ifdef __ARM_EABI__
	LDMIA	sp!, {v1, fp, pc}
#else
	LDMDB	fp, {v1, fp, sp, pc}
#endif
.L6:
	WORD	__ul_global
	DECLARE_FUNCTION __unixlib_fatal

	@ int __valid_address (const void *lower, const void *upper)
	@ Return non-zero value when address range <lower> - <upper> (excl)
	@ is a valid address range.
	.global	__valid_address
	NAME	__valid_address
__valid_address:
	SWI	XOS_ValidateAddress
	MOVVS	a1, #0
	MOVVS	pc, lr
	@ If the C flag is clear then the address range is OK
	@ If the C flag is set then the address range is not OK.
	MOVCC	a1, #1
	MOVCS	a1, #0
	MOV	pc, lr
	DECLARE_FUNCTION __valid_address

	.section ".bss"

	.align

	@ This space is reserved for UnixLib to store the environment
	@ handlers of the calling application.
	@ Size: 17 handlers x 3 words x 4 bytes/word = 204 bytes
	.global	__calling_environment
__calling_environment:
	.space	204
	DECLARE_OBJECT __calling_environment

crt1_data:
	.space	4
	DECLARE_OBJECT crt1_data

	.data

	.global	__dynamic_area_refcount
__dynamic_area_refcount:
	.word	1
	DECLARE_OBJECT __dynamic_area_refcount

dynamic_area_name_begin:
	.space	MAX_DA_NAME_SIZE
dynamic_area_name_end:
	.asciz	"$HeapMax"
	.align
	DECLARE_OBJECT dynamic_area_name_begin

	@ In the shared library, these values cannot refer to any absolute
	@ addresses. They are set to zero at compile time and initialised
	@ at runtime where required.
	.global	__ul_memory
__ul_memory:
	.word	0			@ mutex			offset = 0
	.word	0			@ appspace_himem	offset = 4
	.word	0			@ stack			offset = 8
#if PIC
	.word	0			@ robase		offset = 12
	.word	0			@ rwlomem		offset = 16
	.word	0			@ rwbase		offset = 20
#else
	.word	Image$$RO$$Base		@ robase		offset = 12
	.word	Image$$RW$$Limit	@ rwlomem		offset = 16
	.word	Image$$RW$$Base		@ rwbase		offset = 20
#endif
	.word	0			@ rwbreak		offset = 24
	.word	0			@ stack_limit		offset = 28
	.word	0			@ dalomem		offset = 32
	.word	0			@ dabreak		offset = 36
	.word	0			@ dalimit		offset = 40
	.word	0			@ appspace_limit	offset = 44
	.word	0			@ old_himem		offset = 48
	.word	0			@ appspace_himem_max	offset = 52
	DECLARE_OBJECT __ul_memory
