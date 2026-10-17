/* hello64_hdr.S - the header of Hello64: the header words and the tables.  This is what cmunge will generate from a CMHG file; for now it is written by hand.  Then the kit's generic part. */
#define SWIBASE 0x59A80
#define SVC_NUMBER 0x59A81
	.section .text.header, "ax"
	.global	_mod_start
	.hidden	_mod_start
_mod_start:
	.word	0					/* 00 start entry: none */
	.word	(__mod_init_v - _mod_start) | 0x40000000	/* 04 initialisation (bit 30: not ARM) */
	.word	__mod_final_v - _mod_start		/* 08 finalisation */
	.word	mod_service - _mod_start		/* 0C service call handler */
	.word	mod_title - _mod_start			/* 10 title string */
	.word	mod_help - _mod_start			/* 14 help string */
	.word	mod_cmds - _mod_start			/* 18 help and command keyword table */
	.word	SWIBASE					/* 1C SWI chunk base */
	.word	__mod_swi_v - _mod_start		/* 20 SWI handler */
	.word	mod_swinames - _mod_start		/* 24 SWI decoding table */
	.word	0					/* 28 SWI decoding code: none */
	.word	0					/* 2C message file: none */
	.word	mod_flags - _mod_start			/* 30 offset to the module flags */
mod_title:
	.asciz	"Hello64"
mod_help:
	.asciz	"Hello64\t\t0.01 (16 Oct 2026) a C module for RISC OS 64"
	.balign	4
mod_swinames:
	.asciz	"Hello64"
	.asciz	"Add"				/* 0 */
	.asciz	"Info"				/* 1 */
	.asciz	"Fail"				/* 2 */
	.byte	0
	.balign	4
mod_cmds:
	.asciz	"Hello64_Say"
	.balign	4
	.word	cmd0 - _mod_start
	.word	0x00010000				/* information word: min | GSTrans<<8 | max<<16 | flags<<24: 0 to 1 parameters */
	.word	syn0 - _mod_start
	.word	hlp0 - _mod_start
	.asciz	"Hello64_Error"
	.balign	4
	.word	cmd1 - _mod_start
	.word	0
	.word	syn1 - _mod_start
	.word	hlp1 - _mod_start
	.word	0					/* end of the table */
hlp0:	.asciz	"Hello64_Say greets.\r\nSyntax: *Hello64_Say [<name>]"
syn0:	.asciz	"Syntax: *Hello64_Say [<name>]"
hlp1:	.asciz	"Hello64_Error gives an error made with mk_error.\r\nSyntax: *Hello64_Error"
syn1:	.asciz	"Syntax: *Hello64_Error"
	.balign	4
mod_flags:
	.word	0x14					/* bit 0 clear; bit 2: a zero-init size follows; bits 4-7 = 1: AArch64 */
	.word	__ZISize
cmd0:	mov	x2, #0
	b	__mod_cmd_v
cmd1:	mov	x2, #1
	b	__mod_cmd_v
#include "modcore64.S"
	.section .text.header, "ax"
	MOD_SERVICE_PLAIN SVC_NUMBER
