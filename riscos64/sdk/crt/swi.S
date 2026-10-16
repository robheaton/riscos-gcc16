/* swi.S - unsigned long swi_x (unsigned long swi, unsigned long regs[10]): run the SWI "swi" with the X bit set, x0-x9 loaded from regs[] and stored back to regs[];
   returns 1 if the V flag was set by the SWI (an error: regs[0] is the error block) and 0 if not. */
	.text
	.global swi_x
swi_x:
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	str	x19, [sp, #16]
	mov	x19, x1
	orr	x10, x0, #0x20000
	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]
	ldp	x8, x9, [x19, #64]
	svc	#0
	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]
	stp	x4, x5, [x19, #32]
	stp	x6, x7, [x19, #48]
	stp	x8, x9, [x19, #64]
	cset	x0, vs
	ldr	x19, [sp, #16]
	ldp	x29, x30, [sp], #32
	ret
/* unsigned long swi_f (unsigned long swi, unsigned long regs[10]): as swi_x, but returns the condition flags that the SWI left, as N=8 Z=4 C=2 V=1. */
	.global swi_f
swi_f:
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	str	x19, [sp, #16]
	mov	x19, x1
	orr	x10, x0, #0x20000
	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]
	ldp	x8, x9, [x19, #64]
	svc	#0
	mrs	x11, nzcv
	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]
	stp	x4, x5, [x19, #32]
	stp	x6, x7, [x19, #48]
	stp	x8, x9, [x19, #64]
	lsr	x0, x11, #28
	and	x0, x0, #15
	ldr	x19, [sp, #16]
	ldp	x29, x30, [sp], #32
	ret
