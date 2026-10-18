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
                  "size_t fwrite_unlocked(const void *, size_t, size_t, FILE *) __picolibc_export;\nint fflush_unlocked(FILE *) __picolibc_export;\nint fileno_unlocked(FILE *) __picolibc_export;\nchar *fgets_unlocked(char *, int, FILE *) __picolibc_export;", 1)
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

# O_BINARY: text and binary files are the same here; a non-zero value makes gnulib believe it is on Windows (_setmode)
fc = inc + "/sys/fcntl.h"
t = open(fc).read()
if "#define O_BINARY _FBINARY" in t:
    open(fc, "w").write(t.replace("#define O_BINARY _FBINARY", "#define O_BINARY 0\t/* (picolibc: _FBINARY) */"))

# program_invocation_name and program_invocation_short_name (glibc): set by the start-up code
er = inc + "/errno.h"
t2 = open(er).read()
if "program_invocation_name" not in t2:
    i = t2.rindex("#endif")
    open(er, "w").write(t2[:i] + "#ifdef __cplusplus\nextern \"C\" {\n#endif\nextern char *program_invocation_name, *program_invocation_short_name;\n#ifdef __cplusplus\n}\n#endif\n" + t2[i:])

# ioctl's third parameter is called "param" in picolibc's header; gawk (and others) have a macro of that name
io2 = inc + "/sys/ioctl.h"
t = open(io2).read()
if "void *param" in t:
    open(io2, "w").write(t.replace("void *param", "void *__argp"))

# picolibc's C23 const-preserving strchr, strpbrk and strrchr return "const void *" for a const argument; pointer arithmetic (strchr (s, c) - s) and assignments to const char * need "const char *"
st = inc + "/string.h"
t = open(st).read()
for fn, args in (("strchr", "(_s, _c)"), ("strpbrk", "(_s, _a)"), ("strrchr", "(_s, _c)")):
    old = "__preserve_const(%s, _s, %s)" % (fn, args)
    new = "_Generic(1 ? (_s) : (void *)1, const void *: (const char *)(%s)%s, default: (%s)%s)" % (fn, args, fn, args)
    t = t.replace(old, new)
open(st, "w").write(t)
