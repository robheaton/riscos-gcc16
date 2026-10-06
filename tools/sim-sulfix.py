#!/usr/bin/env python3
"""sim-sulfix.py -- run the Obey files Install, Restore and Check of the SharedULibFix package (recipe/gcc-16.2.0-riscos/sulfix) on a small model of the RISC OS command line and of the layout
of the author's machine, in a scratch directory, and check the LOGIC of the install: what is changed and in which order, what stops it, what is put back.

The program sulfile is the REAL source (sulfile.c, main () included) built for the host against a model of the RISC OS calls it makes (sulfix/host-main.c); fixlevel, modver, vforkbare and vforkfail are
modelled here.  The model cannot know how RISC OS itself behaves: where the Obey files or sulfile depend on a behaviour that this had to assume it says so (ASSUMED in host-main.c and below).
THE LAYOUT (seen in the output of the first install on the machine, 2026-10-04): System$Path = Sys:500.,Sys:400.,Sys:370.,Sys:360.,Sys:350.,Sys:310.,NVMe::NVMe.$.!Boot.Resources.!System. ; the module is the
file Sys:310.Modules.SharedULib ; the folders 500 ... 350 do not exist ; a file read as System:Modules.X is searched in that order.
The model is deliberately PESSIMISTIC about  Sys$ReturnCode : it is reset to 0 by every command that is not a Run (a false If does not touch it: hardware evidence), so an Obey file that tests it later fails
here.  It knows that  Run  passes its arguments on UNEXPANDED (the first run of the old installer printed the literal  <Sul$Dir>.SharedULib-backup-stock) and that the FILE SYSTEM expands <Var> in a name.
usage: sim-sulfix.py SULFIX_DIR SUL_BUILD_DIR SULFILE_MODEL_BINARY      (exit status 0 = every scenario behaves as intended)
  SULFIX_DIR = recipe/gcc-16.2.0-riscos/sulfix      SUL_BUILD_DIR = the output of build-sul.sh      SULFILE_MODEL_BINARY = the program that  gcc -Ihost-stubs host-main.c  makes"""
import os, re, shutil, subprocess, sys, tempfile

BOOT = "NVMe::NVMe.$.!Boot.Resources.!System."
APP = "ADFS::HardDisc4.$.Apps.Utilities.!SULFix."
SYSPATH = "Sys:500.,Sys:400.,Sys:370.,Sys:360.,Sys:350.,Sys:310.,NVMe::NVMe.$.!Boot.Resources.!System."
M310 = BOOT + "310.Modules.SharedULib"                      # the module file of the author's machine, by its full name


class ObeyError(Exception):
    pass


def fnv(b):
    h = 0x811c9dc5
    for c in b:
        h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h


