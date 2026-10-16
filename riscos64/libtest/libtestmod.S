/* libtestmod.S - long a64_call_on_stack (long (*fn) (void), void *top): calls FN with the stack pointer at TOP (a multiple of 16). */
	.text
	.global	a64_call_on_stack
	.type	a64_call_on_stack, %function
a64_call_on_stack:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	and	x2, x1, #-16
	mov	sp, x2
	blr	x0
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	ret
	.size	a64_call_on_stack, . - a64_call_on_stack
