@ SharedUnixLibrary
@ Copyright (c) 2002-2015 UnixLib Developers

#include "internal/asm_dec.s"

#define DEBUG_PROC_MATCHING 0

	.text
	.fpu fpa

@ Structure for clients written for SUL 1.04 and earlier
@ These clients call SharedUnixLibrary_RegisterUpCall to initialise, while
@ later clients call SharedUnixLibrary_Initialise and use the process
@ structure further down
.set	ListNext, 0	@ Pointer to next block
.set	ListHandler, 4	@ UpCall handler function
.set	ListValue, 8	@ Value at UpCall handler function
.set	ListHead, 12	@ Pointer to head of list

.set	ListSize, 16	@ Size of the List structure.

@ Structure for clients written for SUL after v1.04.  This structure happens
@ to be UnixLib struct __sul_process structure followed by internal SUL data
@ for that process.
@ Note that ListNext == PROC_NEXT.
@ See the "struct __sul_process" definition in include/internal/unix.h.
@ All entries upto PROC_SULONLY are accessible by client programs, and should
@ not be altered, to ensure backwards compatibility. Entries after this are
@ private to SUL and can be altered in new versions of SUL without problems.
@ If any new items need to be added to the process structure, they should be
@ added immediately before PROC_SULONLY, and initialised by SUL to suitable
@ default values. New client programs will be able to take advantage of the
@ new entries, while old clients will just ignore them.
.set	PROC_NEXT, 0
.set	PROC_TASKHANDLE, 4
.set	PROC_MALLOC, 8
.set	PROC_FREE, 12
.set	PROC_EXIT, 16
.set	PROC_PID, 20
.set	PROC_PPID, 24
.set	PROC_PGRP, 28
.set	PROC_UID, 32
.set	PROC_EUID, 36
.set	PROC_GID, 40
.set	PROC_EGID, 44
.set	PROC_PPRI, 48
.set	PROC_GPRI, 52
.set	PROC_UPRI, 56
.set	PROC_UMASK, 60
.set	PROC_FDARRAY, 64
.set	PROC_MAXFD, 68
.set	PROC_FDSIZE, 72
.set	PROC_FDHANDLESIZE, 76
.set	PROC_TTYTYPE, 80
.set	PROC_TTYCONSOLE, 84
.set	PROC_TTYRS423, 88
.set	PROC_ENVIRON, 92
.set	PROC_STATUS, 96
.set	PROC_CHILDREN, 100
.set	PROC_NEXTCHILD, 104
.set	PROC_FORK, 108
.set	PROC_ENVIRONSIZE, 112
.set	PROC_EXEC, 116
.set	PROC_WIMPSLOT, 120
.set	PROC_STACK, 124

.set	PROC_SULONLY, 128

.set	PROC_PARENTSULPROC, PROC_SULONLY + 0	@ Process structure of this process' parent
.set	PROC_PARENTSULPROCADDR, PROC_SULONLY + 4	@ Address of the parent's __ul_global.sulproc variable
.set	PROC_CLI, PROC_SULONLY + 8		@ String to pass to OS_CLI on an exec
.set	PROC_OLDEXIT, PROC_SULONLY + 12		@ Old handler values to restire after an exec
.set	PROC_OLDEXITR12, PROC_SULONLY + 16
.set	PROC_OLDERROR, PROC_SULONLY + 20
.set	PROC_OLDERRORR12, PROC_SULONLY + 24
.set	PROC_OLDERRORBUF, PROC_SULONLY + 28
.set	PROC_INITIALAPPSPACE, PROC_SULONLY + 32	@ Original appspace limit when the process was created
.set	PROC_INITIALHIMEM, PROC_SULONLY + 36	@ Original memory limit when the process was created
.set	PROC_PARENTSTACK, PROC_SULONLY + 40	@ The value of the process' __ul_memory.stack
.set	PROC_PARENTADDR, PROC_SULONLY + 44	@ The address the parent has been copied up to
.set	PROC_PARENTSIZE1, PROC_SULONLY + 48	@ The size of the first part of the parent (0x8000 to __ul_memory.stack_limit)
.set	PROC_PARENTSIZE2, PROC_SULONLY + 52	@ The size of the second part of the parent (__ul_memory.stack_limit to himem)
.set	PROC_PRIVATEWORD, PROC_SULONLY + 56	@ The private word of this module, so we can access it from an exit or error handler
.set	PROC_FORK_STORAGE, PROC_SULONLY + 60
	@ Storage for registers to restore on return from a fork/vfork:
	@   f4, f5, f6, f7
	@   {v1-v6, sl, fp, ip, lr}
.set	PROC_SIZE, PROC_FORK_STORAGE + 4*12 + 10*4


@ The size of a __unixlib_fd struct
.set	FDSIZE, 12
@ The size of a __unixlib_fd_handle struct
.set	FDHANDLESIZE, 12

@ Offset of the refcount field in the __unixlib_fd_handle struct
.set	DEVICEHANDLE_REFCOUNT, 0
@ Offset of the devicehandle field in the __unixlib_fd struct
.set	FD_DEVICEHANDLE, 0
@ Offset of the refcount field in the tty struct
.set	TTY_REFCOUNT, 4

@ The default size of the fdarray
@ In future, clients could potentially dynamically increase the size of the
@ array when needed
.set	DEFAULT_MAXFD, 256

@ Bits within the status word
.set	RETURNCODE_MASK, 0xFF
.set	ZOMBIE_BIT, 0x80000

@ The minimum about of free memory to ensure before execing a child
@ Ideally, we would determine this from the size of the binary we are about
@ to run, but getting this information is not easy, so guess.
.set	MIN_EXEC_MEM, 200*1024

@ This is the earliest version of SUL whose structures we support
@ Any application written against an earlier version will get an error returned
@ As only one version of SUL can be loaded at once, we should attempt to
@ support old clients for as long as possible. Hence this value should
@ rarely if ever change.
.set	EARLIEST_SUPPORTED_VERSION, 105


module_start:
	.word	0				@ Start code
	.word	init_code - module_start	@ Initialisation code
	.word	final_code - module_start	@ Finalisation code
	.word	0				@ Service handler
	.word	title - module_start		@ Title string
	.word	help - module_start		@ Help string
	.word	0				@ Command table
	.word	SharedUnixLibrary_SWIChunk	@ SWI Base
	.word	swi_handler - module_start	@ SWI Handler
	.word	swi_table - module_start	@ SWI Table
	.word	0				@ SWI Decoder
	.word	0				@ Messages
	.word	module_flags - module_start	@ Module flags

help:
	.ascii	"SharedUnixLibrary"
	.byte	9
	.ascii	"1.16 (3 Apr 2020) "
