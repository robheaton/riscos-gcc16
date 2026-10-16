/* start.S - start-up of the tests: zero the bss, set the stack pointer to the RAM limit that OS_GetEnv gives, call main, leave with OS_Exit.
   Written from the interface: SVC #0 with the SWI number in x10 (bit 17 = the X bit), arguments in x0-x9. */
	.text
	.global __zeroinit
__zeroinit:
	adrp	x1, __bss_start
	add	x1, x1, :lo12:__bss_start
	adrp	x2, __bss_end
	add	x2, x2, :lo12:__bss_end
1:	cmp	x1, x2
	b.hs	2f
	stp	xzr, xzr, [x1], #16
	b	1b
2:	ret

	.global _start64
_start64:
	mov	x10, #0x10		/* OS_GetEnv: x0 -> the command line, x1 = the RAM limit */
	orr	x10, x10, #0x20000
	svc	#0
	adrp	x3, t_cmdline
	add	x3, x3, :lo12:t_cmdline
	str	x0, [x3]
	mov	fp, #0
	and	x1, x1, #~15
	mov	sp, x1
	bl	main
	mov	x2, x0			/* the return code */
	adr	x0, exit_error
	movz	x1, #0x4241
	movk	x1, #0x5845, lsl #16	/* "ABEX" */
	mov	x10, #0x11		/* OS_Exit */
	svc	#0
1:	b	1b
	.balign	4
exit_error:
	.word	0
	.asciz	"Test finished"
	.balign	4

	.bss
	.global t_cmdline
	.balign	8
t_cmdline:
	.space	8
