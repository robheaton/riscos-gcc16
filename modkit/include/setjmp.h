/* setjmp.h - setjmp / longjmp for a module (soft float: r4 - r11, sp and lr are saved). */
#ifndef _SETJMP_H
#define _SETJMP_H
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __aarch64__
typedef long jmp_buf[22];			/* x19-x30, sp, and d8-d15 */
#else
typedef int jmp_buf[10];
#endif
extern int setjmp (jmp_buf env) __attribute__ ((returns_twice));
extern void longjmp (jmp_buf env, int val) __attribute__ ((noreturn));
#ifdef __cplusplus
}
#endif
#endif
