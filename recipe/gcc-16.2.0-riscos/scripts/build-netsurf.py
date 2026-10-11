#!/usr/bin/env python3
"""build-netsurf.py - fetch and build NetSurf (the development version, RISC OS front end) and everything it needs with the GCC 16 cross compiler, and make a test application folder.

usage: build-netsurf.py --toolchain DIR --work DIR [--opt "FLAGS"] [--jobs N] [--only STEP] [--no-fetch]

  --toolchain DIR   the cross tool chain (the unpacked tarball) with OSLib installed in it (install-oslib.py): it is not changed
  --work DIR        everything goes here: src/ (the downloads), env/ (the install prefix: headers, libraries, pkg-config files), build/, out/
  --opt "FLAGS"     compiler flags for everything that is built from source (default "-O2"), for example
                    "-O2 -mcpu=cortex-a72 -mfpu=neon-fp-armv8 -flto" (with -flto, -ffat-lto-objects is added, and the flags go to the link of NetSurf as well)
  --pgo-dir DIR     profile-guided build: DIR is made by pgo-collect.py from the .gcda files that an instrumented build ("--opt ... -fprofile-generate") wrote on RISC OS
  --release         a build to hand out: NetSurf compiled with NETSURF_LOG_LEVEL=WARNING (the logging calls below that level are not in the program), and a !Run that sets NetSurf$Logging 0 (docs/NETSURF.md)
  --only STEP       run one step (fetch, hosttools, thirdparty, nslibs, netsurf, pack)
  --no-fetch        do not download anything (the sources must be in src/ already)

NetSurf's own sources are cloned from git://git.netsurf-browser.org (the latest commit: the script records the commits in out/BUILD-INFO.txt); the third-party sources are the release tarballs below, checked against
the SHA-256 that were recorded when this recipe was made (the signatures could not be checked: the keys were not at hand).  ccres, the template compiler, is a subversion checkout of svn://svn.riscos.info/ccres/trunk.
Needs on the build machine: git, svn, make, cmake, perl, flex, bison, gperf, pkg-config, patch, a host C compiler.  docs/NETSURF.md describes the result, what was changed and what is missing."""
import argparse, hashlib, os, re, shutil, subprocess, sys, urllib.request

ap = argparse.ArgumentParser()
ap.add_argument("--toolchain", required=True)
ap.add_argument("--work", required=True)
ap.add_argument("--opt", default="-O2")
ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
ap.add_argument("--pgo-dir", help="a folder of profile data made by pgo-collect.py: the build is then profile-guided (-fprofile-use)")
ap.add_argument("--release", action="store_true", help="a build to hand out: no log (the program logs warnings and errors only, and the !Run says NetSurf$Logging 0), no -v")
ap.add_argument("--only")
ap.add_argument("--no-fetch", action="store_true")
a = ap.parse_args()
TC = os.path.abspath(a.toolchain); WORK = os.path.abspath(a.work)
if a.pgo_dir:       # -fprofile-prefix-path: the profile files are looked up by the path of each object relative to --work, so the instrumented build and this one may be in different folders
    a.opt += " -fprofile-use=%s -fprofile-prefix-path=%s -fprofile-correction -Wno-missing-profile -Wno-coverage-mismatch" % (os.path.abspath(a.pgo_dir), WORK)
PROFILE = "-fprofile" in a.opt
LTO = "-flto" in a.opt
if LTO and "-ffat-lto-objects" not in a.opt:
    a.opt += " -ffat-lto-objects"       # the static libraries then link with or without LTO, and "ar" needs no plugin
