/* fpregs.S - helpers of t09fp (the floating point state test of module test 0.4).
   u64 fp_call_swi (u64 swi, u64 regs[10], const u64 pat[64], u64 out[64]): loads x0-x9 from regs[], loads the 32 vector registers (v0-v31, 128 bits each: pat[2n], pat[2n+1]) and makes the SWI (X bit set)
   with nothing between the load of the vector registers and the svc, and stores the 32 vector registers into out[] right after it (before any other code can touch them) and x0-x9 into regs[].
   Returns 1 if the V flag was set.  It leaves the vector registers changed (the caller must assume it, as for any call); d8-d15 are saved and restored as the AAPCS says. */
	.text
	.global	fp_call_swi
fp_call_swi:
	stp	x29, x30, [sp, #-112]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	d8, d9, [sp, #48]
	stp	d10, d11, [sp, #64]
	stp	d12, d13, [sp, #80]
	stp	d14, d15, [sp, #96]
	mov	x19, x1				/* regs */
	mov	x20, x3				/* out */
	mov	x22, x2				/* pat */
	orr	x21, x0, #0x20000		/* the SWI number, X bit set */
	ldp	x0, x1, [x19, #0]
	ldp	x2, x3, [x19, #16]
	ldp	x4, x5, [x19, #32]
	ldp	x6, x7, [x19, #48]
	ldp	x8, x9, [x19, #64]
	ldp	q0, q1, [x22, #0]
	ldp	q2, q3, [x22, #32]
	ldp	q4, q5, [x22, #64]
	ldp	q6, q7, [x22, #96]
	ldp	q8, q9, [x22, #128]
	ldp	q10, q11, [x22, #160]
	ldp	q12, q13, [x22, #192]
	ldp	q14, q15, [x22, #224]
	ldp	q16, q17, [x22, #256]
	ldp	q18, q19, [x22, #288]
	ldp	q20, q21, [x22, #320]
	ldp	q22, q23, [x22, #352]
	ldp	q24, q25, [x22, #384]
	ldp	q26, q27, [x22, #416]
	ldp	q28, q29, [x22, #448]
	ldp	q30, q31, [x22, #480]
	mov	x10, x21
	svc	#0
	stp	q0, q1, [x20, #0]
	stp	q2, q3, [x20, #32]
	stp	q4, q5, [x20, #64]
	stp	q6, q7, [x20, #96]
	stp	q8, q9, [x20, #128]
	stp	q10, q11, [x20, #160]
	stp	q12, q13, [x20, #192]
	stp	q14, q15, [x20, #224]
	stp	q16, q17, [x20, #256]
	stp	q18, q19, [x20, #288]
	stp	q20, q21, [x20, #320]
	stp	q22, q23, [x20, #352]
	stp	q24, q25, [x20, #384]
	stp	q26, q27, [x20, #416]
	stp	q28, q29, [x20, #448]
	stp	q30, q31, [x20, #480]
	stp	x0, x1, [x19, #0]
	stp	x2, x3, [x19, #16]
	stp	x4, x5, [x19, #32]
	stp	x6, x7, [x19, #48]
	stp	x8, x9, [x19, #64]
	cset	x0, vs
	ldp	d14, d15, [sp, #96]
	ldp	d12, d13, [sp, #80]
	ldp	d10, d11, [sp, #64]
	ldp	d8, d9, [sp, #48]
	ldp	x21, x22, [sp, #32]
	ldp	x19, x20, [sp, #16]
	ldp	x29, x30, [sp], #112
	ret

	.global	fp_get_fpcr
fp_get_fpcr:
	mrs	x0, fpcr
	ret
	.global	fp_set_fpcr
fp_set_fpcr:
	msr	fpcr, x0
	ret
	.global	fp_get_fpsr
fp_get_fpsr:
	mrs	x0, fpsr
	ret
