#ifndef _EXECINFO_H
#define _EXECINFO_H
#ifdef __cplusplus
extern "C" {
#endif
/* The return addresses on the chain of frame records (compile with -fno-omit-frame-pointer); backtrace_symbols gives "[0x...]" (the addresses are in the program's image: tools/crashsym.py with the link map names them). */
int backtrace (void **, int);
char **backtrace_symbols (void *const *, int);
void backtrace_symbols_fd (void *const *, int, int);
#ifdef __cplusplus
}
#endif
#endif