#if DEBUG_PROC_MATCHING
	.ascii	"(debug) "
#endif
	.asciz	"(C) UnixLib Developers, 2001-2020"
	.align

title:
swi_table:
	.asciz	"SharedUnixLibrary"
	.asciz	"RegisterUpCall"
	.asciz	"DeRegisterUpCall"
	.asciz	"SetValue"
	.asciz	"Count"
 	.asciz	"Initialise"
	.byte	0
	.align

error_swi:
	.word	SharedUnixLibrary_Error_UnknownSWI
	.asciz	"Unknown SharedUnixLibrary SWI call"
	.align

error_unknown:
	.word	SharedUnixLibrary_Error_UnknownKey
	.asciz	"Unknown SharedUnixLibrary Key"
	.align

error_active:
	.word	SharedUnixLibrary_Error_StillActive
	.asciz	"There are still SharedUnixLibrary clients active"
	.align

error_tooold:
	.word	SharedUnixLibrary_Error_TooOld
	.asciz	"Client too old to be supported by SharedUnixLibrary"
	.align

module_flags:
	.word	1      @ 32-bit compatible

swi_handler:
	LDR	r12, [r12]
	CMP	r11, #5
	ADDCC	pc, pc, r11, LSL#2
	B	swi_handler_unknownswi
	B	swi_register
	B	swi_deregister
	B	swi_value
	B	swi_count
	B	swi_initialise
swi_handler_unknownswi:
	ADR	r0, error_swi
	TEQ	pc, pc
	ORRNES	pc, lr, #VFlag
	MSR	CPSR_f, #VFlag
	MOV	pc, lr


	@ Register an application with the module.
	@ This call is kept for backwards compatibility, new applications
	@ should call SharedUnixLibrary_Initialise instead.
	@
	@ This SWI registers an application as an intended recipient of UpCalls
	@ After calling this method, it should then call OS_ChangeEnvironment
	@ with R0 = 16, and values returned from this SWI.
	@
	@ The application may deregister its interest by calling
	@ DeRegister_UpCall, ; such as during an application's Exit and
	@ Error handlers, or for other reasons.
	@
	@ It is important that the application does so, as the module will not
	@ exit if any claimants remain.
	@
	@ The is no need to deregister if UpCall 256 occurs; the module will do
	@ that automatically.
	@
	@ Entry
	@    R1 = Address of application's handler function
	@    R2 = Value at address, for sanity checking
	@
	@ Exit
	@    R1 = Address of UpCall handler to pass in R1 to
	@         OS_ChangeEnvironment
	@    R2 = SharedUnixLibrary key, required for deregistration, and
	@         passed in R2 to OS_ChangeEnvironment


