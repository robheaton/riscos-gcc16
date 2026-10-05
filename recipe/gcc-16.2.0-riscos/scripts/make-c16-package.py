#!/usr/bin/env python3
"""Build SharedLibs-C-armeabihf_16.2.0-<REL>_arm.zip: the 10.2.0-1 package with libunixlib.so.5.0.0 and libm.so.1.0.0 REPLACED by the ones built
with the GCC 16.2.0 forward-port compiler and binutils 2.45.1 from the patched UnixLib 5.0 sources (recipe scripts/build-unixlib.sh: exception-safe
pthread_once, pthread_cond_timedwait, sysconf, sleep with threads; fix level 5; from REL 2 also fread/fwrite short-transfer fix; REL 3: stack-buffer touch; REL 4: fix level 6; REL 5: no 64-byte vstm in memcpy/memmove, built with -fstack-clash-protection, fix level 7; REL 6: __stack_size for the EABI main stack, heap dynamic area falls back to a smaller maximum, fix level 8; REL 7: mmap/mremap refuse a request that can never be served, the one page signal stack is freed at process exit, fix level 9; REL 8: the _exit of a vfork child that ends without exec leaves the RMA block of the shared program image alone, fix level 10).  libgcc_s, libdl and the loader stay those of 10.2.0-1.
usage: make-c16-package.py [REL] [BUILD_DIR]   (defaults: 1, ~/gccsdk-next/unixlib-v5/build)"""
import os, struct, sys, time, zipfile
HOME = os.path.expanduser("~")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta
SRC = HOME + "/gccsdk/autobuilder/autobuilder_packages/arm/Development/SharedLibs-C-armeabihf_10.2.0-1_arm.zip"
REL = sys.argv[1] if len(sys.argv) > 1 else "1"
NB = (sys.argv[2] if len(sys.argv) > 2 else HOME + "/gccsdk-next/unixlib-v5/build") + "/.libs/"
OUT = os.environ.get("PKG_OUT", HOME + "/gccsdk-next/release") + "/SharedLibs-C-armeabihf_16.2.0-%s_arm.zip" % REL
os.makedirs(os.path.dirname(OUT), exist_ok=True)
NOTE = ("libunixlib.so.5.0.0 and libm.so.1.0.0: UnixLib 5.0 rebuilt with the experimental GCC 16.2.0 forward-port (binutils 2.45.1), including the fixes of the "
        "10.2.0-5 package: pthread_once without a global lock and exception-safe, pthread_cond_timedwait with centisecond-accurate timeouts, "
        "sleep/usleep/nanosleep with several threads, sysconf (_SC_NPROCESSORS_ONLN) == 1; everything else identical to 10.2.0-1 "
        "(libgcc_s, libdl and the loader are the 10.2.0 ones).")
if REL not in ("0", "1"):
    NOTE += (" 16.2.0-2 and later also fix fread() and fwrite(): after a short read() or write() the loop carried on at the START of the buffer again, "
             "so the byte count came out right but the data did not.")
if REL not in ("0", "1", "2"):
    NOTE += (" 16.2.0-3 and later touch a stack buffer in user mode before read()/fread()/recv()/recvfrom() hand it to RISC OS: when RISC OS itself first touches a page of "
             "the lazily mapped EABI stack, the store that takes the page fault is lost (bytes of the buffer are never written).  16.2.0-4 and later also answer sysconf (0x4700) "
             "with 6 (16.2.0-3 had the fix but, through a packaging error, not that answer).")
if REL.isdigit() and int(REL) >= 5:
    NOTE += (" 16.2.0-5 and later answer sysconf (0x4700) with 7, are themselves built with -fstack-clash-protection (the new default of the compiler: every function with a "
             "stack frame touches the pages of its frame, so no page of the lazily mapped EABI stack is first touched by RISC OS or by a 64-byte store), and have memcpy()/memmove() "
             "without the 64-byte store-multiple (a 64-byte aligned vstm that is the first access to a page of the lazily mapped EABI stack crashed with SIGSEGV).")
