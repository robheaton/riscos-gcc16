@ Global definitions used by all assembler files.
@ Copyright (c) 2002-2016 UnixLib Developers.

@ Bits that control which bits are compiled into UnixLib. Note, this must be
@ kept in sync with <sys/syslib.h>, <signal.h> and <errno.h>.

	@ Include the UnixLib build options.
#include "unixlib/buildoptions.h"

#ifndef __TARGET_SCL__
	@ The offset of various members of the __pthread_thread structure
	@ This should be kept in sync with incl-local/internal/pthread.h.
.set	__PTHREAD_MAGIC_OFFSET, 0
.set	__PTHREAD_CONTEXT_OFFSET, 4
.set	__PTHREAD_ALLOCA_OFFSET, 8
.set	__PTHREAD_ERRNO_OFFSET, 12
.set	__PTHREAD_ERRBUF_OFFSET, 16
.set	__PTHREAD_ERRBUF_VALID_OFFSET, 16+256
#endif

.set	USR_Mode, 0x0
.set	FIQ_Mode, 0x1
.set	IRQ_Mode, 0x2
.set	SVC_Mode, 0x3
.set	Mode_Bits, 0x3

.set	NFlag, 0x80000000	@ Negative flag
.set	ZFlag, 0x40000000	@ Zero flag
.set	CFlag, 0x20000000	@ Carry flag
.set	VFlag, 0x10000000	@ Overflow flag
.set	IFlag, 0x08000000	@ IRQ disable
.set	FFlag, 0x04000000	@ FIRQ disable

.set	USR32_mode, 0b10000
.set	FIQ32_mode, 0b10001
.set	IRQ32_mode, 0b10010
.set	SVC32_Mode, 0b10011
.set	ABT32_Mode, 0b10111
.set	UND32_Mode, 0b11011
.set	SYS32_Mode, 0b11111

.set	IFlag32, 0x00000080

	@ Import handy preprocessor definitions based on build settings
#include "internal/defines.h"
	@ Import the assembler helper macros.
#include "internal/elf-macros.s"

.set	EXIT_SUCCESS, 0
.set	EXIT_FAILURE, 1

#ifndef __TARGET_SCL__
	@ Keep the signal list in sync with <signal.h> contents.
.set	SIGHUP, 1	@   hangup
.set	SIGINT, 2	@   terminal interrupt
.set	SIGQUIT, 3	@ * ^\ from terminal
.set	SIGILL, 4	@ * illegal instruction
.set	SIGTRAP, 5	@ * trace trap - unused
.set	SIGIOT, 6	@   abort
.set	SIGABRT, 6	@   abort
.set	SIGEMT, 7	@ * EMT (h/ware error)
.set	SIGSTAK, SIGEMT
.set	SIGFPE, 8	@ * FPE trap
.set	SIGKILL, 9	@   kill signal
.set	SIGBUS, 10	@ * bus error
.set	SIGSEGV, 11	@ * segmentation violation
.set	SIGSYS, 12	@ * bad system call
.set	SIGPIPE, 13	@   broken pipe
.set	SIGALRM, 14	@   alarm call
.set	SIGTERM, 15	@   termination signal
.set	SIGURG, 16	@   urgent or out-of-band data
.set	SIGSTOP, 17	@   stop, unblockable
.set	SIGTSTP, 18	@   keyboard stop
.set	SIGCONT, 19	@   continue
.set	SIGCHLD, 20	@   child process has terminated/stopped
.set	SIGCLD, SIGCHLD
.set	SIGTTIN, 21	@   background read from tty
.set	SIGTTOU, 22	@   background write to tty
.set	SIGIO, 23	@   file descriptor is ready to perform I/O
.set	SIGPOLL, SIGIO
.set	SIGXCPU, 24	@   CPU time limit exceeded
.set	SIGXFSZ, 25	@   file size limit exceeded
.set	SIGVTALRM, 26	@   alarm call
.set	SIGPROF, 27	@   profiler alarm call
.set	SIGWINCH, 28	@   window size change
.set	SIGINFO, 29	@   information request
.set	SIGUSR1, 30	@   user signal 1
.set	SIGUSR2, 31	@   user signal 2
.set	SIGLOST, 32	@   resource lost
.set	SIGOSERROR, 33	@   RISC OS error

