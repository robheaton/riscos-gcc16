/* modcommon.h - what the three tools of modkit share (cmunge, modreloc, mkoslib): messages, memory, files, a growing string, and a reader for 32 bit little endian ELF files.
   Plain C99 and stdio only: the same sources are built for Linux (the tool chain's bin/) and, with the cross compiler, for RISC OS (the native package). */
#ifndef MODCOMMON_H
#define MODCOMMON_H
#include <stddef.h>
#include <stdio.h>

extern const char *progname;                  /* set by main () */

void die (const char *fmt, ...) __attribute__ ((noreturn, format (printf, 1, 2)));
/* A tool that wants to go on after one failure sets die_recover (a jmp_buf it has set with setjmp): die () then leaves the message in die_message (without the program name) and jumps there,
   instead of printing it and ending the program. */
#include <setjmp.h>
extern jmp_buf *die_recover;
extern char die_message[512];
void *xmalloc (size_t n);
void *xrealloc (void *p, size_t n);
char *xstrdup (const char *s);
char *xstrndup (const char *s, size_t n);

typedef struct { char *s; size_t len, cap; } Buf;
void buf_init (Buf *b);
void buf_addn (Buf *b, const char *s, size_t n);
void buf_adds (Buf *b, const char *s);
void buf_addc (Buf *b, char c);
void buf_printf (Buf *b, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

/* the whole file, with a NUL after it; dies when it cannot be read */
unsigned char *read_file (const char *path, size_t *len);
/* writes the file; dies when it cannot */
void write_file (const char *path, const void *data, size_t len);
/* the last part of a path (after the last / ) */
const char *base_name (const char *path);
/* Python's "%#x": always with the 0x, also for 0 (C's gives just 0).  The result is in one of eight buffers that are used in turn. */
const char *hx (unsigned v);
int has_suffix_nocase (const char *s, const char *suffix);

/* ---- ELF32 little endian (ARM) */
typedef struct {
  unsigned name, type, flags, addr, offset, size, link, info, entsize;
  const char *namestr;
} ElfSec;
typedef struct {
  unsigned char *data; size_t len;
  int nsec; ElfSec *sec;
} Elf;
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_NOBITS 8
#define SHT_REL 9
unsigned rd16 (const unsigned char *p);
unsigned rd32 (const unsigned char *p);
void wr32 (unsigned char *p, unsigned v);
void elf_load (Elf *e, const char *path);                /* dies if it is not an ELF32 little endian file */
void elf_load_mem (Elf *e, unsigned char *data, size_t len, const char *what);   /* the same for a file that is in memory already (a member of an archive): the Elf points into DATA, which stays with the caller */
int elf_find_section (const Elf *e, const char *name);   /* index, or -1 */
const char *elf_symname (const Elf *e, const ElfSec *symtab, unsigned strofs);

#endif
