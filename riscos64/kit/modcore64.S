/* modcore64.S - the generic part of the header of a 64-bit module made with the kit: the relocator, the entry veneers, the service call glue and the data they use.  A module's header file (what cmunge will
   generate; by hand for now: examples/hello64/hello64_hdr.S) has the header words and the tables, then  #include "modcore64.S"  to get this.  Everything here is assembled in the same unit, so the offsets
   in the header words are constants.  The C part is modrt64.c, which calls the module's own functions (modkit64.h).
   Written from the module format of the README of https://github.com/gerph/riscos64-clib and from what module tests 0.1 to 0.4 measured on Pyromaniac; no code of it is used.
   - the module is linked at address 4 and loaded at a page plus 4; code and data are reached PC-relative; mod_reloc fixes the 64-bit words that hold addresses (reloc_table, written into the image by
     tools/modreloc64.py) the first time the module runs, then the zero-initialised area is cleared (the kernel allocates it, but *RMReInit does not clear it again);
   - entry registers as CMHG: init x0 -> tail, x1 = instance, x12 = private word; final x0 = fatal, x1 = podule, x12; SWI x0 = offset, x1 -> the register block, x12; commands x0 -> arguments,
     x1 = count, x2 = command number (set by the stub in the command table), x12; service: the fast entry of the service table, x1 = the index of the number in the table. */
	.set	SWIBASE_UNUSED, 0
#ifndef RELOC_CAP
#define RELOC_CAP 1024				/* how many relocations the table has room for (4 bytes each) */
#endif

	.macro	MOD_SERVICE_TABLE nums:vararg		/* the service call table (flags 1, the fast entry, the numbers and a 0, the offset of the table), then the service entry itself: a RET, which is what marks a table */
	.balign	4
svc_table:
	.word	1
	.word	__mod_svc_fast - _mod_start
	.irp	n, \nums
	.word	\n
	.endr
	.word	0
	.word	svc_table - _mod_start
mod_service:
	ret
	.global	__mod_svc_numbers
	.hidden	__mod_svc_numbers
__mod_svc_numbers:				/* the same numbers, for the C part: index -> number */
	.irp	n, \nums
	.word	\n
	.endr
	.word	0
	.endm

	.section .text.header, "ax"
__mod_reloc:				/* adds the distance between where the image was linked (or last relocated to: link_addr is itself in the table) and where it is now to every 64-bit word that the table lists.  Uses x0-x5, x16, x17. */
	adr	x16, _mod_start
	adr	x0, link_addr
	ldr	x17, [x0]
	sub	x17, x16, x17
	cbz	x17, 9f
	adr	x0, reloc_count
	ldr	w1, [x0]
	adr	x2, reloc_table
1:	cbz	w1, 9f
	ldr	w3, [x2], #4
	sub	w1, w1, #1
	add	x4, x16, x3
	ldr	x5, [x4]
	add	x5, x5, x17
	str	x5, [x4]
	b	1b
9:	ret

	.global	__mod_init_v
__mod_init_v:				/* x0 -> the tail, x1 = instance, x12 = the private word */
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	adr	x16, _mod_start			/* linked at 4, code reaches its data with ADRP: it works only if the module is at a page plus 4 */
	and	x17, x16, #0xFFF
	cmp	x17, #4
	b.ne	1f
	stp	x0, x1, [sp, #16]
	bl	__mod_reloc
	adrp	x0, __ZIStart			/* clear the zero-initialised area (both are multiples of 16 apart) */
	add	x0, x0, :lo12:__ZIStart
	adrp	x1, __ZIEnd
	add	x1, x1, :lo12:__ZIEnd
2:	cmp	x0, x1
	b.hs	3f
	stp	xzr, xzr, [x0], #16
	b	2b
3:	ldp	x0, x1, [sp, #16]
	mov	x2, x12
	bl	__modrt_init
	ldp	x29, x30, [sp], #32
	ret
1:	adr	x0, __mod_err_base		/* not at page + 4: refuse to start, with an error block in the module's code (the kernel reads it) */
	ldp	x29, x30, [sp], #32
	ret
	.balign	4
__mod_err_base:
	.word	0x20A04
	.asciz	"Module not loaded at a page boundary plus 4"
	.balign	4
	.global	__mod_final_v
__mod_final_v:				/* x0 = fatal, x1 = podule, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x2, x12
	bl	__modrt_final
	ldp	x29, x30, [sp], #16
	ret
	.global	__mod_decode_v
__mod_decode_v:				/* the SWI decoding code: x0-x3 as the kernel gives them, x12 = the private word; they go back as the C part leaves them */
	stp	x29, x30, [sp, #-48]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x2, x3, [sp, #32]
	add	x0, sp, #16
	mov	x1, x12
	bl	__modrt_decode
	ldp	x0, x1, [sp, #16]
	ldp	x2, x3, [sp, #32]
	ldp	x29, x30, [sp], #48
	ret
	.global	__mod_swi_v
__mod_swi_v:				/* x0 = the SWI offset, x1 -> the register block, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x2, x12
	bl	__modrt_swi
	ldp	x29, x30, [sp], #16
	ret
	.global	__mod_cmd_v
__mod_cmd_v:				/* x0 -> the arguments, x1 = the count, x2 = the command number, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x3, x12
	bl	__modrt_command
	ldp	x29, x30, [sp], #16
	ret
__mod_svc_fast:				/* x1 = the index in the table; the other registers as the caller gave them; they are given back, as the C part left them */
	stp	x29, x30, [sp, #-112]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x2, x3, [sp, #32]
	stp	x4, x5, [sp, #48]
	stp	x6, x7, [sp, #64]
	stp	x8, x9, [sp, #80]
	str	x12, [sp, #96]
	mov	x0, x1
	add	x1, sp, #16
	mov	x2, x12
	bl	__modrt_service
	ldp	x0, x1, [sp, #16]
	ldp	x2, x3, [sp, #32]
	ldp	x4, x5, [sp, #48]
	ldp	x6, x7, [sp, #64]
	ldp	x8, x9, [sp, #80]
	ldr	x12, [sp, #96]
	ldp	x29, x30, [sp], #112
	ret

	.macro	MOD_SERVICE_PLAIN			/* a header with no service table: the service entry is called for every service, x1 = the number */
mod_service:
	stp	x29, x30, [sp, #-112]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x2, x3, [sp, #32]
	stp	x4, x5, [sp, #48]
	stp	x6, x7, [sp, #64]
	stp	x8, x9, [sp, #80]
	str	x12, [sp, #96]
	mov	x0, x1
	add	x1, sp, #16
	mov	x2, x12
	bl	__modrt_service_plain
	ldp	x0, x1, [sp, #16]
	ldp	x2, x3, [sp, #32]
	ldp	x4, x5, [sp, #48]
	ldp	x6, x7, [sp, #64]
	ldp	x8, x9, [sp, #80]
	ldr	x12, [sp, #96]
	ldp	x29, x30, [sp], #112
	ret
	.endm

	.section .data.rel, "aw"
	.balign	8
	.global	link_addr
	.hidden	link_addr
link_addr:
	.quad	_mod_start			/* the address the image was linked at; relocated like every other word, so that it says where the image is */
	.section .rodata.reloc, "a"
	.balign	8
	.global	reloc_count, reloc_cap, reloc_table
	.hidden	reloc_count, reloc_cap, reloc_table
reloc_count:
	.word	0				/* filled in by tools/modreloc64.py */
reloc_cap:
	.word	RELOC_CAP
reloc_table:
	.space	4 * RELOC_CAP			/* image offsets of the 64-bit words that hold addresses, filled in by tools/modreloc64.py */
