@ hello_hdr.s - the module header and the veneers of HelloMod (hand written; CMunge is not used).
@ The header is the first thing in the image (section .text.header); the kernel finds everything through the offsets.  Position independent: only offsets.
	.syntax	unified
	.arm
	.section	".text.header","ax"
	.global	_start
_start:
	.word	0				@ start code offset (none: not runnable)
	.word	init - _start			@ initialisation
	.word	final - _start			@ finalisation
	.word	0				@ service call handler (none)
	.word	title - _start
	.word	help - _start
	.word	cmdtab - _start			@ help and command keyword table
	.word	0				@ SWI chunk base
	.word	0				@ SWI handler
	.word	0				@ SWI decoding table
	.word	0				@ SWI decoding code
	.word	0				@ messages file
	.word	flags - _start			@ module flags
title:	.asciz	"HelloMod"
help:	.asciz	"HelloMod\t0.01 (04 Oct 2026) GCC 16 EABI"
	.align	2
cmdtab:
	.asciz	"HelloMod_Say"
	.align	2
	.word	cmd_say - _start		@ code
	.word	0x00010000			@ min 0 parameters, max 1
	.word	say_syntax - _start
	.word	say_help - _start
	.asciz	"HelloMod_Info"
	.align	2
	.word	cmd_info - _start
	.word	0x00000000			@ no parameters
	.word	info_syntax - _start
	.word	info_help - _start
	.word	0				@ end of the table
info_syntax:
	.asciz	"Syntax: *HelloMod_Info"
info_help:
	.ascii	"HelloMod_Info shows where the module is, the SVC stack pointer and the processor mode.\r"
	.asciz	"Syntax: *HelloMod_Info"
	.align	2
say_syntax:
	.asciz	"Syntax: *HelloMod_Say [name]"
say_help:
	.ascii	"HelloMod_Say prints a greeting from a module built with GCC 16.\r"
	.asciz	"Syntax: *HelloMod_Say [name]"
	.align	2
flags:	.word	1				@ bit 0: 32-bit compatible
link_addr:
	.word	_start				@ the linked address of the image (0): relocated with everything else
reloc_info:
	.word	0				@ offset of the relocation table (filled in by tools/modreloc.py)
	.word	0				@ number of entries
	.ltorg

@ ---- the entry points: the kernel calls them in SVC mode.  Inputs per PRM 1-212 ff.  Result: r0 = 0 or an error block pointer; V set for an error.
@ Everything the kernel does not expect to change (r7-r11) is saved; sp is aligned to 8 for the C code and put back.
	.balign	4
init:						@ r10 = environment string, r11 = podule base / instantiation, r12 = private word
	stmfd	sp!, {r4-r11, lr}
	@ The image was linked at address 0 and is non-PIC: every word that holds an address is in the table that tools/modreloc.py appended; add the load address to each.
	@ link_addr is one of those words: after the first run it holds the real address, so a second run (RMReInit of the same image) finds delta = 0 and does nothing.
	adrl	r4, _start			@ where the image is now (PC relative)
	ldr	r5, link_addr			@ where the linker put it (0), or where it already is
	subs	r6, r4, r5			@ delta
	beq	relocated
	adrl	r7, reloc_info
	ldr	r8, [r7]			@ offset of the table
	ldr	r9, [r7, #4]			@ number of entries
	add	r8, r4, r8
rloop:	cmp	r9, #0
	beq	rdone
	ldr	r0, [r8], #4			@ offset of a word to patch
	ldr	r1, [r4, r0]
	add	r1, r1, r6
	str	r1, [r4, r0]
	sub	r9, r9, #1
	b	rloop
rdone:	mov	r0, #1				@ OS_SynchroniseCodeAreas: the literal pools of the code were written
	mov	r1, r4
	ldr	r2, =__image_end		@ absolute (it is in the table, and has been patched); the end of the range is inclusive
	sub	r2, r2, #1
	swi	0x2006E
relocated:
	mov	r0, r10
	mov	r1, r11
	mov	r2, r12
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello_init
	mov	sp, r4
	b	done

final:						@ r10 = fatality, r11 = instantiation, r12 = private word
	stmfd	sp!, {r4-r11, lr}
	mov	r0, r10
	mov	r1, r11
	mov	r2, r12
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello_final
	mov	sp, r4
	b	done

cmd_say:					@ r0 = argument string, r1 = number of parameters, r12 = private word
	stmfd	sp!, {r4-r11, lr}
	mov	r3, r12
	mov	r2, #0				@ command number
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello_cmd
	mov	sp, r4
	b	done

cmd_info:					@ no parameters; argc is used to pass the SVC stack pointer as the kernel gave it
	mov	r1, sp
	stmfd	sp!, {r4-r11, lr}
	mov	r3, r12
	mov	r2, #1				@ command number 1
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello_cmd
	mov	sp, r4
	b	done

done:						@ r0 = 0 (V clear) or an error pointer (V set)
	cmp	r0, #0				@ clears V
	ldmfdeq	sp!, {r4-r11, pc}
	mov	r1, #0
	cmp	r1, #0x80000000			@ 0 - 0x80000000 overflows: sets V
	ldmfd	sp!, {r4-r11, pc}