.set	EOPSYS, 88	@ RISC OS error, same value as EOPSYS in errno.h
#endif

@ Make the SUL SWI details available for both SCL and UnixLib target builds:
.set	SharedUnixLibrary_SWIChunk, 0x55c80	@Range 0x55c80 - &55cc0 (excl) is allocated.
.set	SharedUnixLibrary_ErrorChunk, 0x81a400	@Range 0x81a400 - &81a40 (excl) is allocated.
@ The beginning of the error range is used by the SUL module.  The end of
@ the error ranged (allocated from back to front) is used by the UnixLib
@ main code itself (to generate errors during startup when necessary).
.set	SharedUnixLibrary_Error_UnknownSWI, SharedUnixLibrary_ErrorChunk + 0x00
.set	SharedUnixLibrary_Error_UnknownKey, SharedUnixLibrary_ErrorChunk + 0x01
.set	SharedUnixLibrary_Error_StillActive, SharedUnixLibrary_ErrorChunk + 0x02
.set	SharedUnixLibrary_Error_TooOld, SharedUnixLibrary_ErrorChunk + 0x03
@ ...
.set	SharedUnixLibrary_Error_NoStack, SharedUnixLibrary_ErrorChunk + 0x37
.set	SharedUnixLibrary_Error_BadVFP, SharedUnixLibrary_ErrorChunk + 0x38
.set	SharedUnixLibrary_Error_NoVFP, SharedUnixLibrary_ErrorChunk + 0x39
.set	SharedUnixLibrary_Error_TooLowCPUArch, SharedUnixLibrary_ErrorChunk + 0x3A
.set	SharedUnixLibrary_Error_FatalError, SharedUnixLibrary_ErrorChunk + 0x3B
.set	SharedUnixLibrary_Error_NoFPE, SharedUnixLibrary_ErrorChunk + 0x3C
.set	SharedUnixLibrary_Error_NotRecentEnough, SharedUnixLibrary_ErrorChunk + 0x3D
.set	SharedUnixLibrary_Error_NotEnoughMem, SharedUnixLibrary_ErrorChunk + 0x3E
.set	SharedUnixLibrary_Error_NoCallASWI, SharedUnixLibrary_ErrorChunk + 0x3F

.set	OS_WriteC, 0x000000
.set	OS_WriteS, 0x000001
.set	OS_Write0, 0x000002
.set	OS_NewLine, 0x000003
.set	OS_CLI, 0x000005
.set	OS_GetEnv, 0x000010
.set	OS_Exit, 0x000011
.set	OS_EnterOS, 0x000016
.set	OS_ChangeDynamicArea, 0x00002A
.set	OS_GenerateError, 0x00002B
.set	OS_ChangeEnvironment, 0x000040
.set	OS_DynamicArea, 0x000066

.set	X_Bit, 0x20000