class Machine:
    def __init__(self, scripts, sul_build, model, root, loaded=None, hooks=None, fixlevel=15, missing_dir_err=False):
        self.scripts, self.sul_build, self.model, self.root = scripts, sul_build, model, root
        for d in ("boot/!System", "app"):
            os.makedirs(os.path.join(root, d), exist_ok=True)
        self.vars = {"Sys$ReturnCode": "0", "System$Path": SYSPATH}
        self.out, self.log, self.runs = [], [], []
        self.loaded, self.hooks, self.fixlevel, self.missing_dir_err = loaded, hooks or {}, fixlevel, missing_dir_err
        rd = lambda n: open(os.path.join(sul_build, n), "rb").read()
        self.known = {fnv(rd("sul-ref.bin")): "stock", fnv(rd("SharedULib-116fix3,ffa")): "fix3", fnv(rd("SharedULib-116fix3t,ffa")): "fix3t"}
        # the folder of the package: the files of the package (Install, Restore, Check as the Obey files; the module as SharedULib)
        shutil.copy(os.path.join(sul_build, "SharedULib-116fix3,ffa"), os.path.join(root, "app", "SharedULib"))

    # ---- file names: the file system expands <Var>; System: is searched for reading
    def gstrans(self, s):
        return re.sub(r"<([^<>]+)>", lambda m: self.vars.get(m.group(1), ""), s)

    def host(self, name, write=False):
        n = self.gstrans(name)
        if n.startswith("Sys:"):
            n = BOOT + n[4:]
        if n.startswith(BOOT):
            return os.path.join(self.root, "boot", "!System", n[len(BOOT):].replace(".", "/"))
        if n.startswith(APP):
            return os.path.join(self.root, "app", n[len(APP):].replace(".", "/"))
        if n.startswith("System:"):
            if write:
                raise ObeyError("Not found")                  # a NEW file written as System:... goes into the first directory of the path, which does not exist
            for el in self.vars["System$Path"].split(","):
                p = self.host(el + n[7:])
                if os.path.exists(p):
                    return p
            return os.path.join(self.root, "absent", n[7:].replace(".", "/"))
        raise ObeyError("File '%s' not found (the model knows only Sys:, System:, %s and %s)" % (name, BOOT, APP))

    def say(self, t):
        self.out.append(t)

    # ---- the Obey interpreter
    def run_line(self, line, depth=0):
        line = line.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("|"):
            return
        m = re.match(r"^\s*(\S+)\s*(.*)$", line)
        cmd, rest = m.group(1).lower(), m.group(2)
        if cmd == "if":
            i = rest.lower().find(" then ")
            cond = self.gstrans(rest[:i]) + rest[i:]
            mm = re.match(r'^"(.*?)"\s*(=|<>)\s*"(.*?)"\s+then\s+(.*)$', cond, re.I)
            if not mm:
                raise ObeyError("syntax: " + line)
            a, op, b, then = mm.groups()
            if (a == b) == (op == "="):
                self.run_line(then, depth)
            return                                          # a FALSE If leaves Sys$ReturnCode alone (hardware evidence)
        if cmd == "run":
            return self.cmd_run(rest)
        try:
            self.other(cmd, rest, line, depth)
        finally:
            self.vars["Sys$ReturnCode"] = "0"               # pessimistic model: any command that is not a Run leaves 0 behind

    def other(self, cmd, rest, line, depth):
        if cmd == "set":
            name, _, val = rest.partition(" ")
            self.vars[name] = self.gstrans(val.strip())
        elif cmd == "unset":
            self.vars.pop(rest.strip(), None)
        elif cmd in ("dir", "time", "show"):
            pass
        elif cmd == "echo":
            self.say(self.gstrans(rest))
        elif cmd == "error":
            raise ObeyError(self.gstrans(rest))
        elif cmd == "remove":
            p = self.host(rest.strip())
            if os.path.exists(p):
                os.remove(p)
                self.log.append(("remove", self.gstrans(rest.strip())))
        elif cmd == "info":
            p = self.host(rest.strip())
            if not os.path.exists(p):
                raise ObeyError("File '%s' not found" % rest.strip())
            self.say("%s  FFA  %08X" % (self.gstrans(rest.strip()), os.path.getsize(p)))
        elif cmd == "copy":
            parts = rest.split()
            src, dst, opts = parts[0], parts[1], parts[2:]
            sp = self.host(src)
            if not os.path.exists(sp):
                raise ObeyError("File '%s' not found" % self.gstrans(src))
            dp = self.host(dst, write=True)
            if not os.path.isdir(os.path.dirname(dp)):
                raise ObeyError("Not found")                # a missing folder: "Not found" (after "0 files copied") on the real machine
            data = open(sp, "rb").read()
            if self.hooks.get("copy"):
                data = self.hooks["copy"](self.gstrans(src), self.gstrans(dst), data)
            if os.path.exists(dp) and "~CF" not in opts and "F" not in opts:
                raise ObeyError("Already exists")
            open(dp, "wb").write(data)
            self.log.append(("copy", self.gstrans(src), self.gstrans(dst)))
            if self.hooks.get("after_copy"):
                self.hooks["after_copy"](self, self.gstrans(src), self.gstrans(dst))
        elif cmd == "obey":
            self.obey(self.gstrans(rest.split()[0]), depth + 1)
        else:
            raise ObeyError("the model does not know the command: " + line)

    # ---- the programs
    def installed_kind(self):
        p = self.host("System:Modules.SharedULib")
        return self.known.get(fnv(open(p, "rb").read()), "other") if os.path.exists(p) else None

    def first_unixlib_program(self):
        """the first program built with the UnixLib tool chain after a boot loads what System:Modules.SharedULib finds at that moment; a module that is already loaded stays"""
        if self.loaded is None:
            self.loaded = self.installed_kind()

    def cmd_run(self, rest):
        parts = rest.split()
        prog = self.gstrans(parts[0]).rsplit(".", 1)[-1]
        args = parts[1:]                                    # the arguments are NOT expanded by Run
        self.runs.append(" ".join([prog] + args))
        self.first_unixlib_program()
        if prog == "sulfile":
            vf = os.path.join(self.root, "vars.txt")
            with open(vf, "w") as f:
                for k, v in self.vars.items():
                    f.write("%s=%s\n" % (k, v))
            env = dict(os.environ, SIM_ROOT=self.root, SIM_VARS=vf, SIM_MISSING_DIR_ERR="1" if self.missing_dir_err else "0")
            r = subprocess.run([self.model] + args, capture_output=True, text=True, env=env)
            for l in (r.stdout + r.stderr).splitlines():
                self.say(l)
            if r.returncode >= 4:
                raise ObeyError("sulfile returned %d (an error of the model or a crash)" % r.returncode)
            new = {}
            for l in open(vf).read().splitlines():
                k, _, v = l.partition("=")
                new[k] = v
            self.vars = new
            self.vars["Sys$ReturnCode"] = str(r.returncode)
        elif prog == "fixlevel":
            want = int(args[0]) if args else 10
            self.say("fixlevel: libunixlib fix level %d (%d or more is needed): %s" % (self.fixlevel, want, "OK" if self.fixlevel >= want else "TOO LOW"))
            self.vars["Sys$ReturnCode"] = "0" if self.fixlevel >= want else "1"
        elif prog == "modver":
            has = args[2] if len(args) >= 3 and args[1] == "--has" else None
            word = {"stock": "1.16 (3 Apr 2020)", "fix3": "1.16-vforkfix3 (4 Oct 2026)", "fix3t": "1.16-vforkfix3t (4 Oct 2026)"}.get(self.loaded, "1.16-vforkfixX")
            self.say("modver: SharedUnixLibrary: SharedUnixLibrary %s" % word)
            self.vars["Sys$ReturnCode"] = str(0 if has is None or has in word else 3)
        elif prog in ("vforkbare", "vforkfail"):
            if self.loaded == "stock":
                self.say("Internal error: abort on data transfer")        # with the stock module the parent of a child that ends without exec dies
                raise ObeyError("the program died: Internal error: abort on data transfer")
            self.say("%s: parent resumed" % prog)
            self.vars["Sys$ReturnCode"] = "0"
        else:
            raise ObeyError("the model does not know the program: " + rest)

    def obey(self, name, depth=0):
        base = name.rsplit(".", 1)[-1]
        self.vars["Obey$Dir"] = APP.rstrip(".")
        for l in open(os.path.join(self.scripts, base + ",feb")):
            self.run_line(l, depth)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    scripts, sb, model = sys.argv[1:4]
    rd = lambda p: open(p, "rb").read()
    stock = lambda: rd(os.path.join(sb, "sul-ref.bin"))
    fixed = lambda: rd(os.path.join(sb, "SharedULib-116fix3,ffa"))
    results = []

    def scenario(name, script, setup, expect_ok, check, **kw):
        tmp = tempfile.mkdtemp(prefix="sulfixsim-")
        m = Machine(scripts, sb, model, os.path.join(tmp, "root"), **kw)
        m.vars["SULFix$Dir"] = APP.rstrip(".")
        setup(m)
        err = None
        try:
            m.obey(APP + script)
        except ObeyError as e:
            err = str(e)
        ok = (err is None) == expect_ok and check(m, err)
        results.append(ok)
        print("%-4s %-100s %s" % ("ok" if ok else "FAIL", name, ("stopped: " + err[:60]) if err else "finished"))
        if not ok:
            print("     --- output:")
            [print("     " + l[:160]) for l in m.out[-16:]]
            print("     --- log:", m.log)
        shutil.rmtree(tmp)

    def put(m, rel, data):
        p = os.path.join(m.root, "boot", "!System", rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "wb").write(data)

    def get(m, rel):
        p = os.path.join(m.root, "boot", "!System", rel)
        return open(p, "rb").read() if os.path.exists(p) else None
    app = lambda m, n: open(os.path.join(m.root, "app", n), "rb").read() if os.path.exists(os.path.join(m.root, "app", n)) else None
    copies = lambda m: [c[2] for c in m.log if c[0] == "copy"]
    no_changes = lambda m: not m.log

    # ===== Install on the layout of the author's machine: the module is in Sys:310.Modules
    put_stock = lambda m: put(m, "310/Modules/SharedULib", stock())
    put_fixed = lambda m: put(m, "310/Modules/SharedULib", fixed())

    def installed_ok(m, err):
        return (get(m, "310/Modules/SharedULib") == fixed() and get(m, "310/Modules/SharedULib-stock") == stock() and app(m, "SharedULib-stock") == stock()
                and get(m, "310/Modules/SharedULib-new") is None and copies(m) == [APP + "SharedULib-stock", M310 + "-stock", M310 + "-new", M310]
                and m.installed_kind() == "fix3" and m.vars.get("SULFix$Real") == M310)
    scenario("Install on the machine's layout: fixed module in place, two backups, the copies in the intended order, no temp file", "Install", put_stock, True, installed_ok)
    scenario("Install when 1.16-vforkfix3 is already installed: stops, changes nothing", "Install", put_fixed, False, lambda m, e: "ALREADY installed" in e and no_changes(m))

    def put_other(m):
        b = bytearray(stock()); b[-1] ^= 0x55; put(m, "310/Modules/SharedULib", bytes(b))
    scenario("Install over a module it does not know: stops, changes nothing", "Install", put_other, False, lambda m, e: "not the stock 1.16" in e and no_changes(m))
    scenario("Install with no SharedULib at all: stops, changes nothing", "Install", lambda m: None, False, lambda m, e: no_changes(m) and "SULFix$Real" not in m.vars)
    scenario("Install when the runtime is too old (fix level 9): stops BEFORE anything is copied", "Install", put_stock, False, lambda m, e: "too old" in e and no_changes(m) and get(m, "310/Modules/SharedULib") == stock(), fixlevel=9)
    scenario("Install when the runtime does not answer at all (the stock libunixlib): stops, changes nothing", "Install", put_stock, False, lambda m, e: "too old" in e and no_changes(m), fixlevel=-1)

    # where the file really is: the first directory of System$Path that has it
    def shadow(m):
        put(m, "350/Modules/SharedULib", stock()); put(m, "310/Modules/SharedULib", stock())
    S350 = BOOT + "350.Modules.SharedULib"
    scenario("Install with a stock copy in Sys:350 AND in Sys:310: the one that System: finds (350) is replaced, the other is left alone", "Install", shadow, True,
             lambda m, e: get(m, "350/Modules/SharedULib") == fixed() and get(m, "310/Modules/SharedULib") == stock() and get(m, "350/Modules/SharedULib-stock") == stock()
             and m.installed_kind() == "fix3" and S350 + "-stock" in copies(m))
    put_last = lambda m: put(m, "Modules/SharedULib", stock())
    scenario("Install with the module in the LAST directory of System$Path (no numbered folder)", "Install", put_last, True,
             lambda m, e: get(m, "Modules/SharedULib") == fixed() and get(m, "Modules/SharedULib-stock") == stock() and m.installed_kind() == "fix3")
    scenario("Install when OS_File 5 reports an error for a directory that does not exist (the pessimistic reading)", "Install", put_stock, True, installed_ok, missing_dir_err=True)

    # damaged copies
    corrupt_new = lambda src, dst, data: data[:-5] + b"xxxxx" if dst.endswith("-new") else data
    scenario("Install with a damaged copy of the new module: nothing replaced, the temp file is removed", "Install", put_stock, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and get(m, "310/Modules/SharedULib-new") is None and "NOTHING WAS REPLACED" in e, hooks={"copy": corrupt_new})
    corrupt_final = lambda src, dst, data: data[:-5] + b"yyyyy" if (dst == M310 and src.endswith("-new")) else data
    scenario("Install with a damaged final copy: the stock module is put back in the file that was replaced", "Install", put_stock, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and "stock module was put back" in e and "NOTHING IS INSTALLED" in e, hooks={"copy": corrupt_final})
    corrupt_app_backup = lambda src, dst, data: data[:-3] + b"zzz" if dst == APP + "SharedULib-stock" else data
    scenario("Install with a damaged backup in the package folder: stops before the module is touched", "Install", put_stock, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and "backup in this folder" in e and M310 not in copies(m), hooks={"copy": corrupt_app_backup})
    corrupt_mod_backup = lambda src, dst, data: data[:-3] + b"zzz" if dst == M310 + "-stock" else data
    scenario("Install with a damaged backup next to the module: stops before the module is touched", "Install", put_stock, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and "backup next to the module" in e and M310 not in copies(m), hooks={"copy": corrupt_mod_backup})

    def hide(m, src, dst):                                    # something hides the new module: System: finds another file after the copy
        if dst == M310 and src.endswith("-new"):
            put(m, "350/Modules/SharedULib", stock())
    scenario("Install when System: finds ANOTHER module after the copy: the file that was replaced gets the stock module back, Error", "Install", put_stock, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and "does not find the fixed module" in e and "NOTHING IS INSTALLED" in e and get(m, "350/Modules/SharedULib") == stock(),
             hooks={"after_copy": hide})

    # ===== Restore
    def installed_state(m):
        put_fixed(m); put(m, "310/Modules/SharedULib-stock", stock()); open(os.path.join(m.root, "app", "SharedULib-stock"), "wb").write(stock())
    scenario("Restore after an install: the stock module is back, verified", "Restore", installed_state, True,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and m.installed_kind() == "stock", loaded="fix3")
    scenario("Restore when the backup next to the module is gone: the one in the package folder is used", "Restore",
             lambda m: (put_fixed(m), open(os.path.join(m.root, "app", "SharedULib-stock"), "wb").write(stock())), True,
             lambda m, e: get(m, "310/Modules/SharedULib") == stock() and any(c[0] == "copy" and c[1] == APP + "SharedULib-stock" for c in m.log), loaded="fix3")
    scenario("Restore when the backup in the package folder is gone: the one next to the module is used", "Restore",
             lambda m: (put_fixed(m), put(m, "310/Modules/SharedULib-stock", stock())), True, lambda m, e: get(m, "310/Modules/SharedULib") == stock(), loaded="fix3")
    scenario("Restore without any backup: stops, the installed module is untouched", "Restore", put_fixed, False,
             lambda m, e: get(m, "310/Modules/SharedULib") == fixed() and not m.log and "NOTHING WAS CHANGED" in e, loaded="fix3")

    def bad_backups(m):
        put_fixed(m); b = bytearray(stock()); b[10] ^= 1
        put(m, "310/Modules/SharedULib-stock", bytes(b)); open(os.path.join(m.root, "app", "SharedULib-stock"), "wb").write(bytes(b))
    scenario("Restore when both backups are not the stock module: stops, nothing is copied", "Restore", bad_backups, False, lambda m, e: get(m, "310/Modules/SharedULib") == fixed() and not m.log, loaded="fix3")
    scenario("Restore when the stock module is already installed: nothing to do, nothing copied", "Restore", lambda m: (put_stock(m), put(m, "310/Modules/SharedULib-stock", stock())), False,
             lambda m, e: "already installed" in e and not m.log)
    corrupt_restore = lambda src, dst, data: data[:-4] + b"qqqq" if dst == M310 else data
    scenario("Restore with a damaged copy: the final check stops it with an Error", "Restore", installed_state, False, lambda m, e: "not the stock module after the copy" in e, loaded="fix3", hooks={"copy": corrupt_restore})
    scenario("Restore when the module file is gone altogether: stops (nothing says where it was), nothing copied", "Restore", lambda m: put(m, "310/Modules/Other", b"x"), False,
             lambda m, e: not m.log and "could not be named" in e)

    # ===== Check
    no_vfork = lambda m: not any(r.startswith("vfork") for r in m.runs)
    scenario("Check with the stock file installed: stops before any test", "Check", put_stock, False, lambda m, e: "not the fixed module" in e and no_vfork(m))
    scenario("Check with the runtime too old: stops before any test", "Check", put_fixed, False, lambda m, e: "too old" in e and no_vfork(m), fixlevel=9)
    scenario("Check with the fixed file installed but the STOCK module still loaded: stops before any vfork test", "Check", put_fixed, False, lambda m, e: "LOADED is not 1.16-vforkfix3" in e and no_vfork(m), loaded="stock")
    scenario("Check with the TRACED module loaded: stops before any vfork test", "Check", put_fixed, False, lambda m, e: "TRACED" in e and no_vfork(m), loaded="fix3t")
    scenario("Check with the fixed file installed and loaded on demand: the three vfork tests run, in order", "Check", put_fixed, True,
             lambda m, e: [r for r in m.runs if r.startswith("vfork")] == ["vforkbare", "vforkfail early", "vforkfail missing"])
    scenario("Check with the fixed file installed and already loaded", "Check", put_fixed, True, lambda m, e: no_vfork(m) is False, loaded="fix3")

    print("%d scenarios, %d failed" % (len(results), results.count(False)))
    print("ALL SCENARIOS OK" if all(results) else "SOME SCENARIOS FAILED")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
