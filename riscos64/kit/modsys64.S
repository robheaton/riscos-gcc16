/* modsys64.S - oserr *a64_swi (unsigned swi, unsigned *r, unsigned *flags): executes a SWI for the library, which keeps its registers in a block of ten 32 bit words (also from module code, EL1: it keeps what it needs on the stack).
   x0 = the SWI number (with the X bit if the errors are to be returned), x1 -> r[0..9], x2 -> where the flags go (or 0).  r[i] goes to x_i (zero extended: the pointers of a program are below 4 GB), svc #0 with
   the number in x10, and x0-x9 come back (low 32 bits) to r[]; the flags N Z C V are stored in bits 31 to 28 of *flags, and when V is set the result is x0, the pointer to the error block, otherwise 0. */
	.text
	.global	a64_swi
	.hidden	a64_swi
	.type	a64_swi, %function
a64_swi:
	stp	x29, x30, [sp, #-32]!
	mov	x29, sp
	stp	x1, x2, [sp, #16]
	mov	x10, x0
	mov	x11, x1
	ldp	w0, w1, [x11]
	ldp	w2, w3, [x11, #8]
	ldp	w4, w5, [x11, #16]
	ldp	w6, w7, [x11, #24]
	ldp	w8, w9, [x11, #32]
	svc	#0
	mrs	x10, nzcv
	ldp	x11, x12, [sp, #16]
	stp	w0, w1, [x11]
	stp	w2, w3, [x11, #8]
	stp	w4, w5, [x11, #16]
	stp	w6, w7, [x11, #24]
	stp	w8, w9, [x11, #32]
	cbz	x12, 1f
	str	w10, [x12]
1:	tst	x10, #0x10000000
	csel	x0, x0, xzr, ne
	ldp	x29, x30, [sp], #32
	ret
	.size	a64_swi, .-a64_swi
