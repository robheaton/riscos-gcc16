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
