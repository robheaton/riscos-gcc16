/* setjmp64.S - setjmp / longjmp for AArch64: x19 - x30 (x29 and x30 included), sp and d8 - d15 are saved in a jmp_buf of 22 longs. */
	.text
	.global	setjmp
	.type	setjmp, %function
setjmp:
	stp	x19, x20, [x0, #0]
	stp	x21, x22, [x0, #16]
	stp	x23, x24, [x0, #32]
	stp	x25, x26, [x0, #48]
	stp	x27, x28, [x0, #64]
	stp	x29, x30, [x0, #80]
	mov	x1, sp
	str	x1, [x0, #96]
#ifndef __ARM_FP_NONE
	stp	d8, d9, [x0, #104]
	stp	d10, d11, [x0, #120]
	stp	d12, d13, [x0, #136]
	stp	d14, d15, [x0, #152]
#endif
	mov	w0, #0
	ret
	.size	setjmp, . - setjmp
	.global	longjmp
	.type	longjmp, %function
longjmp:
	ldp	x19, x20, [x0, #0]
	ldp	x21, x22, [x0, #16]
	ldp	x23, x24, [x0, #32]
	ldp	x25, x26, [x0, #48]
	ldp	x27, x28, [x0, #64]
	ldp	x29, x30, [x0, #80]
	ldr	x2, [x0, #96]
	mov	sp, x2
#ifndef __ARM_FP_NONE
	ldp	d8, d9, [x0, #104]
	ldp	d10, d11, [x0, #120]
	ldp	d12, d13, [x0, #136]
	ldp	d14, d15, [x0, #152]
#endif
	cmp	w1, #0
	csinc	w0, w1, wzr, ne			/* longjmp (env, 0) makes setjmp return 1 */
	ret
	.size	longjmp, . - longjmp
