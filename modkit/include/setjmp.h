/* setjmp.h - setjmp / longjmp for a module (soft float: r4 - r11, sp and lr are saved). */
#ifndef _SETJMP_H
#define _SETJMP_H
typedef int jmp_buf[10];
extern int setjmp (jmp_buf env) __attribute__ ((returns_twice));
extern void longjmp (jmp_buf env, int val) __attribute__ ((noreturn));
#endif
