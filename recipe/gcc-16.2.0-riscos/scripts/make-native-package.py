#!/usr/bin/env python3
"""Build the PackMan package Gcc16_16.2.0-<REL>_arm.zip: the native (RISC OS-hosted) GCC 16.2.0 tool chain as the application !GCC16 in the RISC OS layout.

usage: make-native-package.py TREE [REL]        TREE = the directory made by make-native-tree.sh (TREE/gcc16: Unix names, executables named ,e1f)

What it does with the Unix-named tree:
  * names: the RISC OS way, like GCCSDK's own package: a file whose name ends in one of the "suffix swap" extensions (h, o, tcc ...) goes into a directory of that name
    (include/stdio.h -> include/h/stdio, lib/crt0.o -> lib/o/crt0); everything else keeps its name.  Zip paths use '/', RISC OS shows it as '.', and a '.' inside a leaf
    name becomes '/' on RISC OS (libgcc.a is libgcc/a), which is also what UnixLib does with Unix names: the compiler finds its files with the default suffix swapping.
  * file types (Info-ZIP ARC0 extra field): executables (,e1f in the tree) &E1F, !Boot/!Run/!Help &FEB, !Sprites &FF9, the rest &FFF (text; headers, libraries, objects:
    nothing looks at their type).
  * the application skeleton: !Boot, !Run (Run$Path, return code limit, filename suffix swapping for every tool), !Help, docs/ReadMe, RiscPkg/Control and Copyright.
    The icon sprites are GCCSDK's (taken from its own gcc_10.2.0-1 package when that is present).
"""
import os, struct, sys, time, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pkgmeta

HOME = os.path.expanduser("~")
TREE = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
REL = sys.argv[2] if len(sys.argv) > 2 else "1"
T, V = "arm-riscos-gnueabihf", "16.2.0"
OUT = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), "..", "..", "..", "release", "Gcc16_%s-%s_arm.zip" % (V, REL))
OUT = os.path.normpath(OUT)
if os.environ.get("PKG_OUT"):
    OUT = os.path.join(os.environ["PKG_OUT"], "Gcc16_%s-%s_arm.zip" % (V, REL))
APP = "Apps/Utilities/!GCC16/"
GCCSDK_ZIP = HOME + "/gccsdk/autobuilder/autobuilder_packages/arm/Development/gcc_10.2.0-1_arm.zip"

# the extensions UnixLib swaps with a directory (GCCSDK's !Run list plus the Fortran 90+ ones); only those that occur in the tree matter
SWAP = set("f for F f90 F90 f95 F95 f03 F03 f08 F08 fpp cc cxx cpp c++ C i ii rpo c m h hh s S xrb xrs l o y tcc cmhg adb ads ali".split())
TOOLS = ("gcc g++ cpp gcov gfortran cc1 cc1plus f951 collect2 lto1 lto-wrapper as ld ar nm objdump objcopy readelf strip ranlib size strings addr2line c++filt elfedit make").split()

def riscos_stamp(unix):
    cs = int((unix + 2208988800) * 100)
    return (cs >> 32) & 0xFF, cs & 0xFFFFFFFF

def extra(ftype, unix, attr=0x13):   # Info-ZIP "AC" (ARC0) field: RISC OS load/exec (file type + date) and attributes, plus the Unix time field
    hi, lo = riscos_stamp(unix)
    load = 0xFFF00000 | (ftype << 8) | hi
    ac = b"ARC0" + struct.pack("<III", load, lo, attr) + b"\0\0\0\0"
    return struct.pack("<HH", 0x4341, len(ac)) + ac + struct.pack("<HHBI", 0x5455, 5, 1, int(unix))

NOW = time.time()
entries = []                         # (zip name, data or None for a directory, file type, unix mode)

def add_dir(name):
    entries.append((name.rstrip("/") + "/", None, 0, 0o40775))

def add_file(name, data, ftype, mode=0o100664):
    entries.append((name, data, ftype, mode))

def riscos_name(rel):
    """Unix-style relative path -> the zip name with the suffix swapping applied to the leaf."""
    d, leaf = os.path.split(rel)
    if "." in leaf:
        base, ext = leaf.rsplit(".", 1)
        if base and ext in SWAP:
            return os.path.join(d, ext, base)
    return rel

