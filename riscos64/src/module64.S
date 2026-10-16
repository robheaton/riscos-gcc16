/* module64.S - a minimal 64-bit RISC OS module, Mod64: initialisation, finalisation, a help string, one command (Mod64_Hello) and two SWIs (Mod64_Add: x0 + x1 with the register block read as 64-bit slots, Mod64_Add32: the same read as 32-bit slots: which one is right is what the test finds out).
   Written from the module format of the README of https://github.com/gerph/riscos64-clib: the header words of an ordinary module, bit 30 of the initialisation offset
   set (the module is not ARM), the module flags word with the architecture in bits 4-7 (1 = AArch64) and bit 2 (a zero-initialisation size follows the flags),
   entry points that follow the CMHG pattern (x12 = the private word, errors returned as a pointer in x0, or 0).  The code is position independent (adr only):
   the module is loaded on a page boundary plus 4.  The SWI chunk is &59A00; change it if it is in use. */
	.set	SWIBASE, 0x59A00
	.section .text, "ax"
	.global _mod_start
_mod_start:
	.word	0					/* 00 start entry: none */
	.word	(mod_init - _mod_start) | 0x40000000	/* 04 initialisation (bit 30: not ARM) */
	.word	mod_final - _mod_start			/* 08 finalisation */
	.word	0					/* 0C service call handler: none */
	.word	mod_title - _mod_start			/* 10 title string */
	.word	mod_help - _mod_start			/* 14 help string */
	.word	mod_cmds - _mod_start			/* 18 help and command keyword table */
	.word	SWIBASE					/* 1C SWI chunk base */
	.word	mod_swi - _mod_start			/* 20 SWI handler */
	.word	mod_swinames - _mod_start		/* 24 SWI decoding table */
	.word	0					/* 28 SWI decoding code: none */
	.word	0					/* 2C message file: none */
	.word	mod_flags - _mod_start			/* 30 offset to the module flags */

mod_title:
	.asciz	"Mod64"
mod_help:
	.asciz	"Mod64\t\t0.02 (15 Oct 2026) test module for AArch64"
	.balign	4
mod_swinames:
	.asciz	"Mod64"
	.asciz	"Add"				/* Mod64_Add: x0 + x1, the register block read as 64-bit slots */
	.asciz	"Add32"				/* Mod64_Add32: r0 + r1, the register block read as 32-bit slots */
	.asciz	"Fail"				/* Mod64_Fail: returns an error block in x0 */
	.asciz	"Carry"				/* Mod64_Carry: sets the C flag in the flags word of the register block if r0 is not 0, clears it if it is (64-bit slots only) */
	.asciz	"FailRMA"			/* Mod64_FailRMA: returns an error block that it built in a block claimed from the RMA (OS_Module 6) */
	.byte	0
	.balign	4
mod_cmds:
	.asciz	"Mod64_Hello"
	.balign	4
	.word	mod_cmd_hello - _mod_start		/* code */
	.word	0x00000000				/* information: min 0, max 0 parameters, no flags */
	.word	mod_hello_syntax - _mod_start		/* syntax */
	.word	mod_hello_help - _mod_start		/* help */
	.word	0					/* end of the table */
mod_hello_help:
	.asciz	"Mod64_Hello prints a line from the 64-bit test module.\r\nSyntax: *Mod64_Hello"
mod_hello_syntax:
	.asciz	"Syntax: *Mod64_Hello"
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear: not 32-bit compatible; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	0					/* zero-initialisation size */

mod_msg_init:
	.asciz	"Mod64: initialised (a 64-bit module)"
mod_msg_final:
	.asciz	"Mod64: finalised"
mod_msg_hello:
	.asciz	"Mod64_Hello: hello from a 64-bit module"
	.balign	4

/* print the string at x0 and a newline: OS_Write0, OS_NewLine (X bit set) */
print_line:
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

mod_init:					/* x0 -> the start string, x1 = instance (0), x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	adr	x0, mod_msg_init
	bl	print_line
	mov	x0, #0
	ldp	x29, x30, [sp], #16
	ret
mod_final:				/* x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	adr	x0, mod_msg_final
	bl	print_line
	mov	x0, #0
	ldp	x29, x30, [sp], #16
	ret
mod_cmd_hello:				/* x0 -> arguments, x1 = count, x2 = command number, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	adr	x0, mod_msg_hello
	bl	print_line
	mov	x0, #0
	ldp	x29, x30, [sp], #16
	ret
mod_swi:				/* x0 = SWI offset, x1 -> the register block, x12 = the private word */
	cmp	x0, #4
	b.hi	1f
	cmp	x0, #1
	b.hi	3f
	cbnz	x0, 2f
	ldp	x2, x3, [x1]			/* Mod64_Add: slots of 64 bits */
	add	x2, x2, x3
	str	x2, [x1]
	mov	x0, #0
	ret
2:	ldp	w2, w3, [x1]			/* Mod64_Add32: slots of 32 bits */
	add	w2, w2, w3
	str	w2, [x1]
	mov	x0, #0
	ret
3:	cmp	x0, #2
	b.ne	4f
	adr	x0, mod_err			/* Mod64_Fail: an error block in the module's own code: the kernel sets V */
	ret
4:	cmp	x0, #3
	b.ne	5f
	ldr	w2, [x1]			/* Mod64_Carry: r0 (the low word of the slot) */
	ldr	w3, [x1, #80]			/* the flags word: the eleventh slot, 64-bit slots: bits 31-28 are N Z C V */
	bic	w3, w3, #0x20000000
	mov	w4, #0x20000000
	cmp	w2, #0
	csel	w4, w4, wzr, ne
	orr	w3, w3, w4
	str	w3, [x1, #80]
	mov	x0, #0
	ret
5:	stp	x29, x30, [sp, #-16]!		/* Mod64_FailRMA: claim 64 bytes of RMA (OS_Module 6, X bit), copy the error block there (one is claimed on every call and not freed: a test) */
	mov	x29, sp
	mov	x0, #6
	mov	x3, #64
	mov	x10, #0x1E
	orr	x10, x10, #0x20000
	svc	#0
	b.vs	6f				/* the claim failed: return the kernel's error block */
	adr	x1, mod_err
	ldp	x3, x4, [x1]
	stp	x3, x4, [x2]
	ldp	x3, x4, [x1, #16]
	stp	x3, x4, [x2, #16]
	ldp	x3, x4, [x1, #32]
	stp	x3, x4, [x2, #32]
	mov	x0, x2
6:	ldp	x29, x30, [sp], #16
	ret
1:	mov	x0, #-1				/* not one of ours: -1, the kernel raises the error */
	ret
	.balign	16
mod_err:
	.word	0x20A01
	.asciz	"Mod64: this error is on purpose"
	.balign	16
	.space	16
