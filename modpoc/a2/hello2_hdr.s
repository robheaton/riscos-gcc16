@ hello2_hdr.s - the module header and the veneers of HelloMod2 (hand written; CMunge is not used).
@ The header is the first thing in the image (section .text.header); the kernel finds everything through the offsets.  Position independent: only offsets.
	.syntax	unified
	.equ	Service_UKCommand, 0x04		@ NOT &43 (that is Service_International)
	.equ	XOS_SynchroniseCodeAreas, 0x2006E
	.arm
	.section	".text.header","ax"
	.global	_start
_start:
	.word	0				@ start code offset (none: not runnable)
	.word	init - _start			@ initialisation
	.word	final - _start			@ finalisation
	.word	service - _start		@ service call handler
	.word	title - _start
	.word	help - _start
	.word	cmdtab - _start			@ help and command keyword table
	.word	0x5FC40				@ SWI chunk base (a multiple of 64)
	.word	swi_entry - _start		@ SWI handler
	.word	title - _start			@ SWI decoding table: the prefix, then the names; it shares the title string
	.word	0				@ SWI decoding code
	.word	0				@ messages file
	.word	flags - _start			@ module flags
title:	.asciz	"HelloMod2"			@ ... the SWI names follow, so "HelloMod2" is the title AND the SWI prefix
	.asciz	"Add"
	.asciz	"Op"
	.asciz	"Count"
	.byte	0				@ end of the SWI table
help:	.asciz	"HelloMod2\t0.01 (04 Oct 2026) GCC 16 EABI stage 2"
	.align	2
cmdtab:
	.asciz	"HelloMod2_Test"
	.align	2
	.word	cmd_test - _start		@ code
	.word	0x00000000			@ no parameters
	.word	test_syntax - _start
	.word	test_help - _start
	.word	0				@ end of the table
test_syntax:
	.asciz	"Syntax: *HelloMod2_Test"
test_help:
	.ascii	"HelloMod2_Test runs the checks of the module: data, bss, pointer tables, a switch, libc, its SWIs and a service call.\r"
	.asciz	"Syntax: *HelloMod2_Test"
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
	swi	XOS_SynchroniseCodeAreas
relocated:
	mov	r0, r10
	mov	r1, r11
	mov	r2, r12
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello2_init
	mov	sp, r4
	b	done

final:						@ r10 = fatality, r11 = instantiation, r12 = private word
	stmfd	sp!, {r4-r11, lr}
	mov	r0, r10
	mov	r1, r11
	mov	r2, r12
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello2_final
	mov	sp, r4
	b	done

cmd_test:					@ r0 = argument string, r1 = number of parameters, r12 = private word
	stmfd	sp!, {r4-r11, lr}
	mov	r3, r12
	mov	r2, #0				@ command number
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello2_cmd
	mov	sp, r4
	b	done

@ Service call handler: r1 = service number, r0, r2 - r8 as the service says, r12 = private word.  Pass a call on by returning with the registers unchanged; claim it by returning r1 = 0.
@ Service_UKCommand (&04): r0 = the command line; claimed = r1 = 0 and r0 = 0 (no error).
service:
	teq	r1, #Service_UKCommand
	movne	pc, lr
	stmfd	sp!, {r0-r10, lr}		@ the C code is free to use r0 - r3: every register the service call may not change is saved
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello2_ukcommand		@ r0 = the command line (unchanged); returns 1 if claimed
	mov	sp, r4
	cmp	r0, #0
	beq	svc_pass
	ldmfd	sp!, {r0-r10, lr}
	mov	r0, #0
	mov	r1, #0
	mov	pc, lr
svc_pass:
	ldmfd	sp!, {r0-r10, pc}		@ r1 is still the service number

@ SWI handler: r11 = SWI number - chunk base, r0 - r9 = the SWI's registers, r12 = private word.  The registers go to the C function as a block; whatever it leaves in the block comes back.
@ An error: r0 = the error block pointer, V set (the registers r1 - r9 are the caller's).
swi_entry:
	stmfd	sp!, {r0-r9, lr}
	mov	r0, r11
	mov	r1, sp
	mov	r2, r12
	mov	r4, sp
	bic	sp, sp, #7
	bl	hello2_swi
	mov	sp, r4
	cmp	r0, #0				@ clears V
	bne	swi_err
	ldmfd	sp!, {r0-r9, pc}
swi_err:
	add	sp, sp, #4			@ the saved r0 is not restored: r0 = the error pointer
	ldmfd	sp!, {r1-r9, lr}
	msr	cpsr_f, #0x10000000		@ V set
	mov	pc, lr

done:						@ r0 = 0 (V clear) or an error pointer (V set)
	cmp	r0, #0				@ clears V
	ldmfdeq	sp!, {r4-r11, pc}
	mov	r1, #0
	cmp	r1, #0x80000000			@ 0 - 0x80000000 overflows: sets V
	ldmfd	sp!, {r4-r11, pc}
