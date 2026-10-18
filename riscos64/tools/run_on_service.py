#!/usr/bin/env python3
"""run_on_service.py OUTDIR [--arch aarch64] [--only NAME...]: run the 64-bit tests in OUTDIR (made by build.sh) on the RISC OS Build service (https://build.riscos.online/, RISC OS Pyromaniac)
and compare the output with the expected output.  One request per test (a job stops at the first command that returns a non-zero code).  The service is Gerph's, who has said publicly that it is open to everyone without limits.
Writes OUTDIR/service-results/NAME.json and NAME.txt, and prints a summary.  Every job is a zip (the binaries and a .robuild.yaml) sent through the WebSocket interface of the service (tools/ws_build.py)."""
import base64, io, json, os, re, sys, time, urllib.request, zipfile
out = sys.argv[1]; arch = "aarch64"; only = []
a = sys.argv[2:]
while a:
    x = a.pop(0)
    if x == "--arch": arch = a.pop(0)
    elif x == "--only": only = list(a); a = []
    else: only.append(x)

def obey_lines(name):
    """the commands of an Obey file of a test pack, for a dry run on the service (it has no *Obey): <T64$Dir>. is dropped, the current directory is the folder of the job"""
    out = []
    for l in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "obey", name + ",feb"), encoding="latin-1").read().split("\n"):
        l = l.strip()
        if not l or l.startswith("|") or l.startswith("Spool") or l.startswith("Set T64$Dir") or l.startswith("Dir <T64$Dir>"): continue
        out.append(l.replace("<T64$Dir>.", ""))
    return out
PACK08 = ["Cxx64,ffa", "Dec64,ffa", "Fpmod64,ffa", "Hello64,ffa", "Libmod64,ffa", "Svc64,ffa", "Swi64,ffa", "cxx64test,ff8", "cxxtest,ff8", "dec64test,ff8", "fpmod64test,ff8", "hello,ff8", "hello64test,ff8", "libmod64test,ff8", "libtest64,ff8", "sdktest,ff8",
          "svc64test,ff8", "swi64test,ff8", "t_files,ff8", "t_posix,ff8", "t_proc,ff8", "t_riscosify,ff8", "testriscosify,ff8", "testunixify,ff8", "tst_seekdir,ff8"]
