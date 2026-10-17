/* dlfcn.h - dynamic loading: there is none yet on RISC OS 64 (programs are statically linked); dlopen fails and dlerror says so.  The SDK of RISC OS 64. */
#ifndef _RISCOS64_DLFCN_H
#define _RISCOS64_DLFCN_H
#ifdef __cplusplus
extern "C" {
#endif
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
void *dlopen (const char *, int);
void *dlsym (void *, const char *);
int dlclose (void *);
char *dlerror (void);
#ifdef __cplusplus
}
#endif
#endif
