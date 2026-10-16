/* mod64d_hdr.S - the header and the entry veneers of Mod64D, the 64-bit module of the data test (module test 0.4): writable data, a zero-initialised area, pointers that the module relocates itself,
   a service call entry that only records what the kernel gives it, and error blocks.  The C part is mod64d.c.
   Written from the module format of the README of https://github.com/gerph/riscos64-clib and from what module tests 0.1 to 0.3 measured; no code of it is used.
   - the header words are those of an ordinary module; bit 30 of the initialisation offset; the flags word is &14 (architecture 1 in bits 4-7, bit 2: a zero-initialisation size follows, bit 0 clear);
   - the module is linked at address 4 and loaded at a page plus 4; all its own code reaches code and data PC-relative, and mod_reloc fixes the 64-bit words that hold addresses (table: reloc_table,
     written into the image by tools/modreloc64.py) the first time the module runs;
   - entry registers as CMHG: init x0 -> tail, x1 = instance, x12 = private word; SWI x0 = offset, x1 -> the register block (slots of 32 or 64 bits: the C part finds out which), x12; commands
     x0 -> arguments, x1 = count, x2 = command number (each entry of the command table sets it), x12.  An error is returned as a pointer in x0, 0 for none, -1 for a SWI that is not the module's. */
	.set	SWIBASE, 0x59A40
	.set	SVCNUM, 0x59A41			/* the number of the service call that the test sends itself with OS_ServiceCall */
	.set	RELOC_CAP, 128			/* how many relocations the table has room for */

	.section .text.header, "ax"
	.global	_mod_start
	.hidden	_mod_start
_mod_start:
	.word	0					/* 00 start entry: none */
	.word	(mod_init_v - _mod_start) | 0x40000000	/* 04 initialisation (bit 30: not ARM) */
	.word	mod_final_v - _mod_start		/* 08 finalisation */
	.word	mod_service - _mod_start		/* 0C service call handler */
	.word	mod_title - _mod_start			/* 10 title string */
	.word	mod_help - _mod_start			/* 14 help string */
	.word	mod_cmds - _mod_start			/* 18 help and command keyword table */
	.word	SWIBASE					/* 1C SWI chunk base */
	.word	mod_swi_v - _mod_start			/* 20 SWI handler */
	.word	mod_swinames - _mod_start		/* 24 SWI decoding table */
	.word	0					/* 28 SWI decoding code: none */
	.word	0					/* 2C message file: none */
	.word	mod_flags - _mod_start			/* 30 offset to the module flags */

mod_title:
	.asciz	"Mod64D"
mod_help:
	.asciz	"Mod64D\t\t0.04 (15 Oct 2026) data and relocation test for AArch64"
	.balign	4
mod_swinames:
	.asciz	"Mod64D"
	.asciz	"Add"				/*  0 x0 + x1, the register block read as 64-bit slots */
	.asciz	"Add32"				/*  1 the same read as 32-bit slots */
	.asciz	"Setup"				/*  2 tells the module the slot size (4 or 8) */
	.asciz	"Info"				/*  3 where the module is, where its zero-initialised area is, what it saw at initialisation */
	.asciz	"Reloc"				/*  4 checks the pointers that were relocated */
	.asciz	"Count"				/*  5 a counter in .data */
	.asciz	"Zero"				/*  6 the zero-initialised area: still zero? writable? */
	.asciz	"Stack"				/*  7 the stack pointer and x12 in a SWI */
	.asciz	"FailCopy"			/*  8 returns a static error block, copied into the RMA by the veneer */
	.asciz	"FailRMA"			/*  9 returns an error block built in the RMA */
	.asciz	"Svc"				/* 10 what the service call entry recorded */
	.asciz	"Args"				/* 11 what the last Mod64D_Args command was given */
	.asciz	"Heap"				/* 12 RMA blocks: the address, its alignment, the words before it; OS_Module 13 (extend) */
	.asciz	"Dirty"				/* 13 puts a marker into the zero-initialised area (for the *RMReInit test) */
	.byte	0
	.balign	4
