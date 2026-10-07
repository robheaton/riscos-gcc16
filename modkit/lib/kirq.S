@ kirq.S - _kernel_irqs_on, _kernel_irqs_off, _kernel_irqs_disabled and _kernel_processor_mode of the SharedCLibrary's kernel interface.  ARMv6: CPSIE / CPSID I (in USR mode they do nothing,
@ as in the SharedCLibrary, which does the same with MSR); no register but r0 changes (the SharedCLibrary promises that of the first two: __counter relies on it).
	.syntax	unified
	.arm
	.text
	.global	_kernel_irqs_on
	.type	_kernel_irqs_on, %function
_kernel_irqs_on:
	cpsie	i
	bx	lr
	.size	_kernel_irqs_on, . - _kernel_irqs_on

	.global	_kernel_irqs_off
	.type	_kernel_irqs_off, %function
_kernel_irqs_off:
	cpsid	i
	bx	lr
	.size	_kernel_irqs_off, . - _kernel_irqs_off

	.global	_kernel_irqs_disabled
	.type	_kernel_irqs_disabled, %function
_kernel_irqs_disabled:
	mrs	r0, cpsr
	and	r0, r0, #0x80
	bx	lr
	.size	_kernel_irqs_disabled, . - _kernel_irqs_disabled

	.global	_kernel_processor_mode
	.type	_kernel_processor_mode, %function
_kernel_processor_mode:
	mrs	r0, cpsr
	and	r0, r0, #0x1f
	bx	lr
	.size	_kernel_processor_mode, . - _kernel_processor_mode
