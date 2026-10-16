/* mod64s.S - Mod64S, the small module of the plain service entry test (module test 0.4).  Its header has a service handler and NO table of service numbers and no RET marker: the kernel, if it
   calls it at all, calls it for every service, in whatever convention it has.  The entry counts every call, keeps the first eight values of x1, and for a call in which x1 (or x0) is &59AC1, the
   number that the test program sends with OS_ServiceCall, keeps x0-x5, x12 and x30.  The other module of the test, Mod64D, has the table in the form that CMunge writes (and the 64-bit modules of
   riscos64-clib have); between them one run says which of the two forms the kernel honours and what registers it gives.  Nothing is claimed: every register is as it was on return, except x16 and x17.
   Linked at 4 (mod64sf.ld) and loaded at page+4, so link and run time addresses agree in their low 12 bits; it reaches its state with ADR.  Mod64S_Setup (r0 = the slot size of the register block, 4 or 8), Mod64S_Svc (the counts and x0-x5, x12 of the match) and
   Mod64S_Ring (x30 and the first eight values of x1) answer through the register block in that layout. */
	.set	SWIBASE, 0x59AC0
	.section .text, "ax"
	.global _mod_start
_mod_start:
	.word	0					/* 00 start entry: none */
	.word	(mod_init - _mod_start) | 0x40000000	/* 04 initialisation (bit 30: not ARM) */
	.word	mod_final - _mod_start			/* 08 finalisation */
	.word	mod_service - _mod_start		/* 0C service call handler */
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
	.asciz	"Mod64S"
mod_help:
	.asciz	"Mod64S\t\t0.01 (15 Oct 2026) plain service entry test for AArch64"
	.balign	4
mod_swinames:
	.asciz	"Mod64S"
	.asciz	"Setup"
	.asciz	"Svc"
	.asciz	"Ring"
	.byte	0
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear: not 32-bit compatible; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	0					/* zero-initialisation size */

mod_msg_init:
	.asciz	"Mod64S: initialised (the plain service entry test)"
mod_msg_final:
	.asciz	"Mod64S: finalised"
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

/* the service entry: svs[0] = every call counted, svs[1] = which register held &59AC1 (1: x1, 2: x0) in the last matching call, svs[2] = matching calls, svs[3..8] = x0-x5 of it, svs[9] = x12, svs[10] = x30,
   svs[11..18] = the first eight values of x1, svs[19] = the slot size.  Uses x16 and x17 only, and changes no flag (no compare: eor and cbz, tbz). */
mod_service:
	adr	x16, svs
	ldr	x17, [x16]
	add	x17, x17, #1
	str	x17, [x16]			/* every call counted */
	sub	x17, x17, #9			/* negative while the count is 1..8 */
	tbz	x17, #63, 1f			/* more than eight calls: not kept in the ring */
	add	x16, x16, x17, lsl #3
	str	x1, [x16, #152]			/* svs + 8 * (count - 9) + 152 = svs + 80 + 8 * count */
	adr	x16, svs
1:	mov	x17, #0x9AC1
	movk	x17, #5, lsl #16		/* &59AC1 */
	eor	x17, x17, x1
	cbz	x17, 2f
	mov	x17, #0x9AC1
	movk	x17, #5, lsl #16
	eor	x17, x17, x0
	cbz	x17, 3f
	ret					/* not our number */
2:	mov	x17, #1
	b	4f
3:	mov	x17, #2
4:	str	x17, [x16, #8]
	ldr	x17, [x16, #16]
	add	x17, x17, #1
	str	x17, [x16, #16]
	stp	x0, x1, [x16, #24]
	stp	x2, x3, [x16, #40]
	stp	x4, x5, [x16, #56]
	str	x12, [x16, #72]
	str	x30, [x16, #80]
	ret

mod_swi:				/* x0 = SWI offset, x1 -> the register block */
	adr	x2, svs
	ldr	x3, [x2, #152]			/* the slot size */
	cmp	x0, #2
	b.hi	9f
	cbz	x0, 1f
	cmp	x0, #1
	b.eq	2f
	add	x2, x2, #80			/* Mod64S_Ring: svs[10..18]: x30 and the ring (nine values) */
	mov	x4, #9
	b	4f
1:	ldr	w3, [x1]			/* Mod64S_Setup: the low word of r0 is the slot size */
	str	x3, [x2, #152]
	mov	x0, #0
	ret
2:	mov	x4, #10				/* Mod64S_Svc: svs[0..9] */
4:	ldr	x5, [x2], #8			/* copy x4 values into the register block, in slots of the size in x3 */
	cmp	x3, #8
	b.ne	5f
	str	x5, [x1], #8
	b	6f
5:	str	w5, [x1], #4
6:	subs	x4, x4, #1
	b.ne	4b
	mov	x0, #0
	ret
9:	mov	x0, #-1				/* not one of ours */
	ret

	.section .data, "aw"
	.balign	8
svs:
	.space	8 * 19
	.quad	8				/* svs[19]: the slot size: 8 until Setup says otherwise */