JOBS = [("t01hello", ["/t01hello"]), ("t02swi", ["/t02swi"]), ("t03int", ["/t03int"]), ("t04fp", ["/t04fp"]), ("t05file", ["/t05file"]),
        ("mod64", ["RMLoad Mod64", "Mod64_Hello", "Help Mod64", "/t07mod", "RMKill Mod64"]),
        ("mod64d", ["RMLoad Mod64D", "RMLoad Mod64S", "Help Mod64D", "/t08data", "RMKill Mod64S", "RMKill Mod64D"]), ("mod64f", ["RMLoad Mod64F", "/t09fp", "RMKill Mod64F"]),
        ("mod64r", ["RMLoad Mod64D", "/t08data prep", "RMReInit Mod64D", "/t08data reinit", "RMKill Mod64D"]),
        ("hello", ["/hello one \"two three\" four"]), ("libc", ["/libctest"]),
        ("lt64", ["/libtest64 " + os.environ.get("LT64_ARGS", "-only=ctype")]),
        ("hello64", ["RMLoad Hello64", "Help Hello64", "/hello64test", "RMKill Hello64"]),
        ("libtestmod", ["Set LT_ONLY " + os.environ.get("LT64_ONLY", "ctype,string,printf"), "RMLoad LibTest64", "LibTest64", "RMKill LibTest64"]),
        ("dec64", ["RMLoad Dec64", "/dec64test", "RMKill Dec64"]),
        ("cxx64", ["RMLoad Cxx64", "/cxx64test", "RMKill Cxx64"]),
        ("fpmod64", ["RMLoad Fpmod64", "/fpmod64test", "RMKill Fpmod64"]),
        ("libmod64", ["RMLoad Libmod64", "/libmod64test", "RMKill Libmod64"]),
        ("swi64", ["RMLoad Swi64", "/swi64test", "RMKill Swi64"]),
        ("pack08kit", obey_lines("Run64Kit")), ("pack08unix", obey_lines("Run64Unix")), ("pack08prog", obey_lines("Run64Prog")),
        ("echotest", [l for l in obey_lines("Run64Kit") if l.startswith("Echo") or l.startswith("Spool")]),
        ("spooltest", ["Spool ResultsX", "Echo hi there", "RMLoad Hello64", "Run hello64test", "RMKill Hello64", "Spool", "Type ResultsX"]),
        ("svc64", ["RMLoad Svc64", "/svc64test", "RMKill Svc64"]), ("slot64", ["RMLoad Slot64", "/slot64test", "RMKill Slot64"]),
        ("oslib64", ["Set OlT$Var hello there", "RMLoad OlTest", "OlTest", "RMKill OlTest"]),
        ("sdktest", ["/sdktest"]),
        ("zlib", ["/zlibtest", "/example foogz"]),
        ("cxxtest", ["/cxxtest"]),
        ("ul_riscosify", ["/t_riscosify"]), ("ul_files", ["/t_files"]), ("ul_posix", ["/t_posix"]), ("ul_seekdir", ["/tst_seekdir"]), ("ul_proc", ["/t_proc"]), ("ul_term", ["/t_term"]), ("ul_run", ["Set UnixEnv$swap 1", "/t_run"]), ("ul_vfork", ["Set UnixEnv$swap 1", "/t_vfork"]), ("ul_misc", ["/t_misc"]), ("ul_glob", ["/t_glob"]), ("ul_signal", ["/t_signal"]), ("ul_wide", ["/t_wide"]), ("cx_atomic", ["/atomic"]), ("cx_chrono", ["/chrono"]), ("cx_containers", ["/containers"]), ("cx_cxx17", ["/cxx17"]), ("cx_cxx20", ["/cxx20"]), ("cx_exceptions", ["/exceptions"]), ("cx_format", ["/format"]), ("cx_fs", ["/fs"]), ("cx_fstream", ["/fstream"]), ("cx_locale", ["/locale"]), ("cx_random", ["/random"]), ("cx_regex", ["/regex"]), ("cx_rtti", ["/rtti"]), ("cx_wide", ["/wide"]), ("cx_mutex", ["/mutex"]), ("cx_once2", ["/once2"]), ("cx_once3", ["/once3"]), ("cx_once4", ["/once4"]), ("cx_threads3", ["/threads3"]), ("cx_thread", ["/thread"]), ("cx_future", ["/future"]), ("cx_threads2", ["/threads2"]), ("ul_tls", ["/t_tls"]), ("ul_compat", ["/t_compat"]), ("ul_dbg", ["/t_dbg"]), ("t_vfs", ["/t_vfs"]), ("port_sqlite", ["/sqlite3 --version", "/sqlite3 sqt.db \"create table t(a integer primary key, b text); insert into t(b) values('one'),('two'),('three'); select * from t;\"", "/sqlite3 sqt.db \"select count(*), sum(a) from t; select b from t order by b desc;\"", "/sqlite3 sqt.db \"create index ib on t(b); with recursive c(x) as (select 1 union all select x+1 from c where x<20000) insert into t(b) select 'row'||x from c; select count(*), sum(length(b)) from t; pragma integrity_check; begin; delete from t where a>100; rollback; select count(*) from t;\"", "/sqlite3 :memory: \"select sqlite_version(), typeof(1.5), printf('%.3f', 3.14159), round(2.5), abs(-7), upper('abc'), 7/2, 7.0/2, strftime('%Y', '2026-10-18');\"", "Info sqt/db"]), ("port_bz2", ["Set UnixEnv$swap 1", "/t_bz2", "/bzip2 -k -f sample", "Info sample/bz2", "/bzip2 -t sample.bz2", "/bzip2 -d -f sample.bz2", "/bzip2 -k sample", "/bzip2 -t sample.bz2"]), ("t_spawn", ["Set UnixEnv$swap 1", "/t_spawn"]), ("cli_redir", ["/hello a b { > redirout }", "Type redirout", "Echo hi { > redir2 }", "Type redir2"]), ("t_mtime", ["/t_mtime"]), ("port_make", ["Set UnixEnv$swap 1", "/make --version", "/make -n all", "/make all", "/make show", "/make -j2 all", "/make -f Makefile -p -q show"]), ("t_loc", ["/t_loc"]), ("t_tmp", ["/t_tmp"]), ("lua_t", ["Set UnixEnv$luax$sfix lua", "/luax t.lua", "/luax -e \"_port=true\" bitwise.lua", "/luax -e \"_port=true\" calls.lua", "/luax -e \"_port=true\" closure.lua", "/luax -e \"_port=true\" constructs.lua", "/luax -e \"_port=true\" coroutine.lua", "/luax -e \"_port=true\" goto.lua", "/luax -e \"_port=true\" literals.lua", "/luax -e \"_port=true\" locals.lua", "/luax -e \"_port=true\" math.lua", "/luax -e \"_port=true\" nextvar.lua", "/luax -e \"_port=true\" pm.lua", "/luax -e \"_port=true\" sort.lua", "/luax -e \"_port=true\" strings.lua", "/luax -e \"_port=true\" tpack.lua", "/luax -e \"_port=true\" utf8.lua", "/luax -e \"_port=true\" vararg.lua", "/luax -e \"_port=true\" events.lua", "/luax -e \"_port=true\" errors.lua"]), ("ul_sock", ["/t_sock"]), ("n_probe", ["/n_probe"]), ("net_mods", ["Modules", "Help Internet", "Help Resolver", "Help EtherK", "Help EtherCard"]), ("ul_segv", ["/t_segv", "/t_segv return"]), ("h_probe", ["/h_probe 3", "/h_probe 1", "/h_probe 2", "/h_probe 6", "/h_probe 99"]), ("f_null", ["/f_null"]), ("f_read", ["/f_read"]), ("f_udf", ["/f_udf"]), ("f_jump", ["/f_jump"]), ("f_recurse", ["/f_recurse"]),  ("lt_stdio2", ["RMLoad LibTest64", "Set LT_ONLY stdio2", "Set LT_TRACE 1", "LibTest64", "RMKill LibTest64"]), ("ul_popen", ["Set UnixEnv$swap 1", "/t_popen"]), ("ul_thrun", ["Set UnixEnv$swap 1", "/t_thrun"]), ("ul_thread", ["/t_thread"]), ("ul_wio", ["/t_wio"]), ("ul_dup", ["/t_dup"]), ("nat_cc1", ["WimpSlot -min 120M -max 120M", "/cc1 -quiet -O1 -o o/hello c/hello"]), ("nat_slot", ["WimpSlot", "/exp_env"]), ("nat_ld", ["/ld-new --version"]), ("nat_xgcc", ["/xgcc -dumpmachine", "/xgcc -print-search-dirs", "/xgcc -### -B bin/ -x assembler -c s/hello -o o/hello"]), ("nat_x1", ["/xgcc -v"]), ("nat_x2", ["/xgcc -B bin/ -dumpmachine"]), ("nat_x3", ["/xgcc -B bin -dumpmachine"]), ("nat_x4", ["/xgcc -Bbin/ -dumpmachine"]), ("nat_x5", ["/xgcc -B/work/bin/ -dumpmachine"]), ("nat_y1", ["/xgcc -Ldir -dumpmachine"]), ("nat_y2", ["/xgcc -isystem dir -dumpmachine"]), ("nat_y3", ["/xgcc -print-prog-name=as"]), ("nat_y4", ["/xgcc -B bin/ -print-search-dirs"]), ("nat_y5", ["/xgcc -B/ -dumpmachine"]), ("nat_z1", ["/xgcc -dumpmachine"]), ("nat_z2", ["/xgcc -pipe -dumpmachine"]), ("nat_z3", ["/xgcc -x assembler -dumpmachine"]), ("nat_z4", ["/xgcc -o out/z -dumpmachine"]), ("nat_xgcc4", ["Set UnixEnv$swap 1", "/xgcc -B bin/ -v -x assembler -c s/hello -o o/hello", "Cat o", "/readelf -h o/hello"]), ("nat_drv", ["Set UnixEnv$swap 1", "Set UL64_TraceNosys 1", "/xgcc -B bin/ -x assembler -c s/hello -o o/hello", "/xgcc -B bin/ -B lib/ -L lib -v -o out/hello_elf o/hello", "Info out.hello_elf", "SetType out.hello_elf &FF8", "Dir out", "/hello_elf"]), ("nat_dry", ["Set UnixEnv$swap 1", "/xgcc -### -B bin/ -B lib/ -L lib -isystem include -O1 -x c -c c/hello -o o/hello", "/xgcc -### -save-temps -B bin/ -B lib/ -L lib -isystem include -O1 -x c -o out/hello c/hello"]), ("pack09native", obey_lines("Run64Native")), ("pk_kit", obey_lines("Run64Kit")), ("pk_prog", obey_lines("Run64Prog")), ("pk_unix", obey_lines("Run64Unix")), ("pk_proc", obey_lines("Run64Proc")), ("pack09proc", obey_lines("Run64Proc")), ("ptc", ["Set UnixEnv$swap 1", "Set UL64_TraceNosys 1", "xgcc -B bin/ -B lib/ -L lib -isystem include -save-temps -O1 -x c -o out/hello_c c/hello", "Echo after"]), ("pt1", ["Set UnixEnv$swap 1", "xgcc -B bin/ -x assembler -c s/hello -o o/t1"]), ("pt2", ["Set UnixEnv$swap 1", "xgcc -B bin/ -x assembler -c s/hello -o o/t1", "Set"]),  ("nat_xgcc2", ["/xgcc -B bin/ -x assembler -c s/hello -o o/hello"]), ("nat_xgcc3", ["/xgcc -B bin/ -v"]), ("nat_link", ["/as-new -o o/hello s/hello", "/ld-new -maarch64riscos -static -X --no-warn-rwx-segments -T lib/aif64.ld -o out/hello_elf lib/aifhdr.o lib/crt0.o lib/riscos_start.o lib/riscos_syscalls.o lib/swi.o lib/crti.o lib/crtbegin.o o/hello lib/libgcc.a --start-group lib/libriscosstub.a lib/libc.a lib/libm.a lib/libgcc.a --end-group lib/libgcc.a lib/crtend.o lib/crtn.o", "Info out.hello_elf", "/objcopy -O binary out/hello_elf out/hello,ff8", "Info out.hello,ff8", "SetType out.hello,ff8 &FF8", "Rename out.hello,ff8 out.hello", "Dir out", "/hello"]),  ("ul_sig2", ["/t_sig2"]), ("ul_dir", ["/t_dir"]), ("nat_as", ["/as-new --version", "/as-new -o o/test s/test", "/readelf -h o/test", "/objdump -d o/test", "/ar --version"]), ("exp_env", ["/exp_env"]), ("exp_da", ["/exp_da"]), ("exp_exit", ["/exp_exit /t_child", "/exp_exit -d"]), ("ul_oracle", ["Set UnixEnv$testriscosify$sfix c:o:h:cc:s:cpp", "Set UnixFS$/xxx xxx", "/testriscosify", "/testunixify"]),
        ("svcprobe", ["RMLoad Mod64P", "/t10svc", "RMKill Mod64P"]),
        ("fsdiff", ["/fsdiff"]), ("fsdiff3", ["/fsdiff args3"])]
