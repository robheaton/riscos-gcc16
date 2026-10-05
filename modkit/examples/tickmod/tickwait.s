@ tickwait.s - TickWait <centiseconds>: wait in USER mode (an RISC OS absolute program, loaded and run at &8000), for the hardware runs of TickMod.
@ OS_GetEnv gives the command line (the program name, a space, the number) and the top of the application memory; the wait polls OS_ReadMonotonicTime, a SWI that returns to user mode every time,
@ which is where the kernel runs the transient callbacks that the ticker has asked for.  Ends with OS_Exit, return code 0.
	.syntax	unified
	.arm
	.text
	.global	_start
_start:
	swi	0x10			@ OS_GetEnv: r0 = command line, r1 = application memory limit
	mov	sp, r1
skipname:
	ldrb	r2, [r0], #1
	cmp	r2, #32
	bhi	skipname		@ up to the first space (or the end of the line)
	mov	r4, #0
digits:
	ldrb	r2, [r0], #1
	sub	r3, r2, #'0'
	cmp	r3, #9
	bhi	go			@ not a digit (unsigned: below '0' wraps round)
	add	r4, r4, r4, lsl #2
	add	r4, r3, r4, lsl #1	@ r4 = r4 * 10 + digit
	b	digits
go:
	swi	0x42			@ OS_ReadMonotonicTime
	mov	r5, r0
wait:
	swi	0x42
	sub	r1, r0, r5
	cmp	r1, r4
	blo	wait
	mov	r0, #0
	ldr	r1, =0x58454241		@ "ABEX"
	mov	r2, #0
	swi	0x11			@ OS_Exit
	.ltorg
