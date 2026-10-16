/* aifhdr.S - the header of an AIF64 absolute (RISC OS 64), written from the layout given in the README of https://github.com/gerph/riscos64-clib ("AIF Format"):
   an ARM32 AIF header whose first words are ARM32 code that reports "AArch64 binaries cannot be run on 32bit RISC OS", the address mode word is &40,
   and the AArch64 code starts at offset &100: NOP, BL zeroinit, BL entry.  The sizes come from the linker script (aif64.ld). */
	.section .aifhdr, "ax"
	.global _aif_header
_aif_header:
	.word	0xe1a00000		/* 000 NOP         ARM32 decompression */
	.word	0xe1a00000		/* 004 NOP         ARM32 reloc */
	.word	0xe1a00000		/* 008 NOP         ARM32 zero init */
	.word	0xeb00000b		/* 00C BL &40      ARM32 image entry */
	.word	0xef000011		/* 010 SWI OS_Exit */
	.word	__aif_ro_size		/* 014 read only size (code and static data, with the header) */
	.word	__aif_rw_size		/* 018 read write size */
	.word	0			/* 01C debug size */
	.word	__aif_zi_size		/* 020 zero init size */
	.word	0			/* 024 debug type */
	.word	0x8000			/* 028 linked base address */
	.word	0			/* 02C workspace size (obsolete) */
	.word	0x40			/* 030 address mode: 64 */
	.word	0			/* 034 data base address */
	.word	0			/* 038 reserved */
	.word	0			/* 03C reserved */
	.word	0xe28f0000		/* 040 ADR r0, error_block */
	.word	0xef00002b		/* 044 SWI OS_GenerateError */
	.word	0			/* 048 error number */
	.asciz	"AArch64 binaries cannot be run on 32bit RISC OS"	/* 04C error message */
	.org	_aif_header + 0x100
	nop				/* 100 AArch64 decompression */
	bl	__zeroinit		/* 104 zero init */
	bl	_start64		/* 108 entry */
	mov	x10, #0x11		/* 10C OS_Exit, if the entry returns */
	svc	#0
