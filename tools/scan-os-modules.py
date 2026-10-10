#!/usr/bin/env python3
"""scan-os-modules.py - what do the C modules of the RISC OS Open sources need from a C library and from CMHG?

usage: scan-os-modules.py [--sources DIR] [--lib libmodkit.a] [--nm NM] [--json OUT] [--detail] [--missing]
                          [--scenario NAME=FUNC,FUNC,...]...

  --sources   a checkout of the RISC OS Open sources (default: $RISCOS_SOURCES)
  --lib       libmodkit.a (default: the work area's tool chain, ~/gccsdk-next/env-f): its defined symbols plus the functions that modkit/include/*.h define 'static inline' are
              "what a module can use today"
  --scenario  an additional scenario: "today" plus these functions
  --detail    one line per C module (libc functions it uses that are missing, CMHG constructs that are missing)
  --missing   the missing functions, with the number of modules that use each

A component is a C module when its Makefile has  include CModule.  Its C sources are the files c/<name> of its OBJS (all of c/ when OBJS cannot be read).  A "library call" is an identifier of the
standard C library (a fixed list below, by header) followed by "(" in the code (comments and literals removed), not a member (. or ->).  This counts the C LIBRARY calls only: a real module also
needs its own headers (most of the source is written for the Norcroft C compiler), so the figures say what the library and the header generator would allow, not what builds untouched.

CMHG: the directives at the start of a line of cmhg/<file>, and the options of the commands (international:, add-syntax: ...), compared with what modkit's cmunge supports (CMHG_OK below)."""
import argparse, json, os, re, subprocess, sys

HOME = os.path.expanduser("~")
KNOWN = {
    "stdio": "printf fprintf sprintf snprintf vprintf vfprintf vsprintf vsnprintf scanf fscanf sscanf vsscanf vscanf vfscanf fopen freopen fclose fflush fread fwrite fgets fputs fgetc fputc getc putc getchar "
             "putchar gets puts ungetc fseek ftell rewind fgetpos fsetpos feof ferror clearerr perror remove rename tmpfile tmpnam setbuf setvbuf",
    "stdlib": "malloc calloc realloc free atoi atol atoll atof strtol strtoul strtoll strtoull strtod strtof strtold rand srand abs labs llabs div ldiv lldiv exit abort atexit system getenv qsort bsearch mblen "
              "mbtowc wctomb mbstowcs wcstombs",
    "string": "memcpy memmove memset memcmp memchr strcpy strncpy strcat strncat strcmp strncmp strcoll strxfrm strchr strrchr strstr strspn strcspn strpbrk strtok strlen strerror strdup strndup strcasecmp "
              "strncasecmp stricmp strnicmp strlcpy strlcat strsep strtok_r bcopy bzero memccpy strnlen",
    "ctype": "isalnum isalpha iscntrl isdigit isgraph islower isprint ispunct isspace isupper isxdigit tolower toupper isblank isascii toascii",
    "time": "time clock difftime mktime asctime ctime gmtime localtime strftime",
    "math": "sin cos tan asin acos atan atan2 sinh cosh tanh exp log log10 pow sqrt ceil floor fabs fmod ldexp frexp modf",
    "setjmp": "setjmp longjmp",
    "signal": "signal raise",
    "locale": "setlocale localeconv",
    "riscos": "_kernel_swi _kernel_oscli _kernel_osbyte _kernel_osword _kernel_osrdch _kernel_oswrch _kernel_osbget _kernel_osbput _kernel_osgbpb _kernel_osfind _kernel_osfile _kernel_osargs "
              "_kernel_getenv _kernel_setenv _kernel_last_oserror _kernel_system _swi _swix",
}
CATEGORY = {f: cat for cat, names in KNOWN.items() for f in names.split()}
# directives of CMHG that modkit's cmunge (modkit/src/cmunge.c) implements today
CMHG_OK = set("title-string help-string date-string initialisation-code finalisation-code service-call-handler command-keyword-table swi-chunk-base-number swi-decoding-table swi-handler-code "
              "irq-handlers vector-handlers generic-veneers event-handler module-is-runnable international-help-file swi-decoding-code module-is-not-reentrant module-is-c-plus-plus".split())
