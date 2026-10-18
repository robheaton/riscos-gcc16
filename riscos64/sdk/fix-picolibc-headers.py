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
