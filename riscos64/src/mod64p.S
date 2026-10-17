/* mod64p.S - Mod64P, the service call probe (test pack 0.7).  A plain service entry (no table) can be called by a kernel with the service number in x1 (as Pyromaniac does) or in x0 with an address in x1; this
   module records what its entry is given and how a service is claimed.  It answers: what x1 points to (the first 16 words of it are kept), which way a service is claimed (four ways, one per service number),
   and what x2-x5 and x12 hold.  The service numbers &59B01-&59B08 are records 0-7 (the test uses 0-4).  The entry keeps, for each of them, a record of 32 words (256 bytes, from svp + 64 + 256 * n):
     0 which register held the number (2 = x0, 1 = x1)   1-6 x0-x5   7 x12   8 x30   9-24 the 16 words at x1 (only when the number was in x0 and x1 looks like an address)   26 the record number   27 1 if the words were read
   Claims: record 1 writes a 32-bit zero at [x1+4], record 2 a 64-bit zero at [x1+8], record 3 sets x1 to 0, record 4 sets x0 to 0 (the test says which of them OS_ServiceCall reports as claimed).
   The words at x1 are read only if the number was in x0 and x1 is an 8-byte aligned address between 16 MB and 1 TB: on Pyromaniac (x1 = the number) nothing is read.  No flag is changed.
   Linked at 4 (mod64sf.ld) and loaded at page+4; it reaches its state with ADR.  SWIs: Mod64P_Setup (r0 = the slot size of the register block, 4 or 8), Mod64P_Get (r0 = record, r1 = first word: eight words
   come back in r0-r7, in that slot size), Mod64P_Count (r0 = how many matching calls), Mod64P_Add and Mod64P_Add32 (r0 + r1 in slots of 64 or of 32 bits: the test finds the slot size with them). */
	.set	SWIBASE, 0x59B00
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
	.asciz	"Mod64P"
mod_help:
	.asciz	"Mod64P\t\t0.01 (17 Oct 2026) service call probe for AArch64"
	.balign	4
mod_swinames:
	.asciz	"Mod64P"
	.asciz	"Setup"
	.asciz	"Get"
	.asciz	"Count"
	.asciz	"Add"
	.asciz	"Add32"
	.byte	0
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear: not 32-bit compatible; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	0					/* zero-initialisation size */

mod_msg_init:
	.asciz	"Mod64P: initialised (the service call probe)"
mod_msg_final:
	.asciz	"Mod64P: finalised"
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