.set	XOS_WriteC, 0x000000 + X_Bit
.set	XOS_WriteS, 0x000001 + X_Bit
.set	XOS_Write0, 0x000002 + X_Bit
.set	XOS_NewLine, 0x000003 + X_Bit
.set	XOS_ReadC, 0x000004 + X_Bit
.set	XOS_CLI, 0x000005 + X_Bit
.set	XOS_Byte, 0x000006 + X_Bit
.set	XOS_Word, 0x000007 + X_Bit
.set	XOS_File, 0x000008 + X_Bit
.set	XOS_Args, 0x000009 + X_Bit
.set	XOS_BGet, 0x00000A + X_Bit
.set	XOS_BPut, 0x00000B + X_Bit
.set	XOS_GBPB, 0x00000C + X_Bit
.set	XOS_Find, 0x00000D + X_Bit
.set	XOS_ReadLine, 0x00000E + X_Bit
.set	XOS_Control, 0x00000F + X_Bit
.set	XOS_GetEnv, 0x000010 + X_Bit
.set	XOS_Exit, 0x000011 + X_Bit
.set	XOS_SetEnv, 0x000012 + X_Bit
.set	XOS_IntOn, 0x000013 + X_Bit
.set	XOS_IntOff, 0x000014 + X_Bit
.set	XOS_CallBack, 0x000015 + X_Bit
.set	XOS_EnterOS, 0x000016 + X_Bit
.set	XOS_BreakPt, 0x000017 + X_Bit
.set	XOS_BrealCtrl, 0x000018 + X_Bit
.set	XOS_UpdateMEMC, 0x00001A + X_Bit
.set	XOS_SetCallBack, 0x00001B + X_Bit
.set	XOS_Mouse, 0x00001C + X_Bit
.set	XOS_Heap, 0x00001D + X_Bit
.set	XOS_Module, 0x00001E + X_Bit
.set	XOS_ReadUnsigned, 0x000021 + X_Bit
.set	XOS_ReadVarVal, 0x000023 + X_Bit
.set	XOS_SetVarVal, 0x000024 + X_Bit
.set	XOS_BinaryToDecimal, 0x000028 + X_Bit
.set	XOS_FSControl, 0x000029 + X_Bit
.set	XOS_ChangeDynamicArea, 0x00002A + X_Bit
.set	XOS_ValidateAddress, 0x00003A + X_Bit
.set	XOS_CallAfter, 0x00003B + X_Bit
.set	XOS_CallEvery, 0x00003C + X_Bit
.set	XOS_RemoveTickerEvent, 0x00003D + X_Bit
.set	XOS_ChangeEnvironment, 0x000040 + X_Bit
.set	XOS_ReadMonotonicTime, 0x000042 + X_Bit
.set	XOS_WriteN, 0x000046 + X_Bit
.set	XOS_ReadMemMapInfo, 0x000051 + X_Bit
.set	XOS_AddCallBack, 0x000054 + X_Bit
.set	XOS_SerialOp, 0x000057 + X_Bit
.set	XOS_DynamicArea, 0x000066 + X_Bit
.set	XOS_ClaimProcessorVector, 0x000069 + X_Bit
.set	XOS_PlatformFeatures, 0x00006D + X_Bit
.set	XOS_SynchroniseCodeAreas, 0x00006E + X_Bit
.set	XOS_CallASWI, 0x00006F + X_Bit
.set	XOS_AMBControl, 0x000070 + X_Bit
.set	XOS_CallASWIR12, 0x000071 + X_Bit
.set	XOS_Hardware, 0x00007A + X_Bit
.set	XOS_ConvertHex8, 0x0000D4 + X_Bit
.set	XOS_ConvertInteger1, 0x0000D9 + X_Bit
.set	XOS_ConvertInteger2, 0x0000DA + X_Bit
.set	XOS_ConvertInteger3, 0x0000DB + X_Bit
.set	XOS_ConvertInteger4, 0x0000DC + X_Bit
.set	XOS_WriteI, 0x000100 + X_Bit

.set	XFPEmulator_Version, 0x040480 + X_Bit

.set	XMessageTrans_ErrorLookup, 0x041506 + X_Bit

.set	XTaskWindow_TaskInfo, 0x043380 + X_Bit
.set	XWimp_ReadSysInfo, 0x0400F2 + X_Bit
.set	XWimp_SlotSize, 0x0400EC + X_Bit
.set	XDDEUtils_SetCLSize, 0x042581 + X_Bit

.set	Wimp_SlotSize, 0x0400EC

.set	XFilter_RegisterPreFilter, 0x042640 + X_Bit
.set	XFilter_RegisterPostFilter, 0x042641 + X_Bit
.set	XFilter_DeRegisterPreFilter, 0x042642 + X_Bit
.set	XFilter_DeRegisterPostFilter, 0x042643 + X_Bit

@ Deprecated SWIs:
@.set	XSharedUnixLibrary_RegisterUpCall, 0x55c80 + X_Bit
@.set	XSharedUnixLibrary_DeRegisterUpCall, 0x55c81 + X_Bit
@.set	XSharedUnixLibrary_SetValue, 0x55c82 + X_Bit
@.set	XSharedUnixLibrary_Count, 0x55c83 + X_Bit
.set	SharedUnixLibrary_Initialise, 0x55c84
.set	XSharedUnixLibrary_Initialise, 0x55c84 + X_Bit