# ---- the tree
root = os.path.join(TREE, "gcc16")
if not os.path.isdir(root):
    sys.exit("no %s: run make-native-tree.sh first" % root)
dirs, files = set(), []
for dp, dn, fn in os.walk(root):
    for f in fn:
        p = os.path.join(dp, f)
        rel = os.path.relpath(p, root)
        ftype = 0xFFF
        if rel.endswith(",e1f"):
            rel, ftype = rel[:-4], 0xE1F
        files.append((riscos_name(rel), p, ftype))
for name, p, ftype in sorted(files):
    d = os.path.dirname(name)
    while d:
        dirs.add(d)
        d = os.path.dirname(d)
    files_data = open(p, "rb").read()
    add_file(APP + name, files_data, ftype, 0o100775 if ftype == 0xE1F else 0o100664)

# ---- the application skeleton
sfix = "f:for:F:f90:F90:f95:F95:f03:F03:f08:F08:fpp:cc:cxx:cpp:c++:C:i:ii:rpo:c:m:h:hh:s:S:xrb:xrs:l:o:y:tcc:cmhg:adb:ads:ali"
boot = '''| !Boot file for the native GCC 16.2.0 (GCCSDK forward-port)

Set GCC16$Dir <Obey$Dir>

IconSprites <GCC16$Dir>.!Sprites

Set GCC16$Help <GCC16$Dir>.docs.ReadMe
Set GCC16$Version "16.2.0-%s"
Set GCC16$Title "GCC 16"
Set GCC16$Description "C/C++ compiler (experimental forward-port of the GCCSDK EABI tool chain, GCC 16.2.0, binutils 2.45.1)"
''' % REL
run = '''| !Run file for the native GCC 16.2.0 (GCCSDK forward-port)

Run <Obey$Dir>.!Boot

| Search path for the programs: the drivers (gcc, g++, cpp) and the binutils (as, ld, ar, nm, objdump ...)
If "<GCC16bin$Path>" = "" Then Set Run$Path <Run$Path>,GCC16bin:,GCC16tbin:
Set GCC16bin$Path <GCC16$Dir>.bin.
Set GCC16tbin$Path <GCC16$Dir>.%s.bin.

| The return code limit: UnixLib encodes information within this range, so a failing compile must not be reported as a RISC OS error.
Set Sys$RCLimit 65536

| Ensure correct filename translation for a variety of prefixes (foo.c <-> c.foo, foo.h <-> h.foo, foo.o <-> o.foo ...)
Set UnixEnv$gcc$sfix "%s"
''' % (T, sfix)
for t in TOOLS[1:]:
    run += "Set UnixEnv$%s$sfix <UnixEnv$gcc$sfix>\n" % t
