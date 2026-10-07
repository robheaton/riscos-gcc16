@ veneercall.S - calls a generic veneer of the module with the registers and the flags of the caller's choice (tests of pack/module32, in SVC and in USR mode)
@   unsigned mk_call_veneer (const unsigned in[13], unsigned out[13], void (*veneer) (void), unsigned flags)
@ in[0..11] go to r0 - r11, in[12] to r12, flags (bits 31 - 28) to N Z C V; after the call r0 - r12 are stored in out[0..12]; the result is the flags of that moment (bits 31 - 28)
	.text
	.align	2
	.global	mk_call_veneer
	.type	mk_call_veneer, %function
mk_call_veneer:
	push	{r4-r11, lr}
	push	{r1, r2}			@ [sp] = out, [sp, #4] = the veneer
	msr	cpsr_f, r3
	mov	lr, r0
	ldmia	lr, {r0-r12}
	ldr	lr, [sp, #4]
	blx	lr
	str	r12, [sp, #-4]!			@ [sp] = r12 now, [sp, #4] = out
	mrs	r12, cpsr
	str	r12, [sp, #-4]!			@ [sp] = the cpsr, [sp, #4] = r12, [sp, #8] = out
	ldr	r12, [sp, #8]
	stmia	r12, {r0-r11}
	ldr	r0, [sp, #4]
	str	r0, [r12, #48]
	ldr	r0, [sp]
	and	r0, r0, #0xF0000000
	add	sp, sp, #16			@ the cpsr, r12, out and the veneer
	pop	{r4-r11, pc}
	.size	mk_call_veneer, . - mk_call_veneer