for _s in [x for x in os.environ.get("LT64_SECTIONS", "").split(",") if x]:         # LT64_SECTIONS=a,b,c: one job (and result file lt64-NAME) for each section of the library test
    JOBS.append(("lt64-" + _s, ["/libtest64 -only=" + _s]))
TOPFILES = sorted(f for f in os.listdir(out) if os.path.isfile(os.path.join(out, f)) and f != "README.txt" and not f.startswith("Run64")) if os.path.isdir(out) else []
NATIVETREE = []
if os.path.exists(os.path.join(out, "xgcc,ff8")):			# OUTDIR is a tree made by ports/make-native-tree.py
    for _r, _d, _fs in os.walk(out):
        if "service-results" in _r: continue
        for _f in _fs: NATIVETREE.append(os.path.relpath(os.path.join(_r, _f), out))
FILES = {"t01hello": ["t01hello,ff8"], "t02swi": ["t02swi,ff8"], "t03int": ["t03int,ff8"], "t04fp": ["t04fp,ff8"], "t05file": ["t05file,ff8"], "mod64": ["Mod64,ffa", "t07mod,ff8"], "mod64d": ["Mod64D,ffa", "Mod64S,ffa", "t08data,ff8"], "mod64f": ["Mod64F,ffa", "t09fp,ff8"], "mod64r": ["Mod64D,ffa", "t08data,ff8"], "hello": ["hello,ff8"], "libc": ["libctest,ff8"], "lt64": ["libtest64,ff8"], "hello64": ["Hello64,ffa", "hello64test,ff8"], "libtestmod": ["LibTest64,ffa"], "dec64": ["Dec64,ffa", "dec64test,ff8"], "cxx64": ["Cxx64,ffa", "cxx64test,ff8"], "fpmod64": ["Fpmod64,ffa", "fpmod64test,ff8"], "libmod64": ["Libmod64,ffa", "libmod64test,ff8"], "swi64": ["Swi64,ffa", "swi64test,ff8"], "svc64": ["Svc64,ffa", "svc64test,ff8"], "slot64": ["Slot64,ffa", "slot64test,ff8"], "spooltest": ["Hello64,ffa", "hello64test,ff8"], "echotest": ["Hello64,ffa"], "pack08kit": PACK08, "pack08unix": PACK08, "pack08prog": PACK08, "oslib64": ["OlTest,ffa"], "sdktest": ["sdktest,ff8"], "zlib": ["example,ff8", "zlibtest,ff8"], "cxxtest": ["cxxtest,ff8"], "svcprobe": ["Mod64P,ffa", "t10svc,ff8"], "ul_riscosify": ["t_riscosify,ff8"], "ul_files": ["t_files,ff8"], "ul_posix": ["t_posix,ff8"], "ul_seekdir": ["tst_seekdir,ff8"], "ul_proc": ["t_proc,ff8"], "ul_term": ["t_term,ff8"], "ul_run": ["t_run,ff8", "t_child,ff8"], "ul_vfork": ["t_vfork,ff8", "t_child,ff8"], "ul_misc": ["t_misc,ff8"], "ul_glob": ["t_glob,ff8"], "ul_signal": ["t_signal,ff8"], "ul_wide": ["t_wide,ff8"], "cx_atomic": ["atomic,ff8"], "cx_chrono": ["chrono,ff8"], "cx_containers": ["containers,ff8"], "cx_cxx17": ["cxx17,ff8"], "cx_cxx20": ["cxx20,ff8"], "cx_exceptions": ["exceptions,ff8"], "cx_format": ["format,ff8"], "cx_fs": ["fs,ff8"], "cx_fstream": ["fstream,ff8"], "cx_locale": ["locale,ff8"], "cx_random": ["random,ff8"], "cx_regex": ["regex,ff8"], "cx_rtti": ["rtti,ff8"], "cx_wide": ["wide,ff8"], "cx_mutex": ["mutex,ff8"], "cx_once2": ["once2,ff8"], "cx_once3": ["once3,ff8"], "cx_once4": ["once4,ff8"], "cx_threads3": ["threads3,ff8"], "cx_thread": ["thread,ff8"], "cx_future": ["future,ff8"], "cx_threads2": ["threads2,ff8"], "ul_tls": ["t_tls,ff8"], "ul_compat": ["t_compat,ff8"], "ul_dbg": ["t_dbg,ff8"], "t_vfs": ["t_vfs,ff8"], "port_sqlite": ["sqlite3,ff8"], "port_bz2": ["t_bz2,ff8", "bzip2,ff8", "sample"], "t_spawn": ["t_spawn,ff8", "hello,ff8"], "cli_redir": ["hello,ff8"], "t_mtime": ["t_mtime,ff8", "Makefile"], "port_make": ["make,ff8", "hello,ff8", "Makefile"], "t_loc": ["t_loc,ff8"], "t_tmp": ["t_tmp,ff8"], "lua_t": ["luax,ff8", "lua/t", "lua/bitwise", "lua/calls", "lua/closure", "lua/constructs", "lua/coroutine", "lua/goto", "lua/literals", "lua/locals", "lua/math", "lua/nextvar", "lua/pm", "lua/sort", "lua/strings", "lua/tpack", "lua/utf8", "lua/vararg", "lua/events", "lua/errors"], "ul_sock": ["t_sock,ff8"], "n_probe": ["n_probe,ff8"], "net_mods": ["t_dbg,ff8"], "ul_segv": ["t_segv,ff8"], "h_probe": ["h_probe,ff8"], "f_null": ["f_null,ff8"], "f_read": ["f_read,ff8"], "f_udf": ["f_udf,ff8"], "f_jump": ["f_jump,ff8"], "f_recurse": ["f_recurse,ff8"],  "lt_stdio2": ["LibTest64,ffa"], "ul_popen": ["t_popen,ff8", "t_child,ff8"], "ul_thrun": ["t_thrun,ff8", "t_child,ff8"], "ul_thread": ["t_thread,ff8"], "ul_wio": ["t_wio,ff8"], "ul_dup": ["t_dup,ff8"], "nat_cc1": ["cc1,ff8", "c/hello", "o/keep"], "nat_slot": ["exp_env,ff8"], "nat_ld": ["ld-new,ff8"], "nat_xgcc": ["xgcc,ff8", "bin/as,ff8", "s/hello", "o/keep"], "nat_x1": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_x2": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_x3": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_x4": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_x5": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_y1": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_y2": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_y3": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_y4": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_y5": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_z1": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_z2": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_z3": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_z4": ["xgcc,ff8", "bin/as,ff8", "o/keep"], "nat_xgcc4": ["xgcc,ff8", "bin/as,ff8", "readelf,ff8", "s/hello", "o/keep"], "nat_drv": ["xgcc,ff8", "bin/as,ff8", "bin/ld,ff8", "bin/objcopy,ff8", "bin/collect2,ff8", "s/hello", "o/keep", "out/keep", "lib/aif64.ld", "lib/libgcc.a", "lib/libc.a", "lib/libm.a", "lib/libriscosstub.a"] + ["lib/o/" + n for n in ("aifhdr", "crt0", "riscos_start", "riscos_syscalls", "swi", "crti", "crtbegin", "crtend", "crtn")], "nat_dry": ["xgcc,ff8", "bin/as,ff8", "c/hello", "o/keep", "out/keep"], "pack09native": NATIVETREE, "pk_kit": TOPFILES, "pk_prog": TOPFILES, "pk_unix": TOPFILES, "pk_proc": TOPFILES, "pack09proc": ["exp_env,ff8", "exp_da,ff8", "exp_exit,ff8", "t_child,ff8", "t_term,ff8", "t_run,ff8", "t_vfork,ff8", "t_signal,ff8"], "ptc": NATIVETREE, "pt1": NATIVETREE, "pt2": NATIVETREE, "nat_xgcc2": ["xgcc,ff8", "bin/as,ff8", "s/hello", "o/keep"], "nat_xgcc3": ["xgcc,ff8", "bin/as,ff8", "s/hello", "o/keep"], "nat_link": ["as-new,ff8", "ld-new,ff8", "objcopy,ff8", "s/hello", "o/keep", "out/keep", "lib/aif64.ld", "lib/libgcc.a", "lib/libc.a", "lib/libm.a", "lib/libriscosstub.a"] + ["lib/o/" + n for n in ("aifhdr", "crt0", "riscos_start", "riscos_syscalls", "swi", "crti", "crtbegin", "crtend", "crtn")], "ul_sig2": ["t_sig2,ff8"], "ul_dir": ["t_dir,ff8"], "nat_as": ["as-new,ff8", "readelf,ff8", "objdump,ff8", "ar,ff8", "s/test", "o/keep"], "exp_env": ["exp_env,ff8"], "exp_da": ["exp_da,ff8"], "exp_exit": ["exp_exit,ff8", "t_child,ff8"], "ul_oracle": ["testriscosify,ff8", "testunixify,ff8"], "fsdiff": ["fsdiff,ff8"], "fsdiff3": ["fsdiff,ff8"]}

# the Lua test suite (ports/): one job per test file; the files are lua/NAME (the sfix of the program "luax" is lua, so NAME.lua is lua/NAME)
LUA_TESTS = "bitwise calls closure constructs coroutine goto literals locals math nextvar pm sort strings tpack utf8 vararg events errors api attrib code cstack db files gc gengc big heavy verybig main".split()
for _t in LUA_TESTS:
    JOBS.append(("lua_" + _t, ['Set UnixEnv$luax$sfix lua', '/luax -e "_port=true;_soft=true" %s.lua' % _t]))
    FILES["lua_" + _t] = ["luax,ff8", "lua/" + _t, "lua/bwcoercion", "lua/tracegc"]
for _n, _c in JOBS:
    if _n.startswith("lt64-"): FILES[_n] = ["libtest64,ff8"]
os.makedirs(out + "/service-results", exist_ok=True)
def yaml(cmds):
    y = "%YAML 1.0\n---\njobs:\n  build:\n    env:\n      \"Sys$Environment\": ROBuild\n    script:\n"
    for c in cmds: y += "      - %s\n      - %s\n" % (json.dumps("echo *** " + c.replace('"', "'")), json.dumps(c))          # (JSON strings are YAML strings: a command with ": " in it would be a mapping)
    y += "      - echo *** Done\n"
    return y
def post(zipbytes, timeout=int(os.environ.get("SERVICE_TIMEOUT", "60"))):
    """one job through the WebSocket interface of tools/ws_build.py: returns {"rc": N, "output": [text], "messages": []}"""
    import tempfile, subprocess
    with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as f: f.write(zipbytes); path = f.name
    try:
        r = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "ws_build.py"), path, "--arch", arch, "--timeout", str(timeout)],
                           capture_output=True, text=True, timeout=timeout + 120)
    finally: os.unlink(path)
    rc = 255
    lines = []
    for l in r.stdout.split("\n"):
        if l.startswith("[rc] "): rc = int(l[5:])
        elif l.startswith("[response]") or l.startswith("[message]") or l.startswith("[error]"): continue
        else: lines.append(l + "\n")
    return {"rc": rc, "output": lines, "messages": []}
