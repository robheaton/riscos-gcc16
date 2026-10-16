/* mod64f.S - Mod64F, the module of the floating point test (module test 0.4).  It has two SWIs that use the floating point / vector registers at EL1, the way any module compiled for AArch64 with the
   default options may do it without a word (GCC uses vector registers even to copy a structure): the question is what the kernel does with the caller's floating point state across a module's SWI.
     Mod64F_Probe    one floating point instruction (fmov d0,#1.0) and the FPCR in r0: if floating point traps at EL1 it shows first and costs the least;
     Mod64F_Clobber  writes the 24 vector registers that a C function may clobber (v0-v7 and v16-v31; v8-v15 are callee-saved in the AAPCS and the module saves and restores them) and sets the rounding
                     mode of the FPCR to round towards zero, and leaves that so: r0 = the FPCR before.
   Both return the FPCR in the low word of r0 only (the first slot: the same place in a register block of 32-bit and of 64-bit slots), and write no memory of the module (the stack aside).
   The test program (t09fp.c) compares the registers it had before the call with the ones it has after it.  No code of riscos64-clib is used. */
	.set	SWIBASE, 0x59A80
	.section .text, "ax"
	.global _mod_start
_mod_start:
	.word	0					/* 00 start entry: none */
	.word	(mod_init - _mod_start) | 0x40000000	/* 04 initialisation (bit 30: not ARM) */
	.word	mod_final - _mod_start			/* 08 finalisation */
	.word	0					/* 0C service call handler: none */
	.word	mod_title - _mod_start			/* 10 title string */
	.word	mod_help - _mod_start			/* 14 help string */
	.word	0					/* 18 help and command keyword table: none */
	.word	SWIBASE					/* 1C SWI chunk base */
	.word	mod_swi - _mod_start			/* 20 SWI handler */
	.word	mod_swinames - _mod_start		/* 24 SWI decoding table */
	.word	0					/* 28 SWI decoding code: none */
	.word	0					/* 2C message file: none */
	.word	mod_flags - _mod_start			/* 30 offset to the module flags */

mod_title:
	.asciz	"Mod64F"
mod_help:
	.asciz	"Mod64F\t\t0.01 (15 Oct 2026) floating point state test for AArch64"
	.balign	4
mod_swinames:
	.asciz	"Mod64F"
	.asciz	"Probe"
	.asciz	"Clobber"
	.byte	0
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear: not 32-bit compatible; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	0					/* zero-initialisation size */

mod_msg_init:
	.asciz	"Mod64F: initialised (the floating point state test)"
mod_msg_final:
	.asciz	"Mod64F: finalised"
	.balign	4

print_line:				/* OS_Write0 and OS_NewLine, X bit set */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x10, #0x2
	orr	x10, x10, #0x20000
	svc	#0
	mov	x10, #0x3
	orr	x10, x10, #0x20000
	svc	#0
	ldp	x29, x30, [sp], #16
	ret
mod_init:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	adr	x0, mod_msg_init
	bl	print_line
	mov	x0, #0
	ldp	x29, x30, [sp], #16
	ret
mod_final:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	adr	x0, mod_msg_final
	bl	print_line
	mov	x0, #0
	ldp	x29, x30, [sp], #16
	ret

mod_swi:				/* x0 = SWI offset, x1 -> the register block */
	cmp	x0, #1
	b.hi	9f
	cbnz	x0, 2f
	fmov	d0, #1.0			/* Mod64F_Probe */
	mrs	x2, fpcr
	str	w2, [x1]
	mov	x0, #0
	ret
2:	stp	q8, q9, [sp, #-128]!		/* Mod64F_Clobber: v8-v15 are callee-saved in the AAPCS (a kernel written in C may hold values there across the call to this module): the module saves them and puts them back */
	stp	q10, q11, [sp, #32]
	stp	q12, q13, [sp, #64]
	stp	q14, q15, [sp, #96]
	mrs	x2, fpcr
	str	w2, [x1]
	orr	x3, x2, #0xC00000		/* RMode = round towards zero */
	msr	fpcr, x3
	movi	v0.16b, #64
	movi	v1.16b, #65
	movi	v2.16b, #66
	movi	v3.16b, #67
	movi	v4.16b, #68
	movi	v5.16b, #69
	movi	v6.16b, #70
	movi	v7.16b, #71
	movi	v16.16b, #80
	movi	v17.16b, #81
	movi	v18.16b, #82
	movi	v19.16b, #83
	movi	v20.16b, #84
	movi	v21.16b, #85
	movi	v22.16b, #86
	movi	v23.16b, #87
	movi	v24.16b, #88
	movi	v25.16b, #89
	movi	v26.16b, #90
	movi	v27.16b, #91
	movi	v28.16b, #92
	movi	v29.16b, #93
	movi	v30.16b, #94
	movi	v31.16b, #95
	ldp	q14, q15, [sp, #96]
	ldp	q12, q13, [sp, #64]
	ldp	q10, q11, [sp, #32]
	ldp	q8, q9, [sp], #128
	mov	x0, #0
	ret
9:	mov	x0, #-1				/* not one of ours */
	ret