if REL.isdigit() and int(REL) >= 6:
    NOTE += (" 16.2.0-6 and later answer sysconf (0x4700) with 8 and have two new start-up features: (1) the main stack of an EABI program is as big as the program's "
             "__stack_size (int, bytes) says, at least the 1 MB it always was (programs without __stack_size, or run on an older libunixlib, are not affected): ARMEABISupport maps stack "
             "pages when they are first touched, so a big stack costs address space only, of which all EABI stacks share 256 MB; when there is no room for the size asked for, half of it "
             "is tried, and so on down to 1 MB; (2) when the heap dynamic area (<program>$Heap, __dynamic_da_name) cannot be created with the maximum size asked for "
             "(__dynamic_da_max_size, <program>$HeapMax: only reserved address space, but the reservations of all running programs add up), half of it is tried, and so on down to "
             "2 MB, instead of the program ending at start-up with \"Unable to allocate logical address space\".")
if REL.isdigit() and int(REL) >= 7:
    NOTE += (" 16.2.0-7 and later answer sysconf (0x4700) with 9 and have two more changes: (1) mmap() and mremap() refuse a request that can never be served - 2 GB or more, or more than "
             "the OS clamp on the size of one dynamic area (OS_DynamicArea 8) - with ENOMEM before ARMEABISupport is asked: ARMEABISupport makes an \"mmap#N\" dynamic area for every request "
             "and, when it then cannot serve it, keeps the area (and the pages claimed for it) until the next reboot, so a malloc of 2 GB - 1 (new char[SIZE_MAX / 2]) used to leave two 100 MB "
             "areas behind and a request a little over the clamp pinned all the memory it had claimed; (2) the one page signal stack that every EABI program takes from ARMEABISupport at "
             "start-up is freed when the process ends (4 KB of the 128 MB range all EABI stacks share were left in use per process until the process at the root of the family ended).")
if REL.isdigit() and int(REL) >= 8:
    NOTE += (" 16.2.0-8 and later answer sysconf (0x4700) with 10 and fix a bug of _exit () that could not be seen on EABI before: a vfork child that ends without exec (the usual reaction to an exec "
             "that failed: vfork (); if (child) { execv (...); _exit (127); }) freed the RMA block of the program image that it shares with its parent, the parent freed the same block a second time "
             "when it ended, and a loop of such children corrupted the RMA heap and froze the machine.  (SharedUnixLibrary 1.16 has a bug of its own in the same place - such a child frees its "
             "parent's main stack, so with the stock module the parent dies first - and a second one: the child's exit grows its parent's Wimp slot to the maximum.  The module fix and this "
             "library fix belong together.)")
if REL.isdigit() and int(REL) >= 9:
    NOTE += (" 16.2.0-9 and later answer sysconf (0x4700) with 11 and fix the heap of a program that was started by vfork () + exec (): SharedUnixLibrary keeps the copy of the parent "
             "between the permitted RAM limit it gives the child and the end of the Wimp slot, but UnixLib's heap code treated that memory as spare slot memory, so a child that needed "
             "more heap than lay between its image and the copy (a native C++ compile: cc1plus with a heap of about 60 MB in a 64 MB slot) wrote over the copy, and the parent resumed "
             "with a destroyed image and died at its next system call (\"Internal error: abort on data transfer\"), although the child had finished normally.  appspace_himem is now never "
             "raised above the permitted RAM limit that the program was started with: brk () then fails with ENOMEM and malloc falls back to mmap, which does not use the slot.  "
             "Nothing changes for a program that was not started by exec.")
if REL.isdigit() and int(REL) >= 10:
    NOTE += (" 16.2.0-10 and later answer sysconf (0x4700) with 12 and repair two bugs that showed with the DDEUtils module loaded (text editors such as StrongED load it): (1) UnixLib's inline SWI "
             "wrappers (incl-local/internal/os.h) read the results of the SWI from local register variables after the asm statement; GCC only guarantees such a variable in its register where it is an "
             "operand of an asm, and GCC 16 at -O2 read r0 after the call of strlen () that follows SWI DDEUtils_GetCLSize in __unixinit: the size of the DDEUtils command line became the length of "
             "the program name.  SOManager's loader hands every program its arguments through the DDEUtils command line whenever the module is loaded, so every program started with arguments longer "
             "than its own name got them cut to the length of the name, in a heap block that was too small for the whole line (the native compilers could not compile at all, and a command line of "
             "more than 1023 characters overran the block).  All 32 wrappers now copy their results out of the registers inside the asm; (2) __get_dde_prefix () measured the DDEUtils prefix with a "
             "loop that tested *prefix instead of *end_prefix and never ended when a prefix was set (*Prefix <dir>, StrongED, DDE Make, chdir ()), and every UnixLib program calls it at start-up.")