ansi = re.compile(r"\x1b\[[0-9;]*m")
summary = []
for name, cmds in JOBS:
    if only and name not in only: continue
    z = io.BytesIO()
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(".robuild.yaml", yaml(cmds))
        for f in FILES[name]: zf.write(os.path.join(out, f), f)
    try: res = post(z.getvalue())
    except Exception as e:
        res = {"rc": -1, "output": ["request failed: %s" % e], "messages": []}
    lines = [ansi.sub("", l).replace("\r\n", "\n").replace("\r", "") for l in res.get("output", [])]
    text = "".join(lines)
    json.dump(res, open("%s/service-results/%s.json" % (out, name), "w"), indent=1)
    open("%s/service-results/%s.txt" % (out, name), "w").write(text)
    passed = sum(1 for l in text.split("\n") if l.startswith("PASS ")); failed = sum(1 for l in text.split("\n") if l.startswith("FAIL "))
    summ = [l for l in text.split("\n") if l.startswith("SUMMARY")]
    exp = "%s/expected/%s.txt" % (out, name)
    if os.path.exists(exp):
        got = [l for l in text.split("\n") if l and not l.startswith("***")]
        want = [l for l in open(exp).read().split("\n") if l]
        same = got == want
        print("   output %s the host's expected output (%d lines)" % ("IDENTICAL to" if same else "DIFFERS from", len(want)))
        if not same:
            import difflib
            for l in list(difflib.unified_diff(want, got, "expected", "service", lineterm=""))[:12]: print("   " + l)
            failed += 1
    summary.append((name, res.get("rc"), passed, failed, summ[0] if summ else "(no SUMMARY line)"))
    print("== %s: rc %s, %d PASS, %d FAIL  %s" % (name, res.get("rc"), passed, failed, summ[0] if summ else "(no SUMMARY line)"))
    time.sleep(2)
print()
for n, rc, p, f, s in summary: print("%-9s rc=%s PASS=%d FAIL=%d" % (n, rc, p, f))