mod_cmds:
	.asciz	"Mod64D_FailCode"		/* returns a static error block from the module's own code, as it is */
	.balign	4
	.word	cmd0 - _mod_start
	.word	0x00000000				/* information: min 0, max 0 parameters, no flags */
	.word	mod_syn0 - _mod_start
	.word	mod_hlp0 - _mod_start
	.asciz	"Mod64D_FailCopy"		/* the same block, copied into the RMA by the veneer */
	.balign	4
	.word	cmd1 - _mod_start
	.word	0x00000000
	.word	mod_syn1 - _mod_start
	.word	mod_hlp1 - _mod_start
	.asciz	"Mod64D_FailRMA"		/* an error block built in the RMA */
	.balign	4
	.word	cmd2 - _mod_start
	.word	0x00000000
	.word	mod_syn2 - _mod_start
	.word	mod_hlp2 - _mod_start
	.asciz	"Mod64D_Args"			/* records the argument string and the count that the kernel passes: 0 to 3 parameters */
	.balign	4
	.word	cmd3 - _mod_start
	.word	0x00030000				/* information: bits 0-7 the minimum number of parameters (0), bits 8-15 the GSTrans map (0), bits 16-23 the maximum (3), bits 24-31 flags (none) */
	.word	mod_syn3 - _mod_start
	.word	mod_hlp3 - _mod_start
	.word	0					/* end of the table */
mod_hlp0:
	.asciz	"Mod64D_FailCode returns an error block that is in the module's code.\r\nSyntax: *Mod64D_FailCode"
mod_syn0:
	.asciz	"Syntax: *Mod64D_FailCode"
mod_hlp1:
	.asciz	"Mod64D_FailCopy returns that error block copied into the RMA.\r\nSyntax: *Mod64D_FailCopy"
mod_syn1:
	.asciz	"Syntax: *Mod64D_FailCopy"
mod_hlp2:
	.asciz	"Mod64D_FailRMA returns an error block built in the RMA.\r\nSyntax: *Mod64D_FailRMA"
mod_syn2:
	.asciz	"Syntax: *Mod64D_FailRMA"
mod_hlp3:
	.asciz	"Mod64D_Args records its arguments.\r\nSyntax: *Mod64D_Args [<a> [<b> [<c>]]]"
mod_syn3:
	.asciz	"Syntax: *Mod64D_Args [<a> [<b> [<c>]]]"
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	__ZISize				/* the size of the zero-initialised area (an absolute value) */

/* the service call table, in the form that CMunge writes it for AArch64 and that the 64-bit modules of riscos64-clib have (checked in four of them): the table (flags, the offset of the fast entry, the
   service numbers in ascending order and a 0, the offset of the table), then the service entry itself, which is a RET ("never used", as CMunge says: the RET is what marks a table, as the NOP of 32-bit
   modules does), and the fast, indexed entry.  The fast entry only records what it is given (the registers, how often) and returns with every register as it was: it claims nothing.  The plain,
   non-indexed form of an entry is tested by the other module of the test, Mod64S. */
	.balign	4
svc_table:
	.word	1					/* flags: bit 0 (CMunge: 64-bit RISC OS always uses the fast entry) */
	.word	svc_fast - _mod_start
	.word	SVCNUM
	.word	0
	.word	svc_table - _mod_start
mod_service:					/* the service call entry that is not indexed: a RET */
	ret
svc_fast:				/* the fast entry: x1 is the index in the list of numbers (in CMunge's convention) */
	mov	x17, #1