@ A few of these are required to build SUL.
.set	XARMEABISupport_MemoryOp, 0x59D00 + X_Bit
.set	XARMEABISupport_AbortOp, 0x59D01 + X_Bit
.set	XARMEABISupport_StackOp, 0x59D02 + X_Bit
.set	XARMEABISupport_Cleanup, 0x59D03 + X_Bit

.set	ARMEABISUPPORT_STACKOP_ALLOC,		0
.set	ARMEABISUPPORT_STACKOP_FREE,		1
.set	ARMEABISUPPORT_STACKOP_GET_STACK,	2
.set	ARMEABISUPPORT_STACKOP_GET_BOUNDS,	3
.set	ARMEABISUPPORT_STACKOP_GET_SIZE,	4

.set	ARMEABISUPPORT_MEMORYOP_NEW_ALLOCATOR,		0
.set	ARMEABISUPPORT_MEMORYOP_DESTROY_ALLOCATOR, 	1
.set	ARMEABISUPPORT_MEMORYOP_ALLOC,			2
.set	ARMEABISUPPORT_MEMORYOP_CLAIM_PAGES,		3
.set	ARMEABISUPPORT_MEMORYOP_RELEASE_PAGES,		4
.set	ARMEABISUPPORT_MEMORYOP_MAP_PAGES,		5
.set	ARMEABISUPPORT_MEMORYOP_UNMAP_PAGES,		6
.set	ARMEABISUPPORT_MEMORYOP_FREE,			7
.set	ARMEABISUPPORT_MEMORYOP_INFO,			8

.set	ARMEABISUPPORT_ABORTOP_REGISTER,		0
.set	ARMEABISUPPORT_ABORTOP_DEREGISTER,		1
.set	ARMEABISUPPORT_ABORTOP_INSTALL,			2
.set	ARMEABISUPPORT_ABORTOP_REMOVE,			3

#if !defined(__SOFTFP__) && defined(__VFP_FP__)
.set	VFPSupport_CheckContext, 0x58ec0
.set	XVFPSupport_CheckContext, 0x58ec0 + X_Bit
.set	VFPSupport_CreateContext, 0x58ec1
.set	XVFPSupport_CreateContext, 0x58ec1 + X_Bit
.set	VFPSupport_DestroyContext, 0x58ec2
.set	VFPSupport_ChangeContext, 0x58ec3
.set	VFPSupport_ExamineContext, 0x58ec4
.set	XVFPSupport_Version, 0x58ec7 + X_Bit
.set	VFPSupport_Features, 0x58ec8
.set	VFPSupport_ExceptionDump, 0x58ec9
#endif
.set	XVFPSupport_ChangeContext, 0x58ec3 + X_Bit	@ Outside the above #if for building SUL

.set	XSOM_DeregisterClient, 0x058584 + X_Bit
.set	XSOM_MapObject, 0x058590 + X_Bit

.set	SULPROC_STATUS_FLAG_IS_FORKED, (1 << 21)
.set	SULPROC_STATUS_FLAG_IS_EXECED, (1 << 22)
.set	SULPROC_STATUS_FLAG_IS_DYNAMIC, (1 << 23)
.set	SULPROC_STATUS_FLAG_IS_ARMEABI, (1 << 24)

#ifndef __TARGET_SCL__
	@ Entries into the struct proc structure.  Must be kept in sync with
	@ incl-local/internal/unix.h.
.set	PROC_ARGC, 0			@ = __u->argc
.set	PROC_ARGV, 4			@ = __u->argv

.set	SULPROC_STATUS, 96
.set	SULPROC_STACK, 124

	@ Entries into the __ul_global structure.  Must be kept in sync with
	@ incl-local/internal/unix.h and sys/_syslib.s.
.set	GBL_UNIXLIB_CLI, 0		@ = __ul_global.cli

.set	GBL_TIME_LOW, 4			@ = __ul_global.time[0]
.set	GBL_TIME_HIGH, 8		@ = __ul_global.time[1]

