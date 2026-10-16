/* crt0.S - start-up of a hosted program: the AIF header (aifhdr.S) calls __zeroinit and then _start64.  Zero the bss, ask OS_GetEnv for the command line and the RAM limit, put the stack at the RAM limit and
   call __riscos_start (riscos_start.c) with them.  SVC #0 with the SWI number in x10 (bit 17 = the X bit). */
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
	mov	fp, #0
	and	x1, x1, #~15
	mov	sp, x1
	bl	__riscos_start		/* x0 = the command line, x1 = the RAM limit: does not return */
1:	b	1b