run += '''
| Every program of the tool chain keeps its heap in a dynamic area (built in).  The MAXIMUM size of a heap is only reserved address space, but the reservations of the programs
| that are alive at once (make, gcc, collect2, ld ...) add up and must fit: the drivers (gcc, g++, cpp, collect2) and make have 32 MB, the compilers proper (cc1, cc1plus) and
| the binutils programs 512 MB.  <program>$HeapMax (an integer, in MB) changes the maximum of one program, e.g.   SetEval cc1plus$HeapMax 1024   or   SetEval make$HeapMax 128
| The main stacks are built in too (cc1, cc1plus, f951: 64 MB; make: 8 MB; the binutils and the drivers: 1 MB, UnixLib's default): with SharedLibs-C-armeabihf 16.2.0-6 or later (Gcc16 16.2.0-8 needs 16.2.0-10).

| Ensure the latest version of SUL:
RMEnsure SharedUnixLibrary 1.12 RMLoad System:Modules.SharedULib
RMEnsure SharedUnixLibrary 1.12 Error The GCC 16 tool chain requires SharedUnixLibrary 1.12 or later
'''
helpf = '''| !Help file for the native GCC 16

Run <Obey$Dir>.!Boot
Filer_Run <GCC16$Help>
'''
readme = '''GCC 16.2.0 for RISC OS (native)
===============================
An experimental forward-port of the GCCSDK EABI tool chain: GCC 16.2.0 (C, C++ and Fortran) and binutils 2.45.1, running ON RISC OS (arm-riscos-gnueabihf, hard-float, UnixLib).
Project, source, documentation and bug reports: https://github.com/robheaton/riscos-gcc16  (an independent port: please do not report problems to the GCCSDK mailing list).
@@REQ@@

Using it
  1. Double-click !GCC16 once (it sets Run$Path, the return code limit and UnixLib's filename suffix swapping for the tools).
  2. Open a Task window.  The program image of the C++ compiler is 30 MB and the driver that starts it is saved next to it, so the Task window needs an application space
     of at least 48 MB (Task Manager: the "Next" slot, or  WimpSlot -min 48M -max 48M  in the window).
  3.  gcc -O2 -o hello hello.c        g++ -O2 -o hello hello.cc        gfortran -O2 -o hello hello.f90        (UnixLib maps hello.c to c.hello, hello.f90 to f90.hello, hello.o to o.hello ...)
      The linker gives the program the file type ELF (&E1F): run it by typing its name.
  4. ar, nm, objdump, readelf, strip, size ... are in !GCC16.arm-riscos-gnueabihf.bin (on Run$Path after step 1).
  5. make (GNU make 4.4.1) is in !GCC16.bin.  It never uses a shell: every recipe line is split into words and run directly, so a recipe can call gcc, g++, ar and RISC OS
     commands (Copy, Delete, Echo, Stamp ...) but has no pipes, redirections or && ; the default C compiler is gcc.  The c/h/o directories work as for the compiler
     (the makefile names main.c, util.h, main.o, as on Unix).  Programs are found through Run$Path.

@@THROWBACK@@Notes
  * Programs are linked against the shared UnixLib; libstdc++ and libgfortran are linked statically (the package has no libstdc++.so or libgfortran.so, so Fortran and C++ programs
    do not need the runtime packages for those).  Fortran: gfortran is the driver, the compiler proper f951 is in libexec; the module files it writes (chk.mod) are in the current directory.
  * The memory of every tool is a dynamic area (maximum 32 MB for the drivers and make, 512 MB for cc1, cc1plus, f951 and the binutils; <program>$HeapMax in MB changes it).
  * The main stack: cc1, cc1plus and f951 ask for 64 MB (deeply recursive templates and constexpr evaluation need about 4 KB per level: the 1 MB every EABI program had ends at
    a depth of about 250, the compiler's own limits are 900 and 512), make for 8 MB, as, ld, the other binutils programs and the drivers for nothing (1 MB, UnixLib's default).  ARMEABISupport maps a
    stack page when it is first touched, so a big stack costs address space only; the stacks of ALL EABI programs share one 256 MB range, and when it is short UnixLib tries half the
    size, and so on.  This needs SharedLibs-C-armeabihf 16.2.0-6 or later: an older libunixlib ignores the request and every stack stays 1 MB.
  * If a program stops with "Unable to allocate logical address space", the free address space was too small for the maximum heap size it asked for (it is only reserved, not
    used).  libunixlib 16.2.0-6 and later then tries half the size, and so on, by itself; with an older one lower the size for that program, e.g.   SetEval cc1plus$HeapMax 256
    (MB).  Programs that are alive together (make, gcc, collect2, ld) each reserve theirs.
  * Temporary files: TMPDIR, or UnixFS$/tmp (Wimp$ScrapDir by default).  Keep them on a local disk: with TMPDIR on a network share the compiler was about 20% slower.
  * Tested on a Cortex-A72 machine: the C regression suite (34541 checks) and the C++ suite (139 checks) compile, link and run with it, and so does zlib 1.3.1.
'''
REQ_OLD = """Needs the SharedLibs-C-armeabihf package, version 16.2.0-5 or later (UnixLib with the stack fixes), and ARMEABISupport / SOManager.  With 16.2.0-6 or later the compilers get big
main stacks (see Notes); with 16.2.0-5 everything works as before, with 1 MB stacks."""
REQ_NEW = """Needs the SharedLibs-C-armeabihf package, version 16.2.0-10 or later (UnixLib with the stack fixes and the repair for the DDEUtils module: with an older UnixLib the compilers cannot run
while the DDEUtils module is loaded, and every text editor that does throwback loads it), and ARMEABISupport / SOManager."""
REQ_12 = """Needs the SharedLibs-C-armeabihf package, version 16.2.0-12 or later (UnixLib fix level 14: the stack fixes, the repair for the DDEUtils module and, new in 16.2.0-12, the exit functions
of programs: the counts of a --coverage program are written by one), and ARMEABISupport / SOManager."""
THROWBACK = """Throwback (-mthrowback), new in 16.2.0-8
  gcc -Wall -mthrowback -c main.c         g++ -mthrowback ...         gfortran -mthrowback ...         (add it to the compiler options of your makefile)
  Every diagnostic that has a file and a line (errors, warnings, notes, fatal errors; C, C++ and Fortran) is also sent to your text editor through the DDEUtils module, as the Norcroft DDE does:
  the editor lists them in its throwback window and a double click opens the file at the line.  The text on the screen does not change, and neither does the code that is generated.
  Needs the DDEUtils module loaded (the DDE, !StrongED and others load it from System:Modules) and an editor that has registered with it (StrongED: "Throwback requests" in its Choices).
  An editor can only register with a module that is already there: if DDEUtils was loaded after the editor started, restart the editor (in the tests StrongED also took the registration over when
  another throwback receiver left).  Set THROWBACK_DEBUG to any value to be told on the screen why no throwback arrives (no DDEUtils, no editor registered, not run in the desktop ...).
  Throwback needs the desktop: it does nothing outside it.  Not done yet: the assembler and the linker (their messages do not go to the editor).

"""
LTO = """Link time optimisation (-flto), new in 16.2.0-10
  gcc -O2 -flto -o prog a.c b.c        g++ -O2 -flto ...        gfortran -O2 -flto ...        (or  gcc -O2 -flto -c a.c  for each file, then  gcc -O2 -flto a.o b.o -o prog)
  The compiler writes its intermediate form into the object files next to the normal code ("fat" LTO objects: they also link without LTO), and the link step recompiles the whole
  program at once, so that functions can be inlined across files.  The native linker has no plugin support, so this goes through collect2 and lto-wrapper (programs of the package),
  not through the linker plugin that the Linux cross compiler uses; the consequences: objects in an archive (libfoo.a made with ar) are NOT optimised across modules (give the object
  files to the link directly), and the LTRANS parts are compiled one after the other.  The link needs more memory and time than a plain link (lto1 is as big as cc1); the Task window
  settings of the Notes apply.  Tested on RISC OS: C, C++ and Fortran programs of two files, the whole test programs rotest (34541 checks) and cxxtest (139 checks) built and linked with -flto.
  -flto=N, -flto=auto and a make jobserver are accepted and do the same as -flto (from 16.2.0-11: the LTRANS jobs run one after the other; a process cannot run alongside another one inside a
  task, so make -jN could not run them in parallel anyway).  (Three programs of this package differ from stock GCC 16.2.0 for this: collect2 and lto-wrapper pass the list of the objects through a
  file, because under UnixLib a redirected child process loses its error messages, and lto-wrapper runs serially; lto1 reads the numbers of its section names without the 64 bit scanf conversions
  that UnixLib before 16.2.0-11 does not have.)

"""
COVERAGE = """Coverage and profile-guided optimisation, new in 16.2.0-12
  gcc -O0 --coverage -c prog.c         compile with instrumentation (also g++ and gfortran); the compiler writes prog.gcno next to the object file
  gcc --coverage -o prog prog.o        link
  prog                                 when the program ends it adds its counts to prog.gcda
  gcov prog.c                          prints the percentage of the lines that ran and writes the annotated source prog.c.gcov   (-b: branches, -c: counts)
  Compile with -c and link in a second step: in one command (gcc --coverage -o prog prog.c) GCC names the data files prog-prog.gcno and prog-prog.gcda after the output AND the source.
  Profile-guided optimisation: gcc -O2 -fprofile-generate -c prog.c ; gcc -fprofile-generate -o prog prog.o ; prog (on typical input) ; gcc -O2 -fprofile-use -c prog.c ; gcc -o prog prog.o
  gcov is in !GCC16.bin.  The counts are written by a destructor of the program, which UnixLib runs from 16.2.0-12 on (a program that ends with _exit, abort or a crash writes nothing).
  The file names follow the usual UnixLib rule: prog.gcno, prog.gcda and prog.c.gcov are the RISC OS files prog/gcno, prog/gcda and prog/c/gcov.  (UnixLib would take prog.c.gcov for a RISC OS path,
  the file gcov in the directories prog and c: gcov asks for Unix names only, and a program of your own that opens such a file needs  int __riscosify_control = __RISCOSIFY_STRICT_UNIX_SPECS;
  with  #include <unixlib/local.h> .)  Not available: gprof and -pg (the program links and runs but writes no gmon.out).

"""
if int(REL) >= 9:
    THROWBACK = THROWBACK + LTO