.set	GBL_REWINDPIPEOFFSET, 12	@ = __ul_global.rewindpipeoffset
.set	GBL_TASKWINDOW, 16		@ = __ul_global.taskwindow
.set	GBL_TASKHANDLE, 20		@ = __ul_global.taskhandle
.set	GBL_DYNAMIC_NUM, 24		@ = __ul_global.dynamic_num
.set	GBL_CHANGEREDIR0, 28		@ = __ul_global.changeredir0
.set	GBL_CHANGEREDIR1, 32		@ = __ul_global.changeredir1
.set	GBL_UL_OUT_PATHNAME, 36		@ = __ul_global.ul_out_pathname
.set	GBL_PANIC_MODE, 40		@ = __ul_global.panic_mode
.set	GBL_SULPROC, 44			@ = __ul_global.sulproc
.set	GBL_PAGESIZE, 48		@ = __ul_global.pagesize
.set	GBL_UPCALL_HANDLER_ADDR, 52	@ = __ul_global.upcall_handler_addr
.set	GBL_UPCALL_HANDLER_R12, 56	@ = __ul_global.upcall_handler_r12

.set	GBL_PTH_RETURN_ADDRESS, 60	@ = __ul_global.pthread_return_address
.set	GBL_PTH_WORKSEMAPHORE_POISON, 64	@ = __ul_global.pthread_worksemaphore
.set	GBL_PTH_CALLBACK_SEMAPHORE_POISON, 68	@ = __ul_global.pthread_callback_semaphore
.set	GBL_PTH_SYSTEM_RUNNING, 72	@ = __ul_global.pthread_system_running
.set	GBL_PTH_CALLBACK_MISSED, 76	@ = __ul_global.pthread_callback_missed
.set	GBL_PTH_NUM_RUNNING_THREADS, 80	@ = __ul_global.pthread_num_running_threads

.set	GBL_EXECUTING_SIGNALHANDLER, 84	@ = __ul_global.executing_signalhandler
.set	GBL_SIGNALHANDLER_SL, 88	@ = __ul_global.signalhandler_sl
.set	GBL_SIGNALHANDLER_SP, 92	@ = __ul_global.signalhandler_sp

.set	GBL_LASTENVIRON, 96		@ = __ul_global.last_environ
.set	GBL_MALLOC_STATE, 100		@ = __ul_global.malloc_state
.set	GBL_ESCAPEDISABLED, 104		@ = __ul_global.escape_disabled
.set	GBL_FLS_LBSTM_ON_RD, 108	@ = __ul_global.fls_lbstm_on_rd
.set	GBL_PTH_CALLEVERY_RMA, 112	@ = __ul_global.pthread_callevery_rma

.set	GBL_CPU_FLAGS, 116		@ = __ul_global.cpu_flags

#if !defined(__SOFTFP__) && defined(__VFP_FP__)
.set	GBL_VFP_REGCOUNT, 120		@ = __ul_global.vfp_regcount
#endif

	@ Entries in the __ul_memory table.  Must be kept in sync with
	@ sys/_syslib.s.
.set	MEM_APPSPACE_HIMEM, 4	@ = __ul_memory.appspace_himem
.set	MEM_STACK, 8		@ = __ul_memory.stack
.set	MEM_ROBASE, 12		@ = __ul_memory.robase (const)
.set	MEM_RWLOMEM, 16		@ = __ul_memory.rwlomem (const)
.set	MEM_RWBASE, 20		@ = __ul_memory.rwbase (const)
.set	MEM_RWBREAK, 24		@ = __ul_memory.rwbreak
.set	MEM_STACK_LIMIT, 28	@ = __ul_memory.stack_limit
.set	MEM_DALOMEM, 32		@ = __ul_memory.dalomem
.set	MEM_DABREAK, 36		@ = __ul_memory.dabreak
.set	MEM_DALIMIT, 40		@ = __ul_memory.dalimit
.set	MEM_APPSPACE_LIMIT, 44	@ = __ul_memory.appspace_limit
.set	MEM_OLD_HIMEM, 48	@ = __ul_memory.old_himem
.set	MEM_APPSPACE_HIMEM_MAX, 52	@ = __ul_memory.appspace_himem_max
#endif

