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

# <sys/wait.h> declares wait3 but not wait4 (crt/posix/bsdcompat.c has both): libiberty's pex-unix.c calls wait4 when configure finds it in the library
ww = inc + "/sys/wait.h"
try:
    t = open(ww).read()
    if "wait4" not in t:
        old = "pid_t wait3(int *, int, struct rusage *);"
        assert old in t, "unexpected sys/wait.h"
        open(ww, "w").write(t.replace(old, old + "\npid_t wait4(pid_t, int *, int, struct rusage *);", 1))
except IOError:
    pass

def add_before_last_endif(path, marker, text):
    """adds text before the last #endif of the header (once)"""
    try:
        h = open(inc + "/" + path).read()
    except IOError:
        return					# no such header in this installation
    if marker in h:
        return
    i = h.rfind("#endif")
    if i < 0:
        open(inc + "/" + path, "w").write(h + "\n" + text + "\n")
        return
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
    open(fc, "w").write(t.replace("#define O_BINARY _FBINARY", "#define O_BINARY 0\t/* (picolibc: _FBINARY) */\n#define O_TEXT 0"))

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

# posix_fadvise and posix_fallocate belong in <fcntl.h> (crt/posix/compat.c has them)
fc2 = inc + "/sys/fcntl.h"
t = open(fc2).read()
if "POSIX_FADV_NORMAL" not in t:
    i = t.rindex("#endif")
    open(fc2, "w").write(t[:i] + """#define POSIX_FADV_NORMAL 0
#define POSIX_FADV_RANDOM 1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED 3
#define POSIX_FADV_DONTNEED 4
#define POSIX_FADV_NOREUSE 5
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
int posix_fadvise (int, off_t, off_t, int);
int posix_fallocate (int, off_t, off_t);
#ifdef __cplusplus
}
#endif
""" + t[i:])

# the *at calls (crt/posix/compat.c)
def add_protos(path, marker, text):
    h = open(inc + "/" + path).read()
    if marker in h:
        return
    i = h.rindex("#endif")
    open(inc + "/" + path, "w").write(h[:i] + "#ifdef __cplusplus\nextern \"C\" {\n#endif\n" + text + "#ifdef __cplusplus\n}\n#endif\n" + h[i:])
add_protos("sys/fcntl.h", "openat", "int openat (int, const char *, int, ...);\n")
add_protos("stdio.h", "renameat", "int renameat (int, const char *, int, const char *);\n")
add_protos("unistd.h", "faccessat", "int faccessat (int, const char *, int, int);\nint unlinkat (int, const char *, int);\nint fchownat (int, const char *, uid_t, gid_t, int);\nint linkat (int, const char *, int, const char *, int);\nint symlinkat (const char *, int, const char *);\nssize_t readlinkat (int, const char *, char *, size_t);\n")
add_protos("sys/stat.h", "fstatat", "int fstatat (int, const char *, struct stat *, int);\nint mkdirat (int, const char *, mode_t);\nint fchmodat (int, const char *, mode_t, int);\nint utimensat (int, const char *, const struct timespec[2], int);\nint futimens (int, const struct timespec[2]);\nint mkfifoat (int, const char *, mode_t);\nint mknodat (int, const char *, mode_t, dev_t);\n")

# setgroups takes a size_t (glibc, coreutils); pw_passwd and pw_gecos at the end of struct passwd (crt/posix/passwd.c fills them)
su = inc + "/sys/unistd.h"
h = open(su).read()
h2 = h.replace("int setgroups(int ngroups, const gid_t *grouplist)", "int setgroups(size_t ngroups, const gid_t *grouplist)")
if h2 != h:
    open(su, "w").write(h2)
pw = inc + "/pwd.h"
h = open(pw).read()
if "pw_gecos" not in h:
    h = h.replace("    char *pw_shell; /* default shell */\n};", "    char *pw_shell; /* default shell */\n    char *pw_passwd; /* (the end of the structure: added by the SDK) */\n    char *pw_gecos;\n};")
    open(pw, "w").write(h)

