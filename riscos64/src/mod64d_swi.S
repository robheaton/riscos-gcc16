/* mod64d_swi.S - swi_x for Mod64D: the SWI (X bit set) made from module code, that is from EL1 (module tests 0.1-0.3 made SWIs from module code only with the stack and x10 live: nothing above x10 was
   ever held across an svc at EL1).  Same interface as swi.S: u64 swi_x (u64 swi, u64 regs[10]) loads x0-x9 from regs[], returns 1 if V was set, stores x0-x9 back.  Two differences:
     - the pointer to regs[] is kept on the stack, not in a register that the kernel might not preserve;
     - x19-x28 (callee-saved, which the C code of the module holds values in across this call) are copied to the stack before the svc and compared with the registers after it: if the kernel changed any of
       them, cs_state becomes &BAD (it is &5A while every svc so far preserved them), and the copies are put back, so that the module's C code is safe either way.  Mod64D_Stack reports cs_state.
   Uses x10-x14 (caller-saved). */
	.text
	.global	swi_x
swi_x:
	stp	x29, x30, [sp, #-112]!
	mov	x29, sp
	stp	x19, x20, [sp, #16]
	stp	x21, x22, [sp, #32]
	stp	x23, x24, [sp, #48]
	stp	x25, x26, [sp, #64]
	stp	x27, x28, [sp, #80]
	str	x1, [sp, #96]
	mov	x11, x1
	orr	x10, x0, #0x20000
	ldp	x0, x1, [x11, #0]
	ldp	x2, x3, [x11, #16]
	ldp	x4, x5, [x11, #32]
	ldp	x6, x7, [x11, #48]
	ldp	x8, x9, [x11, #64]
	svc	#0
	cset	x11, vs				/* the V flag, before anything changes the flags */
	ldr	x10, [sp, #96]
	stp	x0, x1, [x10, #0]
	stp	x2, x3, [x10, #16]
	stp	x4, x5, [x10, #32]
	stp	x6, x7, [x10, #48]
	stp	x8, x9, [x10, #64]
	ldp	x12, x13, [sp, #16]
	eor	x12, x12, x19
	eor	x13, x13, x20
	orr	x14, x12, x13
	ldp	x12, x13, [sp, #32]
	eor	x12, x12, x21
	eor	x13, x13, x22
	orr	x14, x14, x12
	orr	x14, x14, x13
	ldp	x12, x13, [sp, #48]
	eor	x12, x12, x23
	eor	x13, x13, x24
	orr	x14, x14, x12
	orr	x14, x14, x13
	ldp	x12, x13, [sp, #64]
	eor	x12, x12, x25
	eor	x13, x13, x26
	orr	x14, x14, x12
	orr	x14, x14, x13
	ldp	x12, x13, [sp, #80]
	eor	x12, x12, x27
	eor	x13, x13, x28
	orr	x14, x14, x12
	orr	x14, x14, x13
	ldp	x19, x20, [sp, #16]		/* put the copies back whatever happened */
	ldp	x21, x22, [sp, #32]
	ldp	x23, x24, [sp, #48]
	ldp	x25, x26, [sp, #64]
	ldp	x27, x28, [sp, #80]
	cbz	x14, 1f
	adrp	x12, cs_state
	add	x12, x12, :lo12:cs_state
	mov	x13, #0xBAD
	str	x13, [x12]
1:	mov	x0, x11
	ldp	x29, x30, [sp], #112
	ret

	.section .data, "aw"
	.balign	8
	.global	cs_state
	.hidden	cs_state
cs_state:
	.quad	0x5A				/* &5A: x19-x28 were preserved by every svc so far; &BAD: one was not */