CMHG_OPT_OK = set("min-args max-args gstrans-map help-text invalid-syntax international add-syntax configure status fs-command handler no-handler".split())
CMHG_DIRECTIVES = set("""title-string help-string date-string initialisation-code finalisation-code service-call-handler command-keyword-table swi-chunk-base-number swi-decoding-table swi-handler-code
irq-handlers vector-handlers generic-veneers event-handler international-help-file module-is-runnable library-enter-code library-initialisation-code module-is-not-reentrant module-is-c-plus-plus
vector-traps pdriver-handler no-handler-for-help-and-syntax""".split())
CMD_OPTS = ("international", "add-syntax", "configure", "status", "fs-command", "help", "handler", "no-handler")


def strip_c(text):
    """the code of a C file without comments, string literals and character literals (each replaced by a blank, line breaks kept)"""
    out = []; i = 0; n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2); j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j])); i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i); j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * max(0, j - i - 2) + c); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def makefile_vars(text):
    """NAME = value (with \\ continuations) of a RISC OS Makefile"""
    v = {}
    text = text.replace("\\\n", " ")
    for m in re.finditer(r"^([A-Za-z_][\w]*)\s*[:+]?=\s*(.*)$", text, re.M):
        v[m.group(1)] = m.group(2).split("#")[0].strip()
    return v


def component_kind(text):
    """CModule when the Makefile includes CModule anywhere (some also build an application or a library next to the module), else the first kind it includes"""
    kinds = [m.group(1) for m in re.finditer(r"^\s*include\s+(\w+)", text, re.M) if m.group(1) in ("CModule", "AAsmModule", "CApp", "CLibrary")]
    if "CModule" in kinds:
        return "CModule"
    return kinds[0] if kinds else None


def read(p):
    return open(p, encoding="latin-1").read()


def calls_in(code):
    found = {}
    for m in re.finditer(r"(?<![\w.>])([A-Za-z_]\w*)\s*\(", code):
        # not a member: the character before an identifier is checked by the look-behind (. and ->); a declaration of the function by the module itself is not a call of the library's
        name = m.group(1)
        if name in CATEGORY:
            found[name] = found.get(name, 0) + 1
    return found


def scan_module(d):
    mk = read(os.path.join(d, "Makefile"))
    vars_ = makefile_vars(mk)
    objs = vars_.get("OBJS", "").split()
    cdir = os.path.join(d, "c")
    files = []
    if os.path.isdir(cdir):
        for o in objs:
            p = os.path.join(cdir, o)
            if os.path.isfile(p):
                files.append(p)
        if not files:
            files = [os.path.join(cdir, f) for f in sorted(os.listdir(cdir)) if os.path.isfile(os.path.join(cdir, f))]
    funcs = {}
    lines = 0
    for p in files:
        t = read(p); lines += t.count("\n")
        for k, v in calls_in(strip_c(t)).items():
            funcs[k] = funcs.get(k, 0) + v
    # CMHG
    cmhg = {"directives": set(), "options": set()}
    hdir = os.path.join(d, "cmhg")
    if os.path.isdir(hdir):
        for f in sorted(os.listdir(hdir)):
            p = os.path.join(hdir, f)
            if not os.path.isfile(p):
                continue
            txt = "\n".join(l for l in read(p).split("\n") if not l.lstrip().startswith(";") and not l.lstrip().startswith("#"))
            for m in re.finditer(r"^([A-Za-z][\w-]*)\s*:", txt, re.M):
                if m.group(1).lower() in CMHG_DIRECTIVES:
                    cmhg["directives"].add(m.group(1).lower())
            body = "\n".join(l for l in txt.split("\n") if l[:1] in " \t")
            for k in CMD_OPTS:
                if re.search(r"(?<![\w-])%s\s*:" % re.escape(k), body):
                    cmhg["options"].add(k)
    return dict(path=d, lines=lines, cfiles=[os.path.basename(f) for f in files], funcs=funcs, cmhg_directives=sorted(cmhg["directives"]), cmhg_options=sorted(cmhg["options"]))


def have_from_lib(lib, nm):
    have = set()
    if lib and os.path.exists(lib):
        out = subprocess.run([nm, "-g", "--defined-only", lib], capture_output=True, text=True).stdout
        for l in out.split("\n"):
            p = l.split()
            if len(p) == 3 and p[1] in "TDBRW":
                have.add(p[2])
    return have