# the conversion state uses wint_t, which only <stddef.h> defines, and not when some other header has already defined _WINT_T (gnulib's wrappers do): use the compiler's type
ty = inc + "/sys/_types.h"
t = open(ty).read()
if "        wint_t        __wch;" in t:
    open(ty, "w").write(t.replace("        wint_t        __wch;", "        __WINT_TYPE__ __wch;"))

# wint_t again: the prototypes of <wchar.h> and <wctype.h> need it, and a header that defined _WINT_T hides the typedef of <stddef.h>; the same typedef twice is allowed
for hn in ("wchar.h", "wctype.h"):
    hp = inc + "/" + hn
    h = open(hp).read()
    if "SDK: wint_t" not in h:
        j = h.index("#define", h.index("#ifndef")) if "#ifndef" in h else 0
        j = h.index("\n", j) + 1
        open(hp, "w").write(h[:j] + "/* SDK: wint_t */\ntypedef __WINT_TYPE__ wint_t;\n" + h[j:])

# CLOCK_MONOTONIC is wanted by plain C programs (glibc has it without _GNU_SOURCE); picolibc hides it unless _POSIX_MONOTONIC_CLOCK or _GNU_SOURCE
tm = inc + "/time.h"
t = open(tm).read()
if "#if defined(_POSIX_MONOTONIC_CLOCK) || __GNU_VISIBLE\n\n/*  The identifier for the system-wide monotonic" in t:
    open(tm, "w").write(t.replace("#if defined(_POSIX_MONOTONIC_CLOCK) || __GNU_VISIBLE\n\n/*  The identifier for the system-wide monotonic", "#if 1\t/* SDK: always */\n\n/*  The identifier for the system-wide monotonic", 1))

# SDK: environ under _GNU_SOURCE, the long double constants of <math.h>
add_before_last_endif("unistd.h", "SDK: environ", """/* SDK: environ */
#if defined _GNU_SOURCE || defined _DEFAULT_SOURCE
extern char **environ;
#endif
""")
add_before_last_endif("math.h", "SDK: M_xxxl", """/* SDK: M_xxxl: the long double constants of glibc's <math.h> */
#if defined _GNU_SOURCE || defined _DEFAULT_SOURCE || defined _XOPEN_SOURCE
#ifndef M_El
#define M_El        2.718281828459045235360287471352662498L
#endif
#ifndef M_LOG2El
#define M_LOG2El    1.442695040888963407359924681001892137L
#endif
#ifndef M_LOG10El
#define M_LOG10El   0.434294481903251827651128918916605082L
#endif
#ifndef M_LN2l
#define M_LN2l      0.693147180559945309417232121458176568L
#endif
#ifndef M_LN10l
#define M_LN10l     2.302585092994045684017991454684364208L
#endif
#ifndef M_PI_2l
#define M_PI_2l     1.570796326794896619231321691639751442L
#endif
#ifndef M_PI_4l
#define M_PI_4l     0.785398163397448309615660845819875721L
#endif
#ifndef M_1_PIl
#define M_1_PIl     0.318309886183790671537767526745028724L
#endif
#ifndef M_2_PIl
#define M_2_PIl     0.636619772367581343075535053490057448L
#endif
#ifndef M_2_SQRTPIl
#define M_2_SQRTPIl 1.128379167095512573896158903121545172L
#endif
#ifndef M_SQRT2l
#define M_SQRT2l    1.414213562373095048801688724209698079L
#endif
#ifndef M_SQRT1_2l
#define M_SQRT1_2l  0.707106781186547524400844362104849039L
#endif
#endif
""")

add_before_last_endif("string.h", "SDK: UnixLib string", """/* SDK: UnixLib string extras (crt/posix/bsdcompat.c) */
#ifdef __cplusplus
extern "C" {
#endif
int stricmp (const char *, const char *);
int strnicmp (const char *, const char *, size_t);
char *strichr (const char *, int);
char *strrichr (const char *, int);
#ifdef __cplusplus
}
#endif
""")

# SDK: the large file names, and the prototypes of the small functions of crt/posix/bsdcompat.c
for h in ("stdio.h", "fcntl.h", "unistd.h", "sys/stat.h", "dirent.h", "sys/mman.h"):
    add_before_last_endif(h, "SDK: lfs64", "/* SDK: lfs64 */\n#include <sys/lfs64.h>\n")
