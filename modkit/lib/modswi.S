@ modswi.S - the generic SWI call of modkit: __modlib_xswi (swi_number_with_X_bit, unsigned regs[10]).  r0 - r9 are loaded from REGS, the SWI is called through OS_CallASWI (the number in r10),
@ r0 - r9 go back into REGS; the result is the error block (V set) or 0.  Used by _kernel_swi and by the OSLib veneers that bin/mkoslib.py generates.
	.syntax	unified
	.arm
	.equ	XOS_CallASWI, 0x2006F
	.text
	.global	__modlib_xswi
	.type	__modlib_xswi, %function
__modlib_xswi:
	stmfd	sp!, {r1, r4-r11, lr}		@ r1 = REGS is kept on the stack
	mov	r10, r0
	ldmia	r1, {r0-r9}
	swi	XOS_CallASWI
	ldr	r11, [sp]
	stmia	r11, {r0-r9}
	movvc	r0, #0				@ no error: 0; V set: r0 is the error block already
	ldmfd	sp!, {r1, r4-r11, pc}
	.size	__modlib_xswi, . - __modlib_xswi

@ __modlib_xswif (swi_number_with_X_bit, unsigned regs[10], unsigned *flags): as __modlib_xswi, and the flags of the SWI's exit (N Z C V: the top four bits of the PSR) are stored in *FLAGS when that is not 0.
	.global	__modlib_xswif
	.type	__modlib_xswif, %function
__modlib_xswif:
	stmfd	sp!, {r1, r2, r4-r11, lr}	@ r1 = REGS and r2 = FLAGS are kept on the stack
	mov	r10, r0
	ldmia	r1, {r0-r9}
	swi	XOS_CallASWI
	mrs	r11, cpsr			@ the flags of the SWI (reading the PSR changes none)
	ldr	r12, [sp]
	stmia	r12, {r0-r9}
	ldr	r12, [sp, #4]
	cmp	r12, #0
	andne	r10, r11, #0xF0000000
	strne	r10, [r12]
	tst	r11, #0x10000000		@ V: an error, r0 is the error block already
	moveq	r0, #0				@ no error: 0
	ldmfd	sp!, {r1, r2, r4-r11, pc}
	.size	__modlib_xswif, . - __modlib_xswif
