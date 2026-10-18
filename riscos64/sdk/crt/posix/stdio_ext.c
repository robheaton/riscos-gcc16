/* stdio_ext.c - see include/stdio_ext.h.  The SDK of RISC OS 64. */
#include <stdio.h>
#include <stdio_ext.h>

size_t __fbufsize (FILE *f) { (void) f; return BUFSIZ; }
size_t __fpending (FILE *f) { (void) f; return 0; }
int __flbf (FILE *f) { (void) f; return 0; }
int __freadable (FILE *f) { return (f->flags & __SRD) != 0; }
int __fwritable (FILE *f) { return (f->flags & __SWR) != 0; }
int __freading (FILE *f) { return (f->flags & __SRD) != 0 && !(f->flags & __SWR); }
int __fwriting (FILE *f) { return (f->flags & __SWR) != 0 && !(f->flags & __SRD); }
int __fsetlocking (FILE *f, int type) { (void) f; (void) type; return FSETLOCKING_INTERNAL; }
void _flushlbf (void) { }
void __fpurge (FILE *f) { (void) f; }