swi_register:
	STMFD	sp!, {r0, r3, lr}

	MOV	r0, #6
	MOV	r3, #ListSize
	SWI	XOS_Module		@ Claim memory to use

	LDMVSFD sp!, {r2, r3, pc}	@ Return to caller if error

	@ result in R2, return to caller at end
	LDR	r0, [r12]		@ Get head of list
	STR	r0, [r2,  #ListNext]	@ Store in new block
	STR	r2, [r12]		@ Set to head of list

	STR	r1, [r2, #ListHandler]	@ Set handler function address
	MOV	r1, #0
	STR	r1, [r2, #ListValue]	@ Set value at handler
	STR	r12, [r2, #ListHead]	@ Set pointer to head of list

	@ return UpCall handler address in R1
	ADR	r1, upcall_handler

	LDMFD	sp!, {r0, r3, pc}


	@ Application UpCall handler, PRM 1-291
	@
	@ The address of this handler is passed back via The RegisterUpCall SWI.
	@ The caller application then provides that address to
	@ OS_ChangeEnvironment
	@ Having this handler in a module enables it to be validly called when
	@ the application isn't paged in.  This is despite the fact that most of
	@ the UpCalls we are not interested in at all.

upcall_handler:
	TEQ	r0, #256		@ New Application UpCall
	MOVNE	pc, lr			@ Otherwise exit quickly

	MOV	r0, #0			@ Prevent new application from starting
	MOV	pc, lr			@ This is easier than ensuring the
					@ handler frees all resources correctly


	@ Deregister a previously registered handler
	@
	@ Entry
	@    R2 = Key provided when registering
	@
	@ Exit
	@    R2 = 1
	@
	@ An error will be generated if the key is invalid, for example
	@ if the handler has already been deregistered.

swi_deregister:
	STMFD	sp!, {r0-r1, lr}
	BL	delink
	CMP	r2, #0
	BEQ	deregister_error

	MOV	r0, #7
	SWI	XOS_Module		@ Free block

	LDMFD	sp!, {r0-r1, pc}

deregister_error:
	LDMFD	sp!, {r0-r1, lr}

	ADR	r0, error_unknown	@ Unable to find key
	TEQ	r0, r0
	TEQ	pc, pc
	ORRNES	pc, lr, #VFlag			@ Return error (26bit)
	MSR	CPSR_f, #VFlag
	MOV	pc, lr				@ Return error (32bit)


	@ Delink the handler
	@
	@ Entry
	@    R2 = Key
	@    sp may not be valid
	@
	@ Exit
	@    R2 - Key = Success, 0 = Key not found


delink:
	@ Try to find reference
	MOV	r1, r12			@ Set to previous point in list
	LDR	r0, [r12]		@ Get first item in list

delink_find:
	CMP	r0, #0
	MOVEQ	r2, #0
	MOVEQ	pc, lr			@ End of list, not found

	CMP	r0, r2			@ Compare passed key
	MOVNE	r1, r0			@ Set to current point in list
	LDR	r0, [r0, #ListNext]	@ Get next item
	BNE	delink_find

	STR	r0, [r1, #ListNext]	@ Fix up next pointer at previous point in list

	MOV	pc, lr


	@ Set value at handler address
	@
	@
	@ Entry
	@    R0 = Key provided when registering
	@    R1 = Value
	@
	@ Exit
	@    Registers preserved
swi_value:
	STMFD   sp!, {lr}

	MOV	r14, r12

value_find:
	LDR	r14, [r14, #ListNext]	@ ListNext == PROC_NEXT
	CMP	r14, #0
	LDMEQFD	sp!, {pc}
	CMP	r14, r0
	BNE	value_find
	STR	r1, [r14, #ListValue]
	LDMFD	sp!, {pc}



	@ Return number of current clients in R0
swi_count:
	STMFD	sp!, {lr}

	MOV	r0, #0
	MOV	r14, r12

count_find:
	LDR	r14, [r14, #ListNext]	@ ListNext == PROC_NEXT
	CMP	r14, #0
	LDMEQFD	sp!, {pc}
	ADD	r0, r0, #1
	B	count_find

	@ Module initialisation
	@
	@ Allocate the initial list pointer rather than use the private word,
	@ so code running in user mode doesn't have to access the private word.
init_code:
	STMFD	sp!, {lr}
	MOV	r0, #6
	MOV	r3, #4
	SWI	XOS_Module
	MOVVC	r0, #0
	STRVC	r0, [r2]
	STRVC	r2, [r12]
	LDMFD	sp!, {pc}


	@ Module finalisation
	@
	@ Only allow exit if there are no claimants
final_code:
	STMFD	sp!, {lr}

	LDR	r0, [r12]
	LDR	r0, [r0]
	TEQ	r0, #0			@ Is head of list set?
	BNE	final_code_err		@ Yes, give error and don't die

	MOV	r0, #7			@ Delete the initial list pointer
	LDR	r2, [r12]
	SWI	XOS_Module
	LDMFD	sp!, {pc}

	@ Set error and disallow module finalisation
final_code_err:
	ADR	r0, error_active
	LDMFD	sp!, {lr}
	@ TEQ	r0, r0			@ Not needed as we're in SVC mode.
	TEQ	pc, pc
	ORRNES	pc, lr, #VFlag		@ Return error (26bit)
	MSR	CPSR_f, #VFlag
	MOV	pc, lr			@ Return error (32bit)


	@ SharedUnixLibrary_Initialise SWI
	@
	@ Entry
	@    R0 = Expected SUL version
	@
	@ Exit
	@    R0 = Address of process structure
	@    R1 = Address of upcall handler
	@    R2 = R12 value to use for upcall handler
	@
	@ Can return with an error.
swi_initialise:
	CMP	r0, #EARLIEST_SUPPORTED_VERSION
	BHS	version_ok

	@ The client was expecting an earlier version of SUL than we are
	@ able to support, so we must refuse to initialise it.
	ADR	r0, error_tooold
	TEQ	r0, r0
	TEQ	pc, pc
	ORRNES	pc, lr, #VFlag			@ Return error (26bit)
	MSR	CPSR_f, #VFlag
	MOV	pc, lr				@ Return error (32bit)

version_ok:
	STMFD   sp!, {v1, v2, lr}

	@ See if there is a proc structure already waiting for us
	@ This will be the case if the client was run as the result of an
	@ exec() call
	MOV	a1, #5
	SWI	XWimp_ReadSysInfo
	MOVVC	v1, a1
	MOVVS	v1, #0

	LDR	v2, [ip]
	TEQ	v2, #0
	BEQ	alloc_proc

	@ The taskhandles match, but we still need to determine if we are the
	@ first program to be called since this process structure was created.
	@ Otherwise, if a non-UnixLib program was called by the original
	@ parent, which then calls this program, we don't want to pick up the
	@ process structure as it was intended for use by the non-UnixLib
	@ program.
	SWI	XOS_GetEnv
	MOVVS	pc, lr
#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"OS_GetEnv: "
	.align
	MOV	a3, a1
	SWI	OS_Write0
	MOV	a1, a3
	SWI	OS_NewLine
#endif

	@ In the case of the command being ADFS::Disc.$.foo then some versions
	@ of RISC OS will treat the ADFS: as a temporary filing system
	@ selection and unhelpfully remove it from the command line provided
	@ by OS_GetEnv.
	@ So we have to skip the FS name before comparing if this is the case.
	LDRB	lr, [a1]
	TEQ	lr, #':'
	BEQ	tmp_fs_loop1_done
	MOV	a2, a1
tmp_fs_loop1:
	LDRB	lr, [a2], #1
	CMP	lr, #' '
	BLE	tmp_fs_loop1_done	@ reached end of argv[0] without finding ':'
	TEQ	lr, #':'
	BNE	tmp_fs_loop1
	MOV	a1, a2
tmp_fs_loop1_done:

#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"OS_GetEnv (no FS): "
	.align
	MOV	a3, a1
	SWI	OS_Write0
	MOV	a1, a3
	SWI	OS_NewLine
#endif

	@ a1 => OS_GetEnv CLI line without FS
findproc_loop:
	LDR	a2, [v2, #PROC_TASKHANDLE]
	TEQ	v1, a2
	BNE	try_next

	LDR	a2, [v2, #PROC_CLI]

#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	" -> proc loop, cli: "
	.align
	MOV	a3, a1
	MOV	a1, a2
	SWI	OS_Write0
	MOV	a1, a3
	SWI	OS_NewLine
#endif

	@ Do the same with the canonicalised exec filename:
	LDRB	a3, [a2]
	TEQ	a3, #':'
	BEQ	tmp_fs_loop2_done
	MOV	a4, a2
tmp_fs_loop2:
	LDRB	a3, [a4], #1
	CMP	a3, #' '
	BLE	tmp_fs_loop2_done
	TEQ	a3, #':'
	BNE	tmp_fs_loop2
	MOV	a2, a4
tmp_fs_loop2_done:

#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"    cli (no FS): "
	.align
	MOV	a3, a1
	MOV	a1, a2
	SWI	OS_Write0
	MOV	a1, a3
	SWI	OS_NewLine
#endif

	@ Compare OS_GetEnv cli (without FS) and exec filename (also without
	@ FS). Stop comparing when encountering a space or control character.
cli_loop:
	LDRB	a3, [a1], #1
	CMP	a3, #' '
	MOVLE	a3, #0
	LDRB	a4, [a2], #1
	CMP	a4, #' '
	MOVLE	a4, #0
	TEQ	a3, a4
	BNE	try_next
	TEQ	a3, #0
	BNE	cli_loop

	@ If we get here, the two CLI strings matched, so use this
	@ process struct.  This means we have a UnixLib compiled program
	@ exec'd from from a fork'd UnixLib compiled program.

#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"** Matched proc"
	.align
	SWI	OS_NewLine
#endif

	@ Reset himem to appspace. We don't record the old values as this
	@ could be an exec of an exec'd process and on exit we need to
	@ restore the limits as they were at the start of the first
	@ exec'd process.
	MOV	a1, #14
	MOV	a2, #0
	SWI	XOS_ChangeEnvironment
	MOVVC	a1, #0
	SWIVC	XOS_ChangeEnvironment
	B	invalidate_handle

try_next:
#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"** Go next proc"
	.align
	SWI	OS_NewLine
#endif

	LDR	v2, [v2, #PROC_NEXT]
	TEQ	v2, #0
	BNE	findproc_loop

alloc_proc:
#if DEBUG_PROC_MATCHING
	SWI	OS_WriteS
	.asciz	"** New proc"
	.align
	SWI	OS_NewLine
#endif

	@ No existing process struct was found, so allocate a new one
	MOV	a1, #6
	LDR	a4, =PROC_SIZE
	SWI	XOS_Module
	LDMVSFD	sp!, {v1, v2, pc}
	MOV	v2, a3

	LDR	a1, [ip]		@ Get head of list
	STR	a1, [v2,  #PROC_NEXT]	@ Store in new block
	STR	v2, [ip]		@ Set to head of list
	STR	ip, [v2,  #PROC_PRIVATEWORD]	@ Store the module's private word

	@ Initialise the structure

	@ As we are using the result of OS_Module for the pid value,
	@ we shrink it to 30-bits because 'pid_t' is a signed-integer
	@ and many applications just test for (pid < 0) to determine fork
	@ failure.
	MOV	a1, v2, LSR #2
	STR	a1, [v2, #PROC_PID]
	MOV	a1, #1
	STR	a1, [v2, #PROC_PPID]
	STR	a1, [v2, #PROC_UID]
	STR	a1, [v2, #PROC_EUID]
	STR	a1, [v2, #PROC_GID]
	STR	a1, [v2, #PROC_EGID]
	MOV	a1, #2
	STR	a1, [v2, #PROC_PGRP]
	MOV	a1, #0
	STR	a1, [v2, #PROC_PPRI]
	STR	a1, [v2, #PROC_GPRI]
	STR	a1, [v2, #PROC_UPRI]
	STR	a1, [v2, #PROC_STATUS]
	STR	a1, [v2, #PROC_CHILDREN]
	STR	a1, [v2, #PROC_NEXTCHILD]
	STR	a1, [v2, #PROC_TTYTYPE]
	STR	a1, [v2, #PROC_TTYCONSOLE]
	STR	a1, [v2, #PROC_TTYRS423]
	STR	a1, [v2, #PROC_FDARRAY]
	STR	a1, [v2, #PROC_ENVIRON]
	STR	a1, [v2, #PROC_ENVIRONSIZE]
	STR	a1, [v2, #PROC_PARENTSULPROC]
	STR	a1, [v2, #PROC_OLDEXIT]
	STR	a1, [v2, #PROC_OLDEXITR12]
	STR	a1, [v2, #PROC_OLDERROR]
	STR	a1, [v2, #PROC_OLDERRORR12]
	STR	a1, [v2, #PROC_OLDERRORBUF]
	STR	a1, [v2, #PROC_CLI]
	STR	a1, [v2, #PROC_PARENTADDR]
	MOV	a1, #18		@ 022
	STR	a1, [v2, #PROC_UMASK]
	MOV	a1, #FDSIZE
	STR	a1, [v2, #PROC_FDSIZE]
	MOV	a1, #FDHANDLESIZE
	STR	a1, [v2, #PROC_FDHANDLESIZE]
	MOV	a1, #DEFAULT_MAXFD
	STR	a1, [v2, #PROC_MAXFD]
	ADR	a1, sul_malloc
	STR	a1, [v2, #PROC_MALLOC]
	ADR	a1, sul_free
	STR	a1, [v2, #PROC_FREE]
	ADRL	a1, sul_exit
	STR	a1, [v2, #PROC_EXIT]
	ADR	a1, sul_fork
	STR	a1, [v2, #PROC_FORK]
	ADRL	a1, sul_exec
	STR	a1, [v2, #PROC_EXEC]
	ADR	a1, sul_wimpslot
	STR	a1, [v2, #PROC_WIMPSLOT]
	MOV	a1, #0
	STR	a1, [v2, #PROC_STACK]

	@ Record the current appspace and himem values, then set himem
	@ to the appspace limit. This is necessary so any calls to
	@ Wimp_SlotSize don't fail.
	@ The process will have already recorded the old value of himem,
	@ so it won't overwrite its parent.
	MOV	a1, #14
	MOV	a2, #0
	SWI	XOS_ChangeEnvironment
	MOVVS	a2, #0
	STR	a2, [v2, #PROC_INITIALAPPSPACE]
	MOV	a1, #0
	SWI	XOS_ChangeEnvironment
	MOVVS	a2, #0
	STR	a2, [v2, #PROC_INITIALHIMEM]

invalidate_handle:
	@ v2 = found, or newly allocated, __ul_global.sulproc (struct __sul_process)

	@ Invalidate the taskhandle so any children of this process don't
	@ pick it up
	MOV	a1, #-1
	STR	a1, [v2, #PROC_TASKHANDLE]

	@ Return the address of the process structure and the upcall handler
	MOV	a1, v2
	ADRL	a2, upcall_handler
	MOV	a3, v2
	LDMFD   sp!, {v1, v2, pc}


@
@ void *sul_malloc (pid_t pid, size_t size);
@
@ Allocate some memory. The client should use this to allocate anything that
@ gets linked to from the process structure.
@
@ On exit, a2-a4 can be corrupted.
@
sul_malloc:
	STMFD	sp!, {lr}
	MOV	a4, a2
	MOV	a1, #6
	SWI	XOS_Module
	MOVVS	a1, #0
	MOVVC	a1, a3
	LDMFD	sp!, {pc}


@
@ void sul_free (pid_t pid, void *ptr);
@
@ Free memory allocated by sul_malloc
@
@ On exit, a1-a4 can be corrupted.
@
sul_free:
	STMFD	sp!, {lr}
	MOV	a3, a2
	MOV	a1, #7
	SWI	XOS_Module
	LDMFD	sp!, {pc}


@
@ void *sul_wimpslot (pid_t pid, void *newslot);
@
@ Increase the wimpslot to the address newslot.
@ Returns the new value, or NULL on error
@
@ On exit, a2-a4 can be corrupted.
@
sul_wimpslot:
	STMFD	sp!, {lr}
	SUB	a1, a2, #0x8000
	MOV	a2, #-1
	SWI	XWimp_SlotSize
	MOVVS	a1, #0
	ADDVC	a1, a1, #0x8000
	LDMFD	sp!, {pc}


@
@ void sul_memmove (void *dest, const void *src, size_t size);
@
@ Internal implementation of memmove.
@ May not have a stack present when called.
@ Can corrupt a1-a4, must not corrupt ip
@ Addresses and size must be word aligned.
@
sul_memmove:
	CMP	a1, a2
	BHI	copy_from_top
copy_from_bot:
	LDR	a4, [a2], #4
	STR	a4, [a1], #4
	SUBS	a3, a3, #4
	BNE	copy_from_bot
	MOV	pc, lr
copy_from_top:
	ADD	a1, a1, a3
	ADD	a2, a2, a3
copy_from_top_loop:
	LDR	a4, [a2, #-4]!
	STR	a4, [a1, #-4]!
	SUBS	a3, a3, #4
	BNE	copy_from_top_loop
	MOV	pc, lr


@
@ pid_t sul_fork (pid_t pid, struct __sul_process **proc, void *stacklimit, void *stack);
@
@ Perform a fork or vfork
@ If stacklimit and stack are non-null then a copy of the process
@ will be made (fork) otherwise no copy will be made (vfork)
@ The memory between stacklimit and stack is unused by the process and hence
@ doesn't need to be preserved.
@ Updates proc to point to the child or parent process struct as appropriate.
@ Returns -1 if unable to allocate memory.
@
sul_fork:
	@ Allocate the new child process structure
	MOV	ip, sp
	MOV	a1, a1, LSL #2 @ expand pid to 32-bits
	STMFD	sp!, {a1-a4}

	LDR	a4, =PROC_SIZE
	MOV	a1, #6
	SWI	XOS_Module
	MOVVS	a1, #-1
	MOVVS	pc, lr

	@ Save registers that need restoring when we return as the parent
	ADD	a1, a3, #PROC_FORK_STORAGE
	SFM	f4, 4, [a1], #4*12
	STMIA	a1, {v1-v6, sl, fp, ip, lr}

	MOV	v2, a3
	LDMFD	sp!, {v1, v3-v5}
	@ At this point:
	@   v1 = parent's __ul_global.sulproc ptr
	@   v2 = ptr new child's struct __sul_process
	@   v3 = & __ul_global.sulproc
	@   v4 = stacklimit
	@   v5 = stack

	@ Copy everything except the fork storage from parent to child
	MOV	a1, v2
	MOV	a2, v1
	LDR	a3, =PROC_FORK_STORAGE
	BL	sul_memmove

	@ Fix process IDs
	MOV	a1, v1, LSR #2 @ shrink parent pid to 30 bits
	STR	a1, [v2, #PROC_PPID]
	MOV	a1, v2, LSR #2 @ shrink child pid to 30 bits
	STR	a1, [v2, #PROC_PID]

	@ Add to SULs linked list
	STR	v2, [v1, #PROC_NEXT]

	@ Add to the parent's linked list of all its children
	LDR	a1, [v1, #PROC_CHILDREN]
	STR	a1, [v2, #PROC_NEXTCHILD]
	STR	v2, [v1, #PROC_CHILDREN]
	MOV	a1, #0
	STR	a1, [v2, #PROC_CHILDREN]

	@ Copy the environment
	LDR	a1, [v1, #PROC_ENVIRON]
	TEQ	a1, #0
	BEQ	skip_environ
	MOV	a1, #6
	LDR	a4, [v1, #PROC_ENVIRONSIZE]
	SWI	XOS_Module
	BVS	fork_error1
	STR	a3, [v2, #PROC_ENVIRON]
	MOV	a1, a3
	LDR	a2, [v1, #PROC_ENVIRON]
	MOV	a3, a4
	BL	sul_memmove
skip_environ:

	@ Copy the file descriptor array
	LDR	a2, [v1, #PROC_MAXFD]
	LDR	a3, [v1, #PROC_FDSIZE]
	MUL	a4, a2, a3
	MOV	a1, #6
	SWI	XOS_Module
	BVS	fork_error2
	STR	a3, [v2, #PROC_FDARRAY]
	LDR	a2, [v1, #PROC_FDARRAY]
	MOV	a1, a3
	MOV	a3, a4
	BL	sul_memmove

	@ Duplicate all file descriptors
	LDR	a1, [v2, #PROC_FDARRAY]
	LDR	a2, [v2, #PROC_MAXFD]
	LDR	a3, [v2, #PROC_FDSIZE]
fd_loop:
	LDR	a4, [a1, #FD_DEVICEHANDLE]
	TEQ	a4, #0
	LDRNE	ip, [a4, #DEVICEHANDLE_REFCOUNT]
	ADDNE	ip, ip, #1
	STRNE	ip, [a4, #DEVICEHANDLE_REFCOUNT]
	ADD	a1, a1, a3
	SUBS	a2, a2, #1
	BNE	fd_loop

	@ Increment tty struct refcounts
	LDR	a1, [v2, #PROC_TTYCONSOLE]
	TEQ	a1, #0
	LDRNE	a2, [a1, #TTY_REFCOUNT]
	ADDNE	a2, a2, #1
	STRNE	a2, [a1, #TTY_REFCOUNT]

	LDR	a1, [v2, #PROC_TTYRS423]
	TEQ	a1, #0
	LDRNE	a2, [a1, #TTY_REFCOUNT]
	ADDNE	a2, a2, #1
	STRNE	a2, [a1, #TTY_REFCOUNT]

	@ Clear various fields that are only valid for the parent
	MOV	a1, #0
	STR	a1, [v2, #PROC_OLDEXIT]
	STR	a1, [v2, #PROC_OLDEXITR12]
	STR	a1, [v2, #PROC_OLDERROR]
	STR	a1, [v2, #PROC_OLDERRORR12]
	STR	a1, [v2, #PROC_OLDERRORBUF]
	STR	a1, [v2, #PROC_CLI]
	STR	a1, [v2, #PROC_INITIALAPPSPACE]
	STR	a1, [v2, #PROC_INITIALHIMEM]
	STR	a1, [v2, #PROC_PARENTADDR]

	@ Save a reference to the parent proc struct
	STR	v1, [v2, #PROC_PARENTSULPROC]
	STR	v3, [v2, #PROC_PARENTSULPROCADDR]
	STR	v2, [v3] @ Update the processes __ul_global.sulproc

	@ If this was due to a fork then take a copy of the parent
	MOV	a1, #0
	MOVS	a2, v4
	MOV	a3, v5
	BLNE	copy_up_parent
	TEQ	a1, #0
	BNE	fork_error3

	@ Return as the child
	@ a1 will already be 0 from copy_up_parent
	ADD	a2, v2, #PROC_FORK_STORAGE
	LFM	f4, 4, [a2], #4*12
	LDMIA	a2, {v1-v6, sl, fp, sp, pc}

	@ Something has gone wrong, so free anything we have allocated before
	@ returning an error
fork_error3:
	MOV	a1, #7
	LDR	a3, [v2, #PROC_FDARRAY]
	SWI	XOS_Module
fork_error2:
	MOV	a1, #7
	LDR	a3, [v2, #PROC_ENVIRON]
	SWI	XOS_Module
fork_error1:
	MOV	a3, v2

	ADD	a2, v2, #PROC_FORK_STORAGE
	LFM	f4, 4, [a2], #4*12
	LDMIA	a2, {v1-v6, sl, fp, sp, lr}

	MOV	a1, #7
	SWI	XOS_Module

	MOV	a1, #-1
	MOV	pc, lr


@ Copy up the parent to the top of the wimpslot, increasing the wimpslot
@ if necessary
@ a1 = min_exec_mem / 0
@ a2 = stacklimit
@ a3 = stack
@ v2 = __ul_global.sulproc
@ on exit, v2 preserved, a1-v1, v3-v6, ip corrupted
@ returns 0 in a1 on success, -1 on error
copy_up_parent:
	MOV	ip, lr	@ The stack won't be valid as we are about to move it
	MOV	v3, a1
	MOV	v1, a2
	MOV	v4, a3

	@ Read current appspace limit
	MOV	a1, #14
	MOV	a2, #0
	SWI	XOS_ChangeEnvironment
	MOVVS	a1, #-1
	MOVVS	pc, ip
	MOV	v5, a2

	@ Calculate the size of the two sections we need to move
	STR	v4, [v2, #PROC_PARENTSTACK]
	SUB	a1, v5, v4
	STR	a1, [v2, #PROC_PARENTSIZE2]
	SUB	a1, v1, #0x8000
	STR	a1, [v2, #PROC_PARENTSIZE1]

	@ v1 = stacklimit
	@ v2 = __ul_global.sulproc
	@ v3 = min mem
	@ v4 = stack
	@ v5 = appspace

	@ Calculate how much more wimpslot we need
	SUB	a2, v4, v1	@ Amount already spare
	MOVS	v1, v3
	SUBEQ	v1, v5, #0x8000	@ Retain all the current space, for a fork
	SUBS	a2, v1, a2	@ Amount we want - amount spare
	MOVLE	v6, v5
	BLE	no_increase
	@ a2 = amount to increase by
	ADD	v6, v5, a2
	@ Align address to 4k
	ADD	v6, v6, #0x0FF
	ADD	v6, v6, #0xF00
	BIC	v6, v6, #0x0FF
	BIC	v6, v6, #0xF00
	SUB	a1, v6, #0x8000
	MOV	a2, #-1
	SWI	XWimp_SlotSize
	MOVVS	a1, #-1
	MOVVS	pc, ip

	@ Move the top section
	@ v6 = new appspace ; v5 = old appspace
	LDR	a3, [v2, #PROC_PARENTSIZE2]
	LDR	a2, [v2, #PROC_PARENTSTACK]
	SUB	a1, v6, a3
	BL	sul_memmove

no_increase:
	@ Move the bottom section
	@ v6 = new appspace ; v5 = old appspace
	STR	v6, [v2, #PROC_INITIALAPPSPACE]

	@ Move the bottom section
	LDR	a3, [v2, #PROC_PARENTSIZE1]
	LDR	a4, [v2, #PROC_PARENTSIZE2]
	MOV	a2, #0x8000
	SUB	a1, v6, a4
	SUB	a1, a1, a3
	STR	a1, [v2, #PROC_PARENTADDR]
	STR	a1, [v2, #PROC_INITIALHIMEM]
	BL	sul_memmove

	@ Set himem if called for a vfork-exec
	TEQ	v3, #0
	MOV	a1, #0
	LDR	a2, [v2, #PROC_INITIALHIMEM]
	SWINE	XOS_ChangeEnvironment

	MOV	a1, #0
	MOV	pc, ip


@
@ Copy down parent from top of wimpslot to its original location
@ v2 = __ul_global.sulproc, preserved
@ a1-a4, ip corrupted
@ sp may not be valid
copy_down_parent:
	MOV	ip, lr

	@ Move the bottom section
	MOV	a1, #0x8000
	LDR	a2, [v2, #PROC_PARENTADDR]
	LDR	a3, [v2, #PROC_PARENTSIZE1]
	BL	sul_memmove

	@ Move the top section
	LDR	a1, [v2, #PROC_PARENTSTACK]
	LDR	a2, [v2, #PROC_PARENTADDR]
	LDR	a4, [v2, #PROC_PARENTSIZE1]
	ADD	a2, a2, a4
	LDR	a3, [v2, #PROC_PARENTSIZE2]
	BL	sul_memmove

	@ Flush instruction cache
	MOV	a1, #1
	MOV	a2, #0x8000
	LDR	a3, [v2, #PROC_PARENTSTACK]
	LDR	a4, [v2, #PROC_PARENTSIZE2]
	ADD	a3, a3, a4
	@ Save the new appspace limit, which can then be restored by
	@ restore_wimpslot
	STR	a3, [v2, #PROC_INITIALAPPSPACE]
	STR	a3, [v2, #PROC_INITIALHIMEM]
	SWI	XOS_SynchroniseCodeAreas

	MOV	pc, ip

@
@ Restore the wimpslot, appspace and himem limits to what they were at
@ process creation
@ v2 = process structure, preserved
@ Corrupts a1-v1
@ sp may not be valid
restore_wimpslot:
	LDR	a1, [v2, #PROC_INITIALAPPSPACE]
	SUB	a1, a1, #0x8000
	MOV	a2, #-1
	SWI	XWimp_SlotSize

	MOV	a1, #14
	LDR	a2, [v2, #PROC_INITIALAPPSPACE]
	SWI	XOS_ChangeEnvironment

	MOV	a1, #0
	LDR	a2, [v2, #PROC_INITIALHIMEM]
	SWI	XOS_ChangeEnvironment

	MOV	pc, lr

runpath_str:
	.asciz	"Run$Path"
	.align

@
@ void sul_exec (pid_t pid, char *cli, void *stacklimit, void *stack)
@   __attribute__ ((__noreturn__))
@
@ Execute a program or *command
@ cli should be allocated with sul_malloc, we will free it.
@ Memory between stack limit and stack is free and can be used by us.
@
@ Never returns, so all registers can be corrupted
@ FIXME: we're assuming sp is valid (sul_malloc, sul_free, etc), is that ok ?
sul_exec:
	MOV	v3, a1, LSL #2	@ expand pid to 32-bits
	MOV	v4, a3		@ v4 = stacklimit
	MOV	v5, a4		@ v5 = stack

	@ Save the cli, and free any old value
	@ But first canonicalise the program name part of the cli and see if
	@ that results in a real file.
	MOV	v6, a2
find_end_program_loop:
	LDRB	v7, [v6], #1
	CMP	v7, #32
	BGT	find_end_program_loop
	@ v6 => cli arguments
	@ v7 < ' ' : there are no cli arguments
	@    = ' ' : space in front of the cli arguments

	@ Calculate cli argument length
	SUB	v8, v6, #1
cli_arg_len_loop:
	LDRB	lr, [v8], #1
	CMP	lr, #32
	BGE	cli_arg_len_loop
	SUB	v8, v8, v6	@ v8 = length cli argument + term char
	ADD	v8, v8, #1	@ v8 = length cli argument + term char + 1

	MOV	lr, #0
	STRB	lr, [v6, #-1]

	MOV	a1, #37
	@ MOV	a2, a2
	MOV	a3, #0		@ Zero ptr buffer
	ADR	a4, runpath_str
	MOV	v1, #0
	MOV	v2, #0		@ Zero length buffer
	SWI	XOS_FSControl
	BVS	restore_cli	@ Don't give error (good idea ?) and continue FIXME: why not use OS_GenerateError which terminates the parent ?

	@ Allocate sul memory to hold canonicalised program name + cli arguments
	STMFD	sp!, {a2}
	MOV	a1, v3, LSR #2
	SUB	v2, v8, v2
	ADD	a2, v2, #1
	BL	sul_malloc	@ a1-a4 can get corrupted
	LDMFD	sp!, {a2}
	MOVS	a3, a1
	BEQ	restore_cli	@ FIXME: better error reporting needed ?

	MOV	a1, #37
	ADR	a4, runpath_str
	ADD	v2, v2, #1
	SWI	XOS_FSControl
	BVS	release_rma_and_restore_cli

	@ When the canonicalised filename does not exist, we have to fall back
	@ on the original sul_exec() argument given as it might be an OSCLI
	@ one.
	STMFD	sp!, {a2, a3}
	MOV	a1, #5
	MOV	a2, a3
	SWI	XOS_File
	LDMFD	sp!, {a2, a3}
	BVS	release_rma_and_restore_cli
	TEQ	a1, #1		@ Is it a file or image file ?
	TEQNE	a1, #3
	BNE	release_rma_and_restore_cli

	CMP	v7, #32
	BLT	swap_cli

	@ Add cli arguments
	MOV	a4, a3		@ Find end canonicalised program name
find_end_canon_program_loop:
	LDRB	lr, [a4], #1
	CMP	lr, #32
	BGT	find_end_canon_program_loop
	STRB	v7, [a4, #-1]	@ Write ' '
add_cli_arguments:		@ Add the cli arguments
	LDRB	lr, [v6], #1
	STRB	lr, [a4], #1
	CMP	lr, #32
	BGE	add_cli_arguments

swap_cli:
	@ a2 non canonicalised cli (sul_malloced)
	@ a3 canonicalised cli (sul_malloced)
	MOV	v1, a3
	MOV	a1, v3
	BL	sul_free	@ a1-a4 can get corrupted

	MOV	a2, v1
	B	update_cli_in_proc

	@ a2 = sul_malloced memory block containing the cli
	@ a3 = sul_malloced memory block we no longer need so we free it here.
release_rma_and_restore_cli:
	MOV	v1, a2
	MOV	a1, v3
	MOV	a2, a3
	BL	sul_free	@ a1-a4 can get corrupted
	MOV	a2, v1

	@ a2 = sul_malloced memory block containing the cli
restore_cli:
	STRB	v7, [v6, #-1]

	@ a2 = sul_malloced memory block containing the cli
update_cli_in_proc:
	LDR	a3, [v3, #PROC_CLI]
	TEQ	a3, #0
	MOVNE	a1, #7
	SWINE	XOS_Module
	STR	a2, [v3, #PROC_CLI]

	@ From here onwards, v2 = pid_t
	MOV	v2, v3

	@ Record the taskhandle, so if the child calls SUL_Initialise then
	@ we can match it with the correct process structure
	MOV	a1, #5
	SWI	XWimp_ReadSysInfo
	MOVVS	a1, #0
	STR	a1, [v2, #PROC_TASKHANDLE]

	@ If this exec was called from an already exec'd child then we don't
	@ need to set up new exit and error handlers, as the old ones are
	@ still valid
	LDR	a1, [v2, #PROC_OLDEXIT]
	TEQ	a1, #0
	BNE	skip_handler_setup

	@ Set up exit and error handlers so we get control back when
	@ the program exits
	MOV	a1, #11
	ADR	a2, exit_handler
	MOV	a3, v2
	SWI	XOS_ChangeEnvironment
	STR	a2, [v2, #PROC_OLDEXIT]
	STR	a3, [v2, #PROC_OLDEXITR12]

	@ We can use the bottom of the wimpslot for the error buffer as we
	@ know that the parent will be at least MIN_EXEC_MEM above 0x8000
	MOV	a1, #6
	ADR	a2, error_handler
	MOV	a3, v2
	MOV	a4, #0x8000
	SWI	XOS_ChangeEnvironment
	STR	a2, [v2, #PROC_OLDERROR]
	STR	a3, [v2, #PROC_OLDERRORR12]
	STR	a4, [v2, #PROC_OLDERRORBUF]

skip_handler_setup:
	LDR	a1, [v2, #PROC_PPID]
	TEQ	a1, #1
	BEQ	no_copy_needed	@ We don't have a parent
	LDR	a1, [v2, #PROC_PARENTADDR]
	TEQ	a1, #0
	BNE	no_copy_needed	@ The parent is already copied

	@ We have a parent that needs copying, i.e. we were created by a vfork
	MOV	a1, #MIN_EXEC_MEM
	MOV	a2, v4
	MOV	a3, v5
	BL	copy_up_parent
	TEQ	a1, #0
	ADRNE	a1, error_no_mem
	MOVNE	ip, v2
	BNE	print_error
	B	do_exec

no_copy_needed:
	@ Restore the old wimpslot value, as we don't need the extra memory
	@ if any was allocated. Also sets himem so any parent won't get
	@ overwritten.
	BL	restore_wimpslot

do_exec:
	@ Ensure the new program doesn't inherit any VFP context
	MOV	a1, #0
	MOV	a2, #0
	SWI	XVFPSupport_ChangeContext
#if DEBUG_PROC_MATCHING
	LDR	a1, [v2, #PROC_CLI]
	SWI	OS_WriteS
	.asciz	"About to exec: "
	.align
	SWI	XOS_Write0
	SWI	XOS_NewLine
#endif
	LDR	a1, [v2, #PROC_CLI]
	SWI	OS_CLI

	@ If we created an DDE extended command line and OSCLI has failed
	@ (probably due to insufficient memory to load program), or returned
	@ from a built-in command, then it is likely that nothing has read
	@ (and hence cleared) the extended command line. We must do it before
	@ it confuses any other program.
	@ You wouldn't credit it, but there is actually a race condition in
	@ DDEUtils!
	@ It only holds *one* extended command line, so it's bad luck if
	@ your task is building the command only for another to issue OSCLI
	@ before you!
	MOV	a1, #0
	SWI	XDDEUtils_SetCLSize

	MOV     a1, v2
	MOV     a2, #0
	B       exit_error_handler_common

error_handler: @ r0 = __ul_global.sulproc
	MOV	ip, a1
	MOV	a1, #0x8000
	ADD	a1, a1, #8

print_error:
	@ The child generated an error. There is not really any way we can
	@ return it to the parent program, but we don't want to lose it so
	@ just print it out.
	SWI	XOS_Write0
	SWI	XOS_NewLine

	MOV	a1, #0
	SWI	XDDEUtils_SetCLSize

	MOV	a1, ip
	MOV	a2, #1
	B	exit_error_handler_common

	@ r12 = __ul_global.sulproc
exit_handler:
	@ Read the return code and convert to an integer (why on earth doesn't
	@ RISC OS store it as a numeric variable?)
	@ We can use the bottom of the wimpslot as a temporary buffer as the
	@ program has finished with it
	ADR	a1, sysretcode
	MOV	a2, #0x8000
	MOV	a3, #4
	MOV	a4, #0
	MOV	v1, #3
	SWI	XOS_ReadVarVal
	MOV	a1, ip
	MOVVS	a2, #1
	BVS	exit_error_handler_common
	MOV	v2, #0x8000
	MOV	a4, #1
	MOV	a2, #0
convert_loop:
	SUBS	a3, a3, #1
	BLT	exit_error_handler_common
	LDRB	v1, [v2], #1
	SUB	v1, v1, #48
	MUL	v1, a4, v1
	ADD	a2, a2, v1
	MOV	v3, #10
	MUL	a4, v3, a4
	B	convert_loop

exit_error_handler_common:
	@ a1 = __ul_global.sulproc, a2 = returncode
	@ Save the return code
	AND	a3, a2, #RETURNCODE_MASK
	STR	a3, [a1, #PROC_STATUS]
	@ Fall though to sul_exit
	MOV	a1, a1, LSR #2 @ shrink pid to 30-bits

@
@ sul_exit (__pid_t pid, int status);
@
@ Called on exit by any process that has called SUL_Initialise,
@ and by any process created by sul_fork. Also called directly
@ from the exit handler above.
@ Does not need to preserve any registers
sul_exit:
	MOV	v2, a1, LSL #2 @ expand pid to 32-bits
	MOV	v3, a2

	@ Disable any active VFP context
	MOV	a1, #0
	MOV	a2, #0
	SWI	XVFPSupport_ChangeContext

	LDR	a1, [v2, #PROC_STATUS]

	@ If it's not dynamic, then nothing to deregister
	TST	a1, #SULPROC_STATUS_FLAG_IS_DYNAMIC
	BEQ	0f

	@ There are 3 possibilities to watch out for here:
	@ a) The process is a child which resulted from a fork(), but
	@    not an exec(). The child is the same client as the parent
	@    and should not be deregistered.
	@ b) The process is a child which resulted from an exec()
	@    (regardless of fork()). The child is a new client which
	@    is distinct from its parent and should be deregistered.
	@ c) The process is not a child at all and should be deregistered.  */
	@
	@ This boils down to:
	@ if (sulproc->status.execed || !sulproc->status.forked)

	TST	a1, #SULPROC_STATUS_FLAG_IS_EXECED
	BNE	1f @ exec flag is set, child is new client - deregister
	TST	a1, #SULPROC_STATUS_FLAG_IS_FORKED
	BNE	0f @ fork flag is set, child is same client as parent, do not deregister
1:
	SWI	XSOM_DeregisterClient
0:
	TST	a1, #SULPROC_STATUS_FLAG_IS_ARMEABI
	BEQ	2f

	MOV	a1, #ARMEABISUPPORT_STACKOP_FREE
	LDR	a2, [v2, #PROC_STACK]
	SWI	XARMEABISupport_StackOp
2:

	@ Restore old exit and error handlers if necessary
	MOV	a1, #11
	LDR	a2, [v2, #PROC_OLDEXIT]
	LDR	a3, [v2, #PROC_OLDEXITR12]
	TEQ	a2, #0
	SWINE	XOS_ChangeEnvironment
	MOV	a1, #6
	LDR	a2, [v2, #PROC_OLDERROR]
	LDR	a3, [v2, #PROC_OLDERRORR12]
	LDR	a4, [v2, #PROC_OLDERRORBUF]
	TEQ	a2, #0
	SWINE	XOS_ChangeEnvironment

	@ Restore old wimpslot and copy parent proc down if necessary
	LDR	a1, [v2, #PROC_PARENTADDR]
	TEQ	a1, #0
	BLNE	copy_down_parent

	BL	restore_wimpslot

	@ Remove the process from our linked list
	LDR	ip, [v2,  #PROC_PRIVATEWORD]
	MOV	a3, v2
	BL	delink

	@ Free the CLI.
	MOV	a1, #7
	LDR	a3, [v2, #PROC_CLI]
	TEQ	a3, #0
	SWINE	XOS_Module

	@ If this process has no parent, then free the process structure.
	@ The parent will free it otherwise.
	LDR	a2, [v2, #PROC_PPID]
	CMP	a2, #1
	BNE	has_parent

	@ Only clean up when we know there is no parent
	LDR	a1, [v2, #PROC_STATUS]
	TST	a1, #SULPROC_STATUS_FLAG_IS_ARMEABI
	SWINE	XARMEABISupport_Cleanup

	@ Free any environ still left. The program should have
	@ already freed anything else
	MOV	a1, #7
	LDR	a3, [v2, #PROC_ENVIRON]
	TEQ	a3, #0
	SWINE	XOS_Module
	MOV	a3, v2
	SWI	XOS_Module

	ADR	a1, error_rclimit_exceeded
	LDR	a2, exit_word
	MOV	a3, v3
	SWI	OS_Exit		@ - never returns

has_parent:
	@ Update parent's __ul_global.sulproc pointer
	LDR	a2, [v2, #PROC_PARENTSULPROC]
	LDR	a3, [v2, #PROC_PARENTSULPROCADDR]
	STR	a2, [a3]

	@ This process is now a zombie child
	LDR	a3, [v2, #PROC_STATUS]
	ORR	a3, a3, #ZOMBIE_BIT
	STR	a3, [v2, #PROC_STATUS]

	@ Restore registers and return with a1 = child pid
	MOV	a1, v2, LSR #2 @ shrink pid to 30-bits
	ADD	a2, v2, #PROC_FORK_STORAGE
	LFM	f4, 4, [a2], #4*12
	LDMIA	a2, {v1-v6, sl, fp, sp, pc}

exit_word:
	.word	0x58454241

error_rclimit_exceeded:
	.word	0x1E2		@ This is an official RISC OS error number
	.asciz	"Return code limit exceeded"
	.align

error_no_mem:
	.asciz	"Not enough memory"
	.align

sysretcode:
	.asciz	"Sys$ReturnCode"
	.align

	.end