SRC = WORK + "/src"; ENV = WORK + "/env"; BLD = WORK + "/build"; OUT = WORK + "/out"; HOST = WORK + "/hosttools"; CROSSBIN = WORK + "/crossbin"
HERE = os.path.dirname(os.path.abspath(__file__))
PATCHES = os.path.join(HERE, "..", "patches-rufl")
T = "arm-riscos-gnueabihf"; TBIN = TC + "/bin"; SYSROOT = TC + "/" + T
for need in (TBIN + "/" + T + "-gcc", SYSROOT + "/include/oslib/wimp.h", SYSROOT + "/lib/libOSLib32.a"):
    if not os.path.exists(need): sys.exit("%s is missing (the tool chain, with OSLib installed by install-oslib.py)" % need)

NS_REPOS = ["buildsystem", "libwapcaplet", "libparserutils", "libhubbub", "libcss", "libdom", "libnsbmp", "libnsgif", "libnsutils", "libnslog", "libnspsl", "libsvgtiny", "libpencil", "librufl", "nsgenbind", "netsurf"]
THIRD = [   # name, file, url, sha256
    ("expat", "expat-2.9.0.tar.xz", "https://github.com/libexpat/libexpat/releases/download/R_2_9_0/expat-2.9.0.tar.xz", "1e6371862cc31999b368c3b89b49994f0677e1bab5f1b2b85ae3741f5d803051"),
    ("zlib", "zlib-1.3.2.tar.xz", "https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.xz", "d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3"),
    ("curl", "curl-8.22.0.tar.xz", "https://curl.se/download/curl-8.22.0.tar.xz", "f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7"),
    ("libjpeg-turbo", "libjpeg-turbo-3.2.0.tar.gz", "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz", "6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e"),
    ("libpng", "libpng-1.6.59.tar.gz", "https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.59.tar.gz", "2540302a1844ad2b2b501977abecfa850f265f97b78f065a712ab4074a89f5b5"),
    ("openssl", "openssl-4.0.3.tar.gz", "https://github.com/openssl/openssl/releases/download/openssl-4.0.3/openssl-4.0.3.tar.gz", "325b5c806167c13b40b1ffeadfe0248197c00eccc4cf123ec1e28d2d2fd216d9"),
]