if int(REL) >= 12:
    THROWBACK = THROWBACK + COVERAGE
if int(REL) >= 12:
    readme = readme.replace("@@REQ@@", REQ_12).replace("@@THROWBACK@@", THROWBACK)
    # the binutils programs have the 8 MB stack request again (data/riscos-da-big.c: the 16.2.0-11 build was made before it was added)
    ra = "make for 8 MB, as, ld, the other binutils programs and the drivers for nothing (1 MB, UnixLib's default)."
    assert ra in readme
    readme = readme.replace(ra, "make and the binutils programs (as, ld ...) for 8 MB, the drivers for nothing (1 MB, UnixLib's default).")
    rb = "make: 8 MB; the binutils and the drivers: 1 MB, UnixLib's default): with SharedLibs-C-armeabihf 16.2.0-6 or later (Gcc16 16.2.0-8 needs 16.2.0-10)."
    assert rb in run
    run = run.replace(rb, "make and the binutils: 8 MB; the drivers: 1 MB, UnixLib's default): with SharedLibs-C-armeabihf 16.2.0-6 or later (Gcc16 16.2.0-12 needs 16.2.0-12).")
elif int(REL) >= 8:
    readme = readme.replace("@@REQ@@", REQ_NEW).replace("@@THROWBACK@@", THROWBACK)
