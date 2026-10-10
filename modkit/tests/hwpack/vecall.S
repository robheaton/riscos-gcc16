@ vecall.S - calls a vector veneer of the module (vector-handlers:) the way the kernel does: with the claim address stacked, and tells whether it came back by the claim address or through lr (tests of pack/module42)
@   unsigned ct_call_vector (const unsigned in[13], unsigned out[13], void (*veneer) (void), unsigned flags, unsigned *claimed)
@ in[0..12] go to r0 - r12, flags (bits 31 - 28) to N Z C V; after the call r0 - r12 are stored in out[0..12]; *claimed is 1 when the veneer returned to the claim address (the vector was claimed), 0 when it
@ returned through lr (passed on); the result is the flags of that moment (bits 31 - 28)
	.text
	.align	2
	.global	ct_call_vector
	.type	ct_call_vector, %function
ct_call_vector:
	push	{r4-r11, lr}
	sub	sp, sp, #16			@ [sp] = out, [sp, #4] = the veneer, [sp, #8] = claimed, [sp, #12] = spare
	str	r1, [sp]
	str	r2, [sp, #4]
	ldr	r4, [sp, #52]			@ the fifth argument is on the caller's stack
	str	r4, [sp, #8]
	adr	lr, ct_claim_ret
	str	lr, [sp, #-4]!			@ the address that a claim returns to (the kernel stacks it); [sp, #8] is the veneer now
	msr	cpsr_f, r3
	mov	lr, r0
	ldmia	lr, {r0-r12}
	adr	lr, ct_pass_ret
	ldr	pc, [sp, #8]
ct_pass_ret:
	add	sp, sp, #4			@ passed on: the claim address is still on the stack
	mov	lr, #0
	b	ct_tail
ct_claim_ret:
	mov	lr, #1				@ claimed: the veneer took the claim address off the stack
ct_tail:
	str	r12, [sp, #-4]!
	mrs	r12, cpsr
	str	r12, [sp, #-4]!			@ [sp] = the cpsr, [sp, #4] = r12, [sp, #8] = out, [sp, #12] = the veneer, [sp, #16] = claimed
	ldr	r12, [sp, #8]
	stmia	r12, {r0-r11}
	ldr	r0, [sp, #4]
	str	r0, [r12, #48]
	ldr	r1, [sp, #16]
	str	lr, [r1]
	ldr	r0, [sp]
	and	r0, r0, #0xF0000000
	add	sp, sp, #24
	pop	{r4-r11, pc}
	.size	ct_call_vector, . - ct_call_vector