if REL.isdigit() and int(REL) >= 11:
    NOTE += (" 16.2.0-11 and later answer sysconf (0x4700) with 13 and teach scanf (vfscanf, so also sscanf, fscanf, vsscanf ...) the length modifiers it did not have: the old BSD vfscanf knew only l, L and h and "
             "converted every integer with strtol or strtoul and stored a long, so \"%llx\" of a number of 16 digits gave ULONG_MAX in the low word and left the high word of the variable as it was, \"%hhx\" "
             "stored a short (over its neighbour), \"%jd\", \"%zu\", \"%td\" and \"%qd\" were not understood, and \"%Lf\" stored a float.  Now ll, q and j (strtoll / strtoull into a long long), hh (a char), z and t "
             "(as big as size_t and ptrdiff_t), \"%n\" with them, and \"%Lf\" (strtold) work as in the C standard.  The first program that needed it was the native GCC's lto1, which reads the 64 bit id of the "
             "sections of its objects with sscanf (\".%llx\") and so could never find them: no -flto link worked.  Nothing else in the library changed (every other object file of the build is identical).")
# the packages from 16.2.0-4 on must contain the code of every fix: look for it in the built library (a silently skipped patch once shipped a package without the fix-level answer)
if REL.isdigit() and int(REL) >= 4:
    import subprocess
    if subprocess.call([HOME + "/gccsdk-next/tools/check-libunixlib.sh", NB + "libunixlib.so.5.0.0"]) != 0:
        sys.exit("ERROR: the built libunixlib is missing a fix: not packaging it")
base = "Resources/!SharedLibs/lib/armeabihf/"
repl = {base + "libunixlib.so.5.0.0": NB + "libunixlib.so.5.0.0", base + "libm.so.1.0.0": NB + "libm.so.1.0.0"}
def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF
def extra(ftype, unix, attr=0x13):          # Info-ZIP "AC" (ARC0) field: RISC OS load/exec (filetype + date) and attributes, plus the Unix time field
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))
NOW = time.time()                            # the replaced files get today's date, so *Info shows which package is installed
src = zipfile.ZipFile(SRC)
with zipfile.ZipFile(OUT, "w") as z:
    for zi in src.infolist():
        data = src.read(zi.filename) if not zi.filename.endswith("/") else b""
        if zi.filename in repl:
            data = open(repl[zi.filename], "rb").read()
        if zi.filename == "RiscPkg/Control":
            data = pkgmeta.control("SharedLibs-C-armeabihf", "16.2.0-" + REL, "Free", pkgmeta.DESCRIPTION["SharedLibs-C-armeabihf"]).encode()
        if zi.filename == "RiscPkg/Copyright":
            data = pkgmeta.copyright_c("16.2.0-" + REL, NOTE).encode()
        n = zipfile.ZipInfo(zi.filename, date_time=zi.date_time)
        n.create_system = zi.create_system
        n.external_attr = zi.external_attr
        n.compress_type = zi.compress_type
        n.extra = extra(0xE1F, NOW) if zi.filename in repl else zi.extra   # replaced files: new date; the rest keeps its ARC0 fields
        z.writestr(n, data)
print("wrote", OUT, os.path.getsize(OUT), "bytes")
chk = zipfile.ZipFile(OUT); assert chk.testzip() is None
for zi in chk.infolist():
    if zi.filename.endswith("/"): continue
    ex = zi.extra; ft = "-"
    if ex[:4] == b"AC\x14\x00":
        load = struct.unpack("<I", ex[8:12])[0]; ft = "%03X" % ((load >> 8) & 0xFFF)
    print("  %-62s type=%-4s %9d bytes" % (zi.filename.replace("Resources/!SharedLibs/lib/", ""), ft, zi.file_size))
print(chk.read("RiscPkg/Control").decode())