else:
    readme = readme.replace("@@REQ@@", REQ_OLD).replace("@@THROWBACK@@", "")
control = pkgmeta.control("Gcc16", "%s-%s" % (V, REL), "GPL",
                          "Experimental native GCC 16.2.0 (C, C++, Fortran), binutils 2.45.1 and GNU make 4.4.1 for RISC OS (forward-port of the GCCSDK EABI tool chain)",
                          depends="SharedLibs-C-armeabihf (>= %s)" % ("16.2.0-12" if int(REL) >= 12 else "16.2.0-10" if int(REL) >= 8 else "16.2.0-5"),
                          components="Apps.Utilities.!GCC16 (Movable LookAt)")
copyright = pkgmeta.copyright_gcc16("%s-%s" % (V, REL))
add_file(APP + "!Boot", boot.encode(), 0xFEB)
add_file(APP + "!Run", run.encode(), 0xFEB)
add_file(APP + "!Help", helpf.encode(), 0xFEB)
add_file(APP + "docs/ReadMe", readme.encode(), 0xFFF)
if os.path.exists(GCCSDK_ZIP):
    g = zipfile.ZipFile(GCCSDK_ZIP)
    for n in ("!Sprites", "!Sprites22"):
        add_file(APP + n, g.read("Apps/Utilities/!GCC/" + n), 0xFF9)
add_file("RiscPkg/Control", control.encode(), 0xFFF)
add_file("RiscPkg/Copyright", copyright.encode(), 0xFFF)

# ---- write the zip (directories first, so the extractor creates them with the right names)
alld = set()
for name, data, ftype, mode in entries:
    d = os.path.dirname(name.rstrip("/"))
    while d:
        alld.add(d)
        d = os.path.dirname(d)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for d in sorted(alld):
        zi = zipfile.ZipInfo(d + "/", date_time=time.localtime(NOW)[:6])
        zi.create_system = 3
        zi.external_attr = (0o40775 << 16) | 0x10
        zi.extra = struct.pack("<HHBI", 0x5455, 5, 1, int(NOW))
        z.writestr(zi, b"")
    for name, data, ftype, mode in entries:
        if data is None:
            continue
        zi = zipfile.ZipInfo(name, date_time=time.localtime(NOW)[:6])
        zi.create_system = 3
        zi.external_attr = mode << 16
        zi.compress_type = zipfile.ZIP_DEFLATED
        zi.extra = extra(ftype, NOW)
        z.writestr(zi, data)
chk = zipfile.ZipFile(OUT)
assert chk.testzip() is None
print("wrote", OUT, os.path.getsize(OUT), "bytes,", len([e for e in entries if e[1] is not None]), "files")