#if __UNIXLIB_CHUNKED_STACK
	@ Entries in the struct __stack_chunk.  Must be kept in sync with
	@ internal/unix.h definition.
	@ First 20 bytes equals SCL's _kernel_stack_chunk structure declared
	@ in kernel.h.
.set	CHUNK_MAGIC, 0		@ Magic number to help detect if someone overwrites the stack
.set	CHUNK_NEXT, 4		@ Ptr to next chunk
.set	CHUNK_PREV, 8		@ Ptr to previous chunk
.set	CHUNK_SIZE, 12		@ Size of chunk, including header
.set	CHUNK_DEALLOC, 16	@ Function to call to free the chunk
#if __TARGET_SCL__
.set	CHUNK_OVERHEAD, 48	@ Size of chunk header
#else
.set	CHUNK_RETURN, 20	@ Return address after freeing this chunk

.set	CHUNK_OVERHEAD, 24	@ Size of chunk header
#endif
#endif

#ifndef __TARGET_SCL__
	@ Entries in the data block passed from crt/(g)crt0.o
.set	CRT1_EXEC_INIT, 0	@ Ptr to program _init section
.set	CRT1_EXEC_FINI, 4	@ Ptr to program _fini section
.set	CRT1_EXEC_START, 8	@ Ptr to R/O base
.set	CRT1_FREE_MEM, 12	@ Ptr to where free memory begins
.set	CRT1_DATA_START, 16	@ Ptr to R/W base
.set	CRT1_MAIN, 20		@ Ptr to main()
.set	CRT1_FLAGS, 24
.set	CRT1_LIB_INIT, 28	@ Ptr to function to initialise shared libaries
				@ before main() called - can be NULL.
.set	CRT1_LIB_FINI, 32	@ Ptr to function to finalise shared libraries
				@ at program exit - can be NULL.
.set	CRT1_INIT_ARRAY_START,	36 @ Ptr to the start of the .init_array section
.set	CRT1_INIT_ARRAY_END, 	40 @ Ptr to the end of the .init_array section
.set	CRT1_FINI_ARRAY_START,	44 @ Ptr to the start of the .fini_array section
.set	CRT1_FINI_ARRAY_END, 	48 @ Ptr to the end of the .fini_array section

	@ Entries in the RMA data block.
						.struct 0
	@ User registers are preserved in here for the callback execution.
	@ User registers = R0-R15 and CPSR in 32-bit mode
	@		 = R0-R15 only in 26-bit mode (the 17th word equals
	@		   the pc value)
PTHREAD_CALLEVERY_CALLBACK_REGS:		.skip 17 * 4
	@ bit 0 Escape condition flag
	@ bit 1 Internet event flag
PTHREAD_CALLEVERY_CALLBACK_FLAG:		.skip 4
	@ Signal number to use for __unixlib_raise_signal().
PTHREAD_CALLEVERY_CALLBACK_A1:			.skip 4
PTHREAD_CALLEVERY_RMA_UPCALL_ADDR:		.skip 4
PTHREAD_CALLEVERY_RMA_UPCALL_R12:		.skip 4
PTHREAD_CALLEVERY_RMA_GOT_PTR:			.skip 4
	@ Have we registered the CallEvery ticker ?
	@ Value 0 : no, CallEvery ticker is not enabled
	@       1 : yes CallEvery ticker is enabled
PTHREAD_CALLEVERY_RMA_TICKER_STARTED:		.skip 4
PTHREAD_CALLEVERY_RMA_WORKSEMAPHORE:		.skip 4
PTHREAD_CALLEVERY_RMA_CALLBACK_SEMAPHORE:	.skip 4
PTHREAD_CALLEVERY_RMA_FILTER_NAME:		.skip 20
PTHREAD_CALLEVERY_STRUCT_SIZE:

	.text
#endif

#define __CPUCAP_HAVE_SWP    0x1
#define __CPUCAP_HAVE_REX    0x2
#define __CPUCAP_HAVE_REXBHD 0x4