def sh(cmd, cwd=None, env=None, log=None):
    e = dict(os.environ, PATH=HOST + "/bin:" + TBIN + ":" + os.environ["PATH"], PKG_CONFIG_PATH=ENV + "/lib/pkgconfig", PKG_CONFIG_LIBDIR=ENV + "/lib/pkgconfig")
    if env: e.update(env)
    r = subprocess.run(cmd, cwd=cwd, env=e, shell=isinstance(cmd, str), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if log:
        os.makedirs(os.path.dirname(log), exist_ok=True)
        open(log, "w").write(r.stdout)
    if r.returncode:
        sys.exit("FAILED: %s\n%s" % (cmd if isinstance(cmd, str) else " ".join(cmd), r.stdout[-3000:]))
    return r.stdout

def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def step_fetch():
    os.makedirs(SRC, exist_ok=True)
    if a.no_fetch: print("fetch: skipped (--no-fetch)"); return
    for r in NS_REPOS:
        d = SRC + "/" + r
        if os.path.isdir(d): continue
        print("git clone", r); sh(["git", "clone", "-q", "--depth", "1", "git://git.netsurf-browser.org/%s.git" % r, d])
    for name, fn, url, want in THIRD:
        p = SRC + "/" + fn
        if not os.path.exists(p):
            print("download", fn); urllib.request.urlretrieve(url, p)
        got = sha256(p)
        if got != want: sys.exit("%s: sha256 %s is not the recorded %s" % (fn, got, want))
        d = SRC + "/" + fn.split(".tar")[0]
        if not os.path.isdir(d): sh(["tar", "-xf", p, "-C", SRC])
    if not os.path.isdir(SRC + "/ccres"):
        print("svn checkout ccres"); sh(["svn", "checkout", "-q", "svn://svn.riscos.info/ccres/trunk/", SRC + "/ccres"])

def make_env():
    """the install prefix: OSLib's headers and library are linked in (the tool chain stays as it is)"""
    os.makedirs(ENV + "/include", exist_ok=True); os.makedirs(ENV + "/lib/pkgconfig", exist_ok=True)
    for src, dst in ((SYSROOT + "/include/oslib", ENV + "/include/oslib"), (SYSROOT + "/lib/libOSLib32.a", ENV + "/lib/libOSLib32.a")):
        if not os.path.lexists(dst): os.symlink(src, dst)
    # c-ares: the RISC OS front end's Makefile asks for it, but no code calls it (curl is built without it): an empty pkg-config file
    open(ENV + "/lib/pkgconfig/libcares.pc", "w").write("# stub: no c-ares is built or linked (NetSurf's Makefile asks for it, no code calls it)\nName: libcares\nDescription: stub\nVersion: 1.34.0\nLibs:\nCflags:\n")
    os.makedirs(CROSSBIN, exist_ok=True)
    for f in os.listdir(TBIN):
        if not os.path.lexists(CROSSBIN + "/" + f): os.symlink(TBIN + "/" + f, CROSSBIN + "/" + f)

def step_hosttools():
    os.makedirs(HOST + "/bin", exist_ok=True)
    sh(["make", "-C", SRC + "/buildsystem", "install", "PREFIX=" + ENV], log=BLD + "/buildsystem.log")
    sh(["make", "-C", SRC + "/nsgenbind", "install", "PREFIX=" + HOST, "NSSHARED=" + ENV + "/share/netsurf-buildsystem"], log=BLD + "/nsgenbind.log")
    # ccres: the native tool; it wants a folder with only OSLib's headers in it
    oi = WORK + "/oslib-only"; os.makedirs(oi + "/include", exist_ok=True)
    if not os.path.lexists(oi + "/include/oslib"): os.symlink(SYSROOT + "/include/oslib", oi + "/include/oslib")
    cb = BLD + "/ccres"
    if os.path.exists(cb): shutil.rmtree(cb)
    shutil.copytree(SRC + "/ccres", cb, ignore=shutil.ignore_patterns(".svn"))
    sh(["make", "GCCSDK_INSTALL_ENV=" + oi], cwd=cb, log=BLD + "/ccres.log")
    shutil.copy(cb + "/Release/ccres", HOST + "/bin/ccres")
    if not os.path.lexists(CROSSBIN + "/ccres"): os.symlink(HOST + "/bin/ccres", CROSSBIN + "/ccres")

def cross_env():
    return dict(CC=TBIN + "/" + T + "-gcc", AR=TBIN + "/" + T + "-ar", RANLIB=TBIN + "/" + T + "-ranlib")

def step_thirdparty():
    ce = cross_env(); opt = a.opt; J = str(a.jobs)
    def autotools(name, d, extra, log, cflags=opt, cppflags=""):
        b = BLD + "/" + name
        if os.path.exists(b): shutil.rmtree(b)
        os.makedirs(b)
        e = dict(ce, CFLAGS=cflags, CPPFLAGS=cppflags)
        sh([SRC + "/" + d + "/configure", "--host=" + T, "--prefix=" + ENV, "--disable-shared", "--enable-static"] + extra, cwd=b, env=e, log=BLD + "/" + name + "-configure.log")
        sh(["make", "-j" + J], cwd=b, env=e, log=BLD + "/" + name + "-make.log")
        sh(["make", "install"], cwd=b, env=e, log=BLD + "/" + name + "-install.log")
    # expat: the random numbers for the hash salt come from /dev/urandom (UnixLib reads it from the CryptRandom module)
    autotools("expat", "expat-2.9.0", ["--without-docbook", "--without-examples", "--without-tests", "--without-xmlwf"], "expat", cppflags="-DXML_DEV_URANDOM")
    # zlib has its own configure
    b = BLD + "/zlib"
    if os.path.exists(b): shutil.rmtree(b)
    shutil.copytree(SRC + "/zlib-1.3.2", b)
    sh(["./configure", "--static", "--prefix=" + ENV], cwd=b, env=dict(ce, CFLAGS=opt), log=BLD + "/zlib-configure.log")
    sh(["make", "-j" + J, "libz.a"], cwd=b, log=BLD + "/zlib-make.log"); sh(["make", "install"], cwd=b, log=BLD + "/zlib-install.log")
    # the cmake tool chain file of libjpeg-turbo and libpng
    tcf = BLD + "/riscos-toolchain.cmake"
    open(tcf, "w").write("set(CMAKE_SYSTEM_NAME Generic)\nset(CMAKE_SYSTEM_PROCESSOR arm)\nset(CMAKE_C_COMPILER %s/%s-gcc)\nset(CMAKE_AR %s/%s-ar)\nset(CMAKE_RANLIB %s/%s-ranlib)\n"
                          "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)\nset(CMAKE_FIND_ROOT_PATH %s)\nset(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\nset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n" % (TBIN, T, TBIN, T, TBIN, T, ENV))
    # libjpeg-turbo: SIMD only if the flags give NEON (the 32-bit ARM NEON code is assembler); the test programs do not build (fmemopen), so only the library
    simd = "1" if "neon" in opt else "0"
    b = BLD + "/jpeg"
    if os.path.exists(b): shutil.rmtree(b)
    os.makedirs(b)
    sh(["cmake", SRC + "/libjpeg-turbo-3.2.0", "-DCMAKE_TOOLCHAIN_FILE=" + tcf, "-DCMAKE_INSTALL_PREFIX=" + ENV, "-DENABLE_SHARED=0", "-DENABLE_STATIC=1", "-DWITH_SIMD=" + simd, "-DWITH_TURBOJPEG=0", "-DWITH_JPEG8=1", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_FLAGS=" + opt], cwd=b, log=BLD + "/jpeg-configure.log")
    sh(["cmake", "--build", ".", "--target", "jpeg-static", "-j" + J], cwd=b, log=BLD + "/jpeg-make.log")
    for comp in ("lib", "include", "dev"): sh(["cmake", "--install", ".", "--component", comp], cwd=b, log=BLD + "/jpeg-install-%s.log" % comp)
    b = BLD + "/png"
    if os.path.exists(b): shutil.rmtree(b)
    os.makedirs(b)
    sh(["cmake", SRC + "/libpng-1.6.59", "-DCMAKE_TOOLCHAIN_FILE=" + tcf, "-DCMAKE_INSTALL_PREFIX=" + ENV, "-DPNG_SHARED=OFF", "-DPNG_STATIC=ON", "-DPNG_TESTS=OFF", "-DPNG_TOOLS=OFF", "-DPNG_HARDWARE_OPTIMIZATIONS=OFF", "-DZLIB_ROOT=" + ENV, "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_FLAGS=" + opt], cwd=b, log=BLD + "/png-configure.log")
    sh(["cmake", "--build", ".", "-j" + J], cwd=b, log=BLD + "/png-make.log"); sh(["cmake", "--install", "."], cwd=b, log=BLD + "/png-install.log")
    if not os.path.lexists(ENV + "/lib/libpng16.a"): os.symlink("libpng.a", ENV + "/lib/libpng16.a")      # NetSurf asks for -lpng16
    # OpenSSL: a generic 32-bit Unix target, no assembler, no threads; UnixLib has no EPROTO (OpenSSL only uses it to recognise a non-fatal socket error)
    b = BLD + "/openssl"
    if os.path.exists(b): shutil.rmtree(b)
    os.makedirs(b)
    sh([SRC + "/openssl-4.0.3/Configure", "BSD-generic32", "--prefix=" + ENV, "--openssldir=" + ENV + "/ssl", "no-shared", "no-pic", "no-asm", "no-threads", "no-dso", "no-docs", "no-tests", "no-ui-console"] + opt.split() + ["-DEPROTO=9999"], cwd=b, env=ce, log=BLD + "/openssl-configure.log")
    sh(["make", "-j" + J, "build_libs"], cwd=b, env=ce, log=BLD + "/openssl-make.log"); sh(["make", "install_dev"], cwd=b, env=ce, log=BLD + "/openssl-install.log")
    # curl with OpenSSL (NetSurf needs a TLS backend even to start: it sets CURLOPT_CAINFO)
    autotools("curl", "curl-8.22.0", ["--with-openssl=" + ENV, "--without-libpsl", "--with-zlib=" + ENV, "--disable-ldap", "--disable-ldaps", "--disable-rtsp", "--disable-dict", "--disable-telnet", "--disable-tftp", "--disable-pop3",
                                         "--disable-imap", "--disable-smtp", "--disable-gopher", "--disable-mqtt", "--disable-manual", "--disable-docs", "--without-brotli", "--without-zstd", "--without-libidn2",
                                         "--without-nghttp2", "--disable-threaded-resolver", "--disable-ipv6"], "curl", cflags=opt)

def step_nslibs():
    ce = cross_env()
    # RUfl: the debug log of the library is hard-wired on and goes to stderr: the patch of this project turns it off
    d = SRC + "/librufl"
    if os.path.exists(PATCHES + "/rufl-log-off.patch") and "RUFL_LOG" not in open(d + "/src/rufl_internal.h").read():
        sh(["patch", "-p1", "--no-backup-if-mismatch", "-s", "-i", os.path.abspath(PATCHES + "/rufl-log-off.patch")], cwd=d)
    for lib in ("libwapcaplet", "libparserutils", "libhubbub", "libcss", "libnsbmp", "libnsgif", "libnsutils", "libnslog", "libnspsl", "librufl", "libpencil", "libdom", "libsvgtiny"):
        def mk(j):
            sh(["make", "-C", SRC + "/" + lib, "install", "-j" + str(j), "HOST=" + T, "PREFIX=" + ENV, "GCCSDK_INSTALL_ENV=" + ENV, "NSSHARED=" + ENV + "/share/netsurf-buildsystem",
                "WARNFLAGS=-Wall", "OPTCFLAGS=-DNDEBUG " + a.opt], env={"GCCSDK_INSTALL_CROSSBIN": CROSSBIN}, log=BLD + "/" + lib + ".log")
        try: mk(a.jobs)
        except SystemExit:      # some of the libraries' makefiles race when run in parallel (libnslog: flex runs before the build directory is made): once more, one job
            print("retrying", lib, "with one job"); mk(1)
        print("built", lib)

def step_netsurf():
    d = SRC + "/netsurf"
    open(d + "/Makefile.config", "w").write(("# written by build-netsurf.py\n" + ("override NETSURF_LOG_LEVEL := WARNING\n" if a.release else "")) + "NETSURF_USE_CURL := YES\nNETSURF_USE_OPENSSL := YES\nNETSURF_USE_JPEG := YES\nNETSURF_USE_PNG := YES\nNETSURF_USE_WEBP := NO\nNETSURF_USE_JPEGXL := NO\n"
                                       "NETSURF_USE_UTF8PROC := NO\nNETSURF_USE_NSPSL := YES\nNETSURF_USE_NSLOG := YES\n")
    e = {"GCCSDK_INSTALL_ENV": ENV, "GCCSDK_INSTALL_CROSSBIN": CROSSBIN}
    if LTO or "-fprofile-generate" in a.opt: e["LDFLAGS"] = a.opt        # the link must know the flags too (-flto, -fprofile-generate ...): NetSurf's Makefile keeps LDFLAGS from the environment
    r = subprocess.run(["make", "TARGET=riscos", "PREFIX=" + ENV, "clean"], cwd=d, env=dict(os.environ, PATH=HOST + "/bin:" + TBIN + ":" + os.environ["PATH"], PKG_CONFIG_PATH=ENV + "/lib/pkgconfig", **e), capture_output=True)
    # the last step of the front end's Makefile (elf2aif -e, which GCCSDK 4.7.4's elf2aif does not know) fails: the ELF program is complete before it, so the make is run with -k and the result is checked
    r = subprocess.run(["make", "-k", "-j" + str(a.jobs), "TARGET=riscos", "PREFIX=" + ENV, "OPTCFLAGS=-DNDEBUG " + a.opt], cwd=d, env=dict(os.environ, PATH=HOST + "/bin:" + TBIN + ":" + os.environ["PATH"], PKG_CONFIG_PATH=ENV + "/lib/pkgconfig", **e), capture_output=True, text=True)
    open(BLD + "/netsurf.log", "w").write(r.stdout + r.stderr)
    if not os.path.exists(d + "/frontends/riscos/appdir/!RunImage,e1f"): sys.exit("NetSurf did not link: see " + BLD + "/netsurf.log")
    left = [l for l in (r.stdout + r.stderr).splitlines() if "error:" in l or "undefined reference" in l]
    if left: sys.exit("errors in the NetSurf build:\n" + "\n".join(left[:10]))

def step_pack():
    d = SRC + "/netsurf"; appdir = d + "/frontends/riscos/appdir"
    app = OUT + "/!NetSurf"
    if os.path.exists(app): shutil.rmtree(app)
    os.makedirs(OUT, exist_ok=True)
    shutil.copytree(appdir, app, symlinks=False)
    subprocess.run([TBIN + "/" + T + "-strip", "-o", app + "/!RunImage,e1f", appdir + "/!RunImage,e1f"], check=True)
    # !Run: the front end's template, then the slot and the program.  The size of the slot that GCCSDK's makerun works out from the file (5.5 MB) is far too small for a program with UnixLib's heap in its slot
    t = open(d + "/frontends/riscos/scripts/Run", newline="").read()
    if a.release:      # to hand out: logging off (the program then logs warnings and errors only) and the log, as the front end's own !Run has it, in the scrap folder
        run = "WimpSlot -min 32000k -max 32000k\nRun <NetSurf$Dir>.!RunImage %*0 2><Wimp$ScrapDir>.WWW.NetSurf.Log\n"
        t = t.replace("Set NetSurf$Logging 1", "Set NetSurf$Logging 0")
    else:              # for testing: verbose, and the log next to the application where it can be read
        run = "WimpSlot -min 32000k -max 32000k\nRun <NetSurf$Dir>.!RunImage -v %*0 2><NetSurf$Dir>.Log\n"
    t, n = re.subn(r"WIMPSLOT\nRun <NetSurf\$Dir>\.!RunImage %\*0 2>[^\n]*\n?", run, t)
    if n != 1: sys.exit("the !Run template of NetSurf has changed: look at frontends/riscos/scripts/Run")
    open(app + "/!Run,feb", "w", newline="").write(t)
    info = ["NetSurf build %s" % sh(["git", "log", "-1", "--format=%h %cs %s"], cwd=d).strip(), "flags: " + a.opt, "toolchain: " + sh([TBIN + "/" + T + "-gcc", "--version"]).splitlines()[0]]
    for r in NS_REPOS:
        if r not in ("netsurf",): info.append("%-15s %s" % (r, sh(["git", "log", "-1", "--format=%h %cs"], cwd=SRC + "/" + r).strip()))
    for name, fn, url, want in THIRD: info.append("%-15s %s sha256 %s" % (name, fn, want))
    open(OUT + "/BUILD-INFO.txt", "w").write("\n".join(info) + "\n")
    print("application folder:", app, "(%d bytes of program)" % os.path.getsize(app + "/!RunImage,e1f"))

STEPS = [("fetch", step_fetch), ("hosttools", step_hosttools), ("thirdparty", step_thirdparty), ("nslibs", step_nslibs), ("netsurf", step_netsurf), ("pack", step_pack)]
os.makedirs(BLD, exist_ok=True)
make_env()
for name, fn in STEPS:
    if a.only and a.only != name: continue
    print("== " + name); fn()
print("done")