add_before_last_endif("stdio.h", "SDK: ctermid", """/* SDK: ctermid */
#ifdef __cplusplus
extern "C" {
#endif
char *ctermid (char *);
char *cuserid (char *);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("unistd.h", "SDK: setpgrp", """/* SDK: setpgrp */
#ifdef __cplusplus
extern "C" {
#endif
int setpgrp (void);
useconds_t ualarm (useconds_t, useconds_t);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("sys/time.h", "SDK: settimeofday", """/* SDK: settimeofday */
#ifdef __cplusplus
extern "C" {
#endif
int settimeofday (const struct timeval *, const struct timezone *);
int adjtime (const struct timeval *, struct timeval *);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("sys/stat.h", "SDK: mknod", """/* SDK: mknod */
#ifdef __cplusplus
extern "C" {
#endif
int mknod (const char *, mode_t, dev_t);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("sys/mman.h", "SDK: shm_open", """/* SDK: shm_open */
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
int shm_open (const char *, int, mode_t);
int shm_unlink (const char *);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("malloc.h", "SDK: malloc_trim", """/* SDK: malloc_trim */
#ifdef __cplusplus
extern "C" {
#endif
int malloc_trim (size_t);
#ifdef __cplusplus
}
#endif
""")

add_before_last_endif("unistd.h", "SDK: dysize", """/* SDK: dysize */
#ifdef __cplusplus
extern "C" {
#endif
int getdomainname (char *, size_t);
int setdomainname (const char *, size_t);
int brk (void *);
#ifdef __cplusplus
}
#endif
""")
add_before_last_endif("time.h", "SDK: dysize", """/* SDK: dysize */
#ifdef __cplusplus
extern "C" {
#endif
int dysize (int);
#ifdef __cplusplus
}
#endif
""")

add_before_last_endif("stdlib.h", "SDK: b64_ntop", """/* SDK: b64_ntop (crt/posix/bsdcompat.c) */
#ifdef __cplusplus
extern "C" {
#endif
int b64_ntop (const unsigned char *, size_t, char *, size_t);
int b64_pton (const char *, unsigned char *, size_t);
#ifdef __cplusplus
}
#endif
""")

# SDK: MB_LEN_MAX must be at least MB_CUR_MAX of the UTF-8 locales (6 here): a program that has a buffer of MB_LEN_MAX bytes for wcrtomb () would overflow it (the fortified
# check says "overflow detected" and the program ends).  glibc has 16.
_l = open(inc + "/limits.h").read()
if "SDK: MB_LEN_MAX" not in _l:
    import re
    _l2 = re.sub(r"#define __MB_LEN_MAX\s+\d+", "#define __MB_LEN_MAX 16\t/* SDK: MB_LEN_MAX */", _l)
    open(inc + "/limits.h", "w").write(_l2)

add_before_last_endif("grp.h", "SDK: getgrouplist", """/* SDK: getgrouplist (crt/posix/bsdcompat.c) */
#ifdef __cplusplus
extern "C" {
#endif
int getgrouplist (const char *, gid_t, gid_t *, int *);
#ifdef __cplusplus
}
#endif
""")

add_before_last_endif("sys/types.h", "SDK: off64_t", """/* SDK: off64_t is a type, as in glibc (off_t is 64 bits here): programs and gnulib test for it with <sys/types.h> only */
#if defined _LARGEFILE64_SOURCE || defined _GNU_SOURCE || defined _DEFAULT_SOURCE
#ifndef _SDK_OFF64_T
#define _SDK_OFF64_T
typedef off_t off64_t;
#endif
#endif
""")

add_before_last_endif("search.h", "SDK: lfind", """/* SDK: lfind, lsearch, insque, remque (crt/posix/bsdcompat.c) */
#ifdef __cplusplus
extern "C" {
#endif
void *lfind(const void *, const void *, size_t *, size_t, int (*)(const void *, const void *));
void *lsearch(const void *, void *, size_t *, size_t, int (*)(const void *, const void *));
void insque(void *, void *);
void remque(void *);
#ifdef __cplusplus
}
#endif""")