svc_record:				/* x17 = which entry; uses x16 and x17 only; svc_state: [0] count, [1] entry, [2..11] x0..x9, [12] x12, [13] x30 */
	adrp	x16, svc_state
	add	x16, x16, :lo12:svc_state
	stp	x0, x1, [x16, #16]
	stp	x2, x3, [x16, #32]
	stp	x4, x5, [x16, #48]
	stp	x6, x7, [x16, #64]
	stp	x8, x9, [x16, #80]
	str	x12, [x16, #96]
	str	x30, [x16, #104]
	str	x17, [x16, #8]
	ldr	x17, [x16]
	add	x17, x17, #1
	str	x17, [x16]
	ret

/* the relocator: adds the distance between where the image was linked (or last relocated to: link_addr is itself in the table) and where it is now to every 64-bit word that the table lists.  It needs no
   stack, no data pointer and no C: it is called first, from the initialisation entry.  Uses x0-x5, x16, x17. */
	.section .text.header, "ax"			/* the same section as the header: its offsets are then constants for the assembler */
mod_reloc:
	adr	x16, _mod_start			/* where the image is now */
	adr	x0, link_addr
	ldr	x17, [x0]			/* where it was when its words were last made right */
	sub	x17, x16, x17			/* the difference */
	cbz	x17, 9f				/* nothing to do (the same place: a second initialisation) */
	adr	x0, reloc_count
	ldr	w1, [x0]
	adr	x2, reloc_table
1:	cbz	w1, 9f
	ldr	w3, [x2], #4			/* the image offset of a word that holds an address */
	sub	w1, w1, #1
	add	x4, x16, x3			/* where that word is now */
	ldr	x5, [x4]
	add	x5, x5, x17
	str	x5, [x4]
	b	1b
9:	ret

/* the entry veneers: thin, as CMunge's are; the work is in C (mod64d.c) */
mod_init_v:				/* x0 -> the tail, x1 = instance, x12 = the private word */
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	adr	x16, _mod_start			/* the module was linked at 4 and the code reaches its data with ADRP: it works only if it is loaded at a page boundary plus 4 */
	and	x17, x16, #0xFFF
	cmp	x17, #4
	b.ne	bad_base
	stp	x0, x1, [sp, #16]
	bl	mod_reloc
	ldp	x0, x1, [sp, #16]
	mov	x2, x12
	bl	c_mod_init
	ldp	x29, x30, [sp], #32
	ret
bad_base:				/* not loaded at page+4: refuse to start, with an error block that the kernel reads (it is in the module's code) */
	adr	x0, err_base
	ldp	x29, x30, [sp], #32
	ret
	.balign	4
err_base:
	.word	0x20A04
	.asciz	"Mod64D: not loaded at a page boundary plus 4"
	.balign	4
mod_final_v:				/* x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x0, x12
	bl	c_mod_final
	ldp	x29, x30, [sp], #16
	ret
mod_swi_v:				/* x0 = the SWI offset, x1 -> the register block, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x2, x12
	bl	c_mod_swi			/* returns 0, -1, or a pointer to an error block (copied into the RMA by the C part) */
	ldp	x29, x30, [sp], #16
	ret
cmd0:	mov	x2, #0
	b	cmd_common
cmd1:	mov	x2, #1
	b	cmd_common
cmd2:	mov	x2, #2
	b	cmd_common
cmd3:	mov	x2, #3
cmd_common:				/* x0 -> the arguments, x1 = the count, x2 = the command number, x12 = the private word */
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	mov	x3, x12
	bl	c_mod_command
	ldp	x29, x30, [sp], #16
	ret

/* the data that the assembler part owns */
	.section .data.rel, "aw"
	.balign	8
	.global	link_addr
	.hidden	link_addr
link_addr:
	.quad	_mod_start			/* the address the image was linked at; relocated like every other word, so that it says where the image is */
	.section .rodata.reloc, "a"
	.balign	8
	.global	reloc_count
	.hidden	reloc_count
	.global	reloc_table
	.hidden	reloc_table
	.global	reloc_cap
	.hidden	reloc_cap
reloc_count:
	.word	0				/* filled in by tools/modreloc64.py */
reloc_cap:
	.word	RELOC_CAP
reloc_table:
	.space	4 * RELOC_CAP			/* image offsets of the 64-bit words that hold addresses, filled in by tools/modreloc64.py */
	.section .data, "aw"
	.balign	16
	.global	svc_state
	.hidden	svc_state
svc_state:
	.space	8 * 14