/* the service entry: uses x16, x17 and four more that it saves; changes no flag (eor, lsr, cbz, cbnz only) */
mod_service:
	stp	x10, x11, [sp, #-32]!
	stp	x14, x15, [sp, #16]
	adr	x16, svp
	mov	x15, #0x9B01
	movk	x15, #5, lsl #16		/* &59B01 */
	sub	x17, x0, x15
	lsr	x14, x17, #3
	cbz	x14, 1f				/* x0 is one of the eight numbers */
	sub	x17, x1, x15
	lsr	x14, x17, #3
	cbz	x14, 2f				/* x1 is */
	b	9f				/* not ours */
1:	mov	x14, #2
	b	3f
2:	mov	x14, #1
3:	ldr	x15, [x16]
	add	x15, x15, #1
	str	x15, [x16]			/* every matching call counted */
	add	x16, x16, #64
	add	x16, x16, x17, lsl #8		/* the record */
	str	x14, [x16]
	stp	x0, x1, [x16, #8]
	stp	x2, x3, [x16, #24]
	stp	x4, x5, [x16, #40]
	str	x12, [x16, #56]
	str	x30, [x16, #64]
	str	x17, [x16, #208]
	str	xzr, [x16, #216]
	sub	x15, x14, #2
	cbnz	x15, 5f				/* the number was in x1: no block to read */
	lsr	x15, x1, #24
	cbz	x15, 5f				/* below 16 MB */
	lsr	x15, x1, #40
	cbnz	x15, 5f				/* above 1 TB */
	and	x15, x1, #7
	cbnz	x15, 5f				/* not 8-byte aligned */
	mov	x17, x1
	add	x14, x16, #72
	mov	x15, #16
4:	ldr	x10, [x17], #8
	str	x10, [x14], #8
	sub	x15, x15, #1
	cbnz	x15, 4b
	mov	x15, #1
	str	x15, [x16, #216]
5:	ldr	x15, [x16, #208]
	sub	x14, x15, #1
	cbz	x14, 6f
	sub	x14, x15, #2
	cbz	x14, 7f
	sub	x14, x15, #3
	cbz	x14, 8f
	sub	x14, x15, #4
	cbnz	x14, 9f
	mov	x0, #0				/* claim 4: x0 = 0 */
	b	9f
6:	ldr	x14, [x16, #216]		/* claim 1: a 32-bit zero at [x1+4] (r1 of a block of 32-bit slots) */
	cbz	x14, 9f
	str	wzr, [x1, #4]
	b	9f
7:	ldr	x14, [x16, #216]		/* claim 2: a 64-bit zero at [x1+8] (r1 of a block of 64-bit slots) */
	cbz	x14, 9f
	str	xzr, [x1, #8]
	b	9f
8:	mov	x1, #0				/* claim 3: x1 = 0 (as in 32-bit RISC OS) */
9:	ldp	x14, x15, [sp, #16]
	ldp	x10, x11, [sp], #32
	ret

mod_swi:				/* x0 = SWI offset, x1 -> the register block */
	adr	x2, svp
	ldr	x3, [x2, #8]			/* the slot size */
	cmp	x0, #4
	b.hi	9f
	cmp	x0, #3
	b.eq	10f
	b.hi	11f
	cbz	x0, 1f
	cmp	x0, #1
	b.eq	2f
	ldr	x5, [x2]			/* Mod64P_Count */
	b	6f
10:	ldr	x2, [x1]			/* Mod64P_Add: r0 + r1 read as 64-bit slots */
	ldr	x3, [x1, #8]
	add	x2, x2, x3
	str	x2, [x1]
	mov	x0, #0
	ret
11:	ldr	w2, [x1]			/* Mod64P_Add32: r0 + r1 read as 32-bit slots */
	ldr	w3, [x1, #4]
	add	w2, w2, w3
	str	w2, [x1]
	mov	x0, #0
	ret
1:	ldr	w3, [x1]			/* Mod64P_Setup: the low word of r0 is the slot size */
	str	x3, [x2, #8]
	mov	x0, #0
	ret
2:	ldr	w5, [x1]			/* Mod64P_Get: r0 = the record, r1 = the first word (slots of the current size) */
	cmp	x3, #8
	b.ne	3f
	ldr	x6, [x1, #8]
	b	4f
3:	ldr	w6, [x1, #4]
4:	cmp	x5, #8
	b.hs	9f
	add	x2, x2, #64
	add	x2, x2, x5, lsl #8
	cmp	x6, #24
	b.hi	9f
	add	x2, x2, x6, lsl #3
	mov	x4, #8
5:	ldr	x5, [x2], #8
	cmp	x3, #8
	b.ne	7f
	str	x5, [x1], #8
	b	8f
7:	str	w5, [x1], #4
8:	subs	x4, x4, #1
	b.ne	5b
	mov	x0, #0
	ret
6:	cmp	x3, #8
	b.eq	61f
	str	w5, [x1]
	b	62f
61:	str	x5, [x1]
62:	mov	x0, #0
	ret
9:	mov	x0, #-1				/* not one of ours (or a record that does not exist) */
	ret

	.section .data, "aw"
	.balign	8
svp:
	.quad	0				/* svp[0]: the matching calls */
	.quad	8				/* svp[1]: the slot size: 8 until Setup says otherwise */
	.space	48
	.space	256 * 8				/* the eight records, from svp + 64 */
