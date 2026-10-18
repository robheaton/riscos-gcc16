#!/usr/bin/env python3
"""fix-picolibc-headers.py INCLUDEDIR: repairs things in picolibc's headers that GCC's and binutils' configure scripts trip over (idempotent):
 * <sys/ioctl.h> defines TIOCGWINSZ but not struct winsize (it is in <sys/termios.h>): the include is added.
 * getchar_unlocked and putchar_unlocked are macros with the wrong number of arguments (getchar_unlocked (f), putchar_unlocked (c, f)); they take (void) and (c) in POSIX.
 * fputc_unlocked, fputs_unlocked, fread_unlocked, fwrite_unlocked, fflush_unlocked and fileno_unlocked have no prototype; the SDK (crt/posix/misc.c) defines them, and GCC's system.h calls them
   when configure finds them in the library."""
import sys
inc = sys.argv[1]
io = inc + "/sys/ioctl.h"
i = open(io).read()
if "sys/termios.h" not in i:
    i = i.replace("#define TIOCGWINSZ", "#include <sys/termios.h>\n#define TIOCGWINSZ", 1)
    open(io, "w").write(i)
p = inc + "/stdio.h"
t = open(p).read()
t = t.replace("#define getchar_unlocked(f)    getc(stdin)", "#define getchar_unlocked()     getc(stdin)")
t = t.replace("#define putchar_unlocked(c, f) putc(c, stdout)", "#define putchar_unlocked(c)    putc(c, stdout)")
if "fputc_unlocked" not in t:
    marker = "int putc_unlocked(int, FILE *) __nonnull((2)) __picolibc_export;"
    assert marker in t, "unexpected stdio.h"
    t = t.replace(marker, marker + "\nint fputc_unlocked(int, FILE *) __picolibc_export;\nint fputs_unlocked(const char *, FILE *) __picolibc_export;\nsize_t fread_unlocked(void *, size_t, size_t, FILE *) __picolibc_export;\n"
                  "size_t fwrite_unlocked(const void *, size_t, size_t, FILE *) __picolibc_export;\nint fflush_unlocked(FILE *) __picolibc_export;\nint fileno_unlocked(FILE *) __picolibc_export;", 1)
open(p, "w").write(t)

def add_before_last_endif(path, marker, text):
    """adds text before the last #endif of the header (once)"""
    h = open(inc + "/" + path).read()
    if marker in h:
        return
    i = h.rindex("#endif")
    open(inc + "/" + path, "w").write(h[:i] + text + "\n" + h[i:])

add_before_last_endif("unistd.h", "SDK: more POSIX", """/* SDK: more POSIX (crt/posix/compat.c, passwd.c) */
#ifdef __cplusplus
extern "C" {
#endif
int renameat (int, const char *, int, const char *);
int posix_fadvise (int, off_t, off_t, int);
int posix_fallocate (int, off_t, off_t);
int mlock (const void *, size_t);
int pipe2 (int[2], int);
int dup3 (int, int, int);
int initgroups (const char *, gid_t);
int getlogin_r (char *, size_t);
#ifdef __cplusplus
}
#endif""")
add_before_last_endif("poll.h", "ppoll", """#include <sys/_sigset.h>
struct timespec;
int ppoll (struct pollfd *, nfds_t, const struct timespec *, const __sigset_t *);""")

# descriptors of sockets are 300 and up (crt/posix/sockets.c): fd_set must reach them
sel = inc + "/sys/_select.h"
t = open(sel).read()
if "#define FD_SETSIZE 512" not in t:
    t = t.replace("#define FD_SETSIZE 64", "#define FD_SETSIZE 512", 1)
    open(sel, "w").write(t)

# FIONBIO and FIONREAD for sockets: the numbers of the BSD ioctl that the Internet module takes
ioc = inc + "/sys/ioctl.h"
t = open(ioc).read()
if "FIONBIO" not in t:
    i = t.rindex("#endif")
    open(ioc, "w").write(t[:i] + "#define FIONBIO 0x8004667e\n#define FIONREAD 0x4004667f\n" + t[i:])

# _setjmp and _longjmp (the versions that do not save the signal mask; Lua and others use them)
sj = inc + "/setjmp.h"
t = open(sj).read()
if "_longjmp" not in t:
    i = t.rindex("#endif")
    open(sj, "w").write(t[:i] + "#ifndef _setjmp\n#define _setjmp(env) setjmp (env)\n#define _longjmp(env, val) longjmp (env, val)\n#endif\n" + t[i:])

# utime has a struct but no prototype in picolibc's header (the SDK defines it in crt/posix/fileops.c)
ut = inc + "/sys/utime.h"
t = open(ut).read()
if "utime (const" not in t:
    t = t.replace("_END_STD_C\n", "int utime (const char *, const struct utimbuf *);\n\n_END_STD_C\n", 1)
    open(ut, "w").write(t)

# getrusage, setpriority and getpriority are BSD calls too (glibc has them under _DEFAULT_SOURCE)
rs = inc + "/sys/resource.h"
t = open(rs).read()
t2 = t.replace("#if __XSI_VISIBLE\nint getpriority", "#if __XSI_VISIBLE || __BSD_VISIBLE\nint getpriority").replace("#if __XSI_VISIBLE\nint getrusage", "#if __XSI_VISIBLE || __BSD_VISIBLE\nint getrusage")
if t2 != t:
    open(rs, "w").write(t2)
