@ setjmp.S - setjmp / longjmp for a module (ARM state, soft float): r4 - r11, sp and lr are saved in a jmp_buf of ten words.
	.syntax	unified
	.arm
	.text
	.global	setjmp
	.type	setjmp, %function
setjmp:
	mov	ip, sp
	stmia	r0, {r4-r11, ip, lr}
	mov	r0, #0
	bx	lr
	.size	setjmp, . - setjmp
	.global	longjmp
	.type	longjmp, %function
longjmp:
	ldmia	r0, {r4-r11, ip, lr}
	mov	sp, ip
	movs	r0, r1				@ longjmp (env, 0) makes setjmp return 1
	moveq	r0, #1
	bx	lr
	.size	longjmp, . - longjmp