def inline_from_headers(inc):
    have = set()
    if os.path.isdir(inc):
        for f in os.listdir(inc):
            if f.endswith(".h"):
                for m in re.finditer(r"static\s+inline\s+[\w\s\*]+?\b(\w+)\s*\(", read(os.path.join(inc, f))):
                    have.add(m.group(1))
    return have


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--sources", default=os.environ.get("RISCOS_SOURCES"))
    ap.add_argument("--lib", default=os.path.join(HOME, "gccsdk-next/env-f/arm-riscos-gnueabihf/lib/libmodkit-core.a"))
    ap.add_argument("--nm", default=os.path.join(HOME, "gccsdk-next/env-f/bin/arm-riscos-gnueabihf-nm"))
    ap.add_argument("--include", default=os.path.join(HOME, "gccsdk-next/modkit/include"), help="modkit's headers (static inline functions count as present)")
    ap.add_argument("--json"); ap.add_argument("--detail", action="store_true"); ap.add_argument("--missing", action="store_true")
    ap.add_argument("--scenario", action="append", default=[], help="NAME=FUNC,FUNC,...: today's library plus these functions")
    ap.add_argument("--cmhg-ok", default="", help="more CMHG directives / command options that the generator supports (comma separated)")
    a = ap.parse_args()
    if not a.sources or not os.path.isdir(a.sources): sys.exit("give --sources DIR (a checkout of https://gitlab.riscosopen.org/RiscOS/Sources) or set RISCOS_SOURCES")

    mods = []
    for root, dirs, files in os.walk(a.sources):
        if "Makefile" in files:
            kind = component_kind(read(os.path.join(root, "Makefile")))
            if kind:
                mods.append((kind, root))
    kinds = {}
    for k, _ in mods:
        kinds[k] = kinds.get(k, 0) + 1
    print("components by build rule: " + ", ".join("%d %s" % (v, k) for k, v in sorted(kinds.items())))
    cmods = [scan_module(d) for k, d in sorted(mods, key=lambda x: x[1]) if k == "CModule"]
    for m in cmods:
        m["name"] = os.path.relpath(m["path"], a.sources)

    have = have_from_lib(a.lib, a.nm) | inline_from_headers(a.include)
    present = {f for f in CATEGORY if f in have}
    extra_ok = set(x for x in a.cmhg_ok.split(",") if x)
    d_ok = CMHG_OK | {x for x in extra_ok if x in CMHG_DIRECTIVES}
    o_ok = (CMHG_OPT_OK & set(CMD_OPTS)) | {x for x in extra_ok if x in CMD_OPTS}

    def libc_missing(m, add):
        return sorted(f for f in m["funcs"] if f not in present and f not in add)

    def cmhg_missing(m):
        return sorted(set(m["cmhg_directives"]) - d_ok) + sorted("%s:" % o for o in set(m["cmhg_options"]) - o_ok)

    n = len(cmods)
    print("C modules: %d (%d lines of C); using any library function: %d" % (n, sum(m["lines"] for m in cmods), sum(1 for m in cmods if m["funcs"])))
    print("library functions present today (of the %d standard names counted): %s" % (len(CATEGORY), " ".join(sorted(present))))
    scen = [("today", set())]
    for s in a.scenario:
        name, _, fs = s.partition("=")
        scen.append((name, set(fs.split(","))))
    for name, add in scen:
        ok = [m for m in cmods if not libc_missing(m, add)]
        both = [m for m in ok if not cmhg_missing(m)]
        print("  %-24s library calls covered: %2d of %d (%d%%)   and CMHG covered too: %2d" % (name, len(ok), n, round(100.0 * len(ok) / n), len(both)))
    cm = [m for m in cmods if not cmhg_missing(m)]
    print("CMHG constructs covered: %d of %d modules" % (len(cm), n))
    usage = {}
    for m in cmods:
        for f in libc_missing(m, set()):
            usage.setdefault(f, []).append(m["name"])
    if a.missing:
        print("\nmissing library functions (modules that use each):")
        for f, ms in sorted(usage.items(), key=lambda x: (-len(x[1]), x[0])):
            print("  %-12s %-7s %2d" % (f, CATEGORY[f], len(ms)))
    dir_use = {}
    for m in cmods:
        for x in cmhg_missing(m):
            dir_use.setdefault(x, []).append(m["name"])
    print("\nCMHG constructs not supported (modules that use each):")
    for x, ms in sorted(dir_use.items(), key=lambda x: (-len(x[1]), x[0])):
        print("  %-28s %2d" % (x, len(ms)))
    if a.detail:
        print()
        for m in cmods:
            print("%-48s %6d lines  libc missing: %-60s cmhg missing: %s" % (m["name"], m["lines"], " ".join(libc_missing(m, set())) or "-", " ".join(cmhg_missing(m)) or "-"))
    if a.json:
        json.dump(dict(modules=cmods, present=sorted(present)), open(a.json, "w"), indent=1)


main()
