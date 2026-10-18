/* The routine that WrchV calls: it counts the call and passes it on.  The registers are as the caller of OS_WriteC had them; "ret" returns to the kernel's vector chain (pass on). */
	.text
	.p2align 2
	.global vec_veneer
vec_veneer:
	adrp	x16, vec_calls
	add	x16, x16, :lo12:vec_calls
	ldr	w17, [x16]
	add	w17, w17, #1
	str	w17, [x16]
	ret
	.global vec_veneer_addr
vec_veneer_addr:
	adr	x0, vec_veneer
	ret
	.bss
	.p2align 3
	.global vec_calls
vec_calls:
	.space 8
