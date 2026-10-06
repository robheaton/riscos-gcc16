#!/usr/bin/env python3
"""mkmodhdr.py - CMHG file -> the module header and veneers (GNU assembler) and the C header, for the self-relocating freestanding EABI module model of modkit.

  mkmodhdr.py [-s OUT.s] [-d OUT.h] [--module-name NAME] FILE.cmhg

The same job as CMunge, for modules that are built with GCC 16 (arm-riscos-gnueabihf) WITHOUT SharedCLibrary / UnixLib: the image is linked at address 0, is not position independent, and
relocates itself once, in its initialisation (modkit/bin/modreloc.py appends the table of address words; see modkit/README.md).  What is supported of CMHG:

  title-string:, help-string: (name and version), date-string:        the module's name, help line (title <tab> version (date)) and the Module_* macros of the C header
  initialisation-code: FN          _kernel_oserror *FN (const char *tail, int podule_base, void *pw)
  finalisation-code: FN            _kernel_oserror *FN (int fatal, int podule_base, void *pw)
  service-call-handler: FN [N ...] void FN (int service_number, _kernel_swi_regs *r, void *pw);  with service numbers the kernel only calls it for those (the 'fast' service table, as the RISC OS
                                   modules have it); to claim a call the handler sets r->r[1] = 0; every other register of r is what the service call returns
  command-keyword-table: FN        ENTRIES:  name (min-args: n, max-args: m, gstrans-map: bits, help-text: "...", invalid-syntax: "...")   -  _kernel_oserror *FN (const char *arg_string, int argc,
                                   int number, void *pw) is called with the number of the command (CMD_<name> in the C header); it returns NULL or an error block
  swi-chunk-base-number: N, swi-decoding-table: PREFIX NAME ..., swi-handler-code: FN     _kernel_oserror *FN (int swi_offset, _kernel_swi_regs *r, void *pw)
  irq-handlers:, vector-handlers:, generic-veneers: ENTRY/FN, ...     int FN (_kernel_swi_regs *r, void *pw): the veneer saves every register, switches to SVC mode, calls FN with the register block (r0 - r9, r10,
                                   r11) and the private word and returns to the original mode; FN returns 0 to CLAIM a vector (the veneer returns to the claim address the kernel stacked), non-zero to pass on
                                   (for a callback: always non-zero, the veneer returns with MOV pc, lr)
  module-is-runnable:              ignored with a warning (there is no start code in this model)
Anything else is an error, not silently dropped.  All the RISC OS numbers in the output come from the RISC OS headers (riscos_consts.py)."""
import argparse, os, re, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "modpoc"))
try:
    import riscos_consts as rc
except ImportError:
    rc = None

class CmhgError(Exception): pass

# ---------------------------------------------------------------- the CMHG parser
def logical_lines(text):
    out, cur = [], None
    for raw in text.split("\n"):
        line = raw.rstrip("\r")
        if not line.strip(): continue
        if line.lstrip().startswith(";"): continue
        if line[0] in " \t" and cur is not None: cur[1] += "\n" + line
        else:
            m = re.match(r"^([A-Za-z][\w-]*)\s*:\s*(.*)$", line)
            if not m: raise CmhgError("cannot parse the line: %r" % line)
            cur = [m.group(1).lower(), m.group(2)]; out.append(cur)
    return out

ESC = {"n": 13, "r": 13, "t": 9, "\\": 92, '"': 34, "'": 39, "a": 7, "b": 8, "f": 12, "v": 11, "0": 0}
def parse_string_literals(s, pos):
    """one or more adjacent string literals starting at POS (after blanks): returns (bytes, new pos).  CMHG's \\n is the RISC OS line end 13."""
    out = bytearray(); first = True
    while True:
        while pos < len(s) and s[pos] in " \t\n\r": pos += 1
        if pos >= len(s) or s[pos] != '"': break
        pos += 1
        while pos < len(s) and s[pos] != '"':
            c = s[pos]
            if c == "\\":
                pos += 1; e = s[pos]
                if e == "x":
                    out.append(int(s[pos + 1:pos + 3], 16)); pos += 2
                elif e in ESC: out.append(ESC[e])
                else: raise CmhgError("unknown escape \\%s in a string" % e)
            else: out.append(ord(c))
            pos += 1
        if pos >= len(s): raise CmhgError("unterminated string")
        pos += 1; first = False
    if first: raise CmhgError("a string was expected near %r" % s[pos:pos + 20])
    return bytes(out), pos

def parse_command_table(rest):
    """'HANDLER\\n name(opts), name(opts) ...' -> (handler, [dict])"""
    m = re.match(r"\s*([A-Za-z_]\w*)\s*", rest)
    if not m: raise CmhgError("command-keyword-table: needs the name of the handler function")
    handler = m.group(1); pos = m.end(); cmds = []
    while pos < len(rest):
        while pos < len(rest) and rest[pos] in " \t\n\r,": pos += 1
        if pos >= len(rest): break
        m = re.match(r"([A-Za-z_][\w$%]*)\s*", rest[pos:])
        if not m: raise CmhgError("a command name was expected near %r" % rest[pos:pos + 30])
        name = m.group(1); pos += m.end()
        cmd = dict(name=name, min=0, max=255, gstrans=0, help=None, syntax=None, flags=0)
        if pos < len(rest) and rest[pos] == "(":
            pos += 1
            while True:
                while pos < len(rest) and rest[pos] in " \t\n\r,": pos += 1
                if rest[pos] == ")": pos += 1; break
                m = re.match(r"([A-Za-z][\w-]*)\s*:?\s*", rest[pos:]); key = m.group(1).lower(); pos += m.end()
                if key in ("min-args", "max-args", "gstrans-map"):
                    m = re.match(r"(0x[0-9A-Fa-f]+|&[0-9A-Fa-f]+|\d+)", rest[pos:]); v = m.group(1); pos += m.end()
                    v = int(v[1:], 16) if v[0] == "&" else int(v, 0)
                    cmd[{"min-args": "min", "max-args": "max", "gstrans-map": "gstrans"}[key]] = v
                elif key in ("help-text", "invalid-syntax"):
                    b, pos = parse_string_literals(rest, pos); cmd["help" if key == "help-text" else "syntax"] = b
                else: raise CmhgError("command option %r is not supported" % key)
        cmds.append(cmd)
    if not cmds: raise CmhgError("the command-keyword-table has no commands")
    return handler, cmds

def parse(text):
    m = dict(title=None, help=None, date=None, init=None, final=None, service=None, service_numbers=[], commands=None, cmd_handler=None, swi_chunk=0, swi_prefix=None, swi_names=[], swi_handler=None,
             veneers=[], runnable=False, warnings=[])
    for key, rest in logical_lines(text):
        r = rest.strip()
        if key == "title-string": m["title"] = r
        elif key == "help-string": m["help"] = r
        elif key == "date-string": m["date"] = r
        elif key == "initialisation-code": m["init"] = r
        elif key == "finalisation-code": m["final"] = r
        elif key == "service-call-handler":
            parts = r.replace(",", " ").split(); m["service"] = parts[0]
            for p in parts[1:]:
                m["service_numbers"].append(int(p[1:], 16) if p[0] == "&" else int(p, 0))
        elif key == "command-keyword-table": m["cmd_handler"], m["commands"] = parse_command_table(rest)
        elif key == "swi-chunk-base-number": m["swi_chunk"] = int(r[1:], 16) if r[0] == "&" else int(r, 0)
        elif key == "swi-decoding-table":
            parts = [p for p in re.split(r"[\s,]+", r) if p]; m["swi_prefix"], m["swi_names"] = parts[0], parts[1:]
        elif key == "swi-handler-code": m["swi_handler"] = r
        elif key in ("irq-handlers", "vector-handlers", "generic-veneers"):
            for ent in [p for p in re.split(r"[\s,]+", r) if p]:
                if "/" not in ent: raise CmhgError("%s: ENTRY/HANDLER was expected, got %r" % (key, ent))
                e, h = ent.split("/", 1); m["veneers"].append((key, e, h))
        elif key == "module-is-runnable": m["runnable"] = True; m["warnings"].append("module-is-runnable: ignored (this model has no start code)")
        else: raise CmhgError("%s: is not supported" % key)
    for k in ("title", "help"):
        if not m[k]: raise CmhgError("%s-string: is missing" % k)
    return m

# ---------------------------------------------------------------- generators
def const(name, fallback):
    # the one number the header needs besides the ones in the CMHG file; RISCOS_SOURCES (a checkout of the RISC OS Open sources) makes it be READ from the sources instead of taken from here
    if rc is None or not rc.BASE: return fallback
    return rc.get(name)

def asm_bytes(b):
    """a .ascii / .byte sequence for BYTES (printable runs as strings)"""
    out, run = [], bytearray()
    def flush():
        if run: out.append('\t.ascii\t"%s"' % bytes(run).decode("latin-1").replace("\\", "\\\\").replace('"', '\\"')); run.clear()
    for c in b:
        if 32 <= c < 127: run.append(c)
        else: flush(); out.append("\t.byte\t%d" % c)
    flush()
    return "\n".join(out) if out else "\t.ascii\t\"\""

def generate_asm(m, src):
    title = m["title"]
    ver = m["help"][len(title):].strip() if m["help"].startswith(title) else m["help"]
    helpline = title + ("\t\t" if len(title) < 8 else "\t") + ver + ((" (" + m["date"] + ")") if m["date"] else "")
    o = []
    A = o.append
    A("@ Generated by mkmodhdr.py from %s.  The module header and the veneers of the module; see modkit/README.md.  DO NOT EDIT." % os.path.basename(src))
    A("\t.syntax\tunified\n\t.arm")
    A("\t.equ\tXOS_SynchroniseCodeAreas, %#x" % const("XOS_SynchroniseCodeAreas", 0x2006E))
    A("\t.section\t\".text.header\",\"ax\"\n\t.global\t_start\n_start:")
    cmds = m["commands"] or []
    has_svc = m["service"] is not None; has_swi = m["swi_handler"] is not None
    A("\t.word\t0\t\t\t\t@ start code (none)")
    A("\t.word\tinit - _start\n\t.word\tfinal - _start")
    A("\t.word\t%s" % ("service - _start\t\t@ service call handler" if has_svc else "0\t\t\t\t@ service call handler (none)"))
    A("\t.word\ttitle - _start\n\t.word\thelp - _start")
    A("\t.word\t%s" % ("cmdtab - _start" if cmds else "0"))
    A("\t.word\t%#x\t\t\t@ SWI chunk base" % (m["swi_chunk"] if has_swi else 0))
    A("\t.word\t%s" % ("swi_entry - _start" if has_swi else "0"))
    A("\t.word\t%s" % ("title - _start\t\t@ the decoding table shares the title string" if has_swi and m["swi_prefix"] else "0"))
    A("\t.word\t0\t\t\t\t@ SWI decoding code\n\t.word\t0\t\t\t\t@ messages file\n\t.word\tflags - _start")
    A("title:\n\t.asciz\t\"%s\"" % title)
    if has_swi and m["swi_prefix"]:
        if m["swi_prefix"].lower() != title.lower():
            raise CmhgError("this model shares the title string with the SWI decoding table: the SWI prefix (%s) must be the title (%s)" % (m["swi_prefix"], title))
        for n in m["swi_names"]: A("\t.asciz\t\"%s\"" % n)
        A("\t.byte\t0\t\t\t\t@ end of the SWI table")
    A("help:\n" + asm_bytes(helpline.encode("latin-1")) + "\n\t.byte\t0\n\t.align\t2")
    # command table
    if cmds:
        for i, c in enumerate(cmds):
            if c["help"] is not None: A("ht%d:\n%s\n\t.byte\t0" % (i, asm_bytes(c["help"])))
            if c["syntax"] is not None: A("is%d:\n%s\n\t.byte\t0" % (i, asm_bytes(c["syntax"])))
        A("\t.align\t2\ncmdtab:")
        for i, c in enumerate(cmds):
            info = c["min"] | (c["gstrans"] << 8) | (min(c["max"], 255) << 16) | (c["flags"] << 24)
            A("\t.asciz\t\"%s\"\n\t.align\t2" % c["name"])
            A("\t.word\tcmd%d - _start\t\t@ code" % i)
            A("\t.word\t%#x\t\t\t@ min %d, max %d parameters" % (info, c["min"], c["max"]))
            A("\t.word\t%s" % ("is%d - _start" % i if c["syntax"] is not None else "0\t\t\t\t@ no invalid syntax line"))
            A("\t.word\t%s" % ("ht%d - _start" % i if c["help"] is not None else "0\t\t\t\t@ no help text"))
        A("\t.word\t0\t\t\t\t@ end of the table")
    A("\t.align\t2\nflags:\t.word\t1\t\t\t\t@ bit 0: 32-bit compatible")
    A("link_addr:\n\t.word\t_start\t\t\t\t@ the linked address of the image (0): relocated with everything else")
    A("reloc_info:\n\t.word\t0\t\t\t\t@ offset of the relocation table (filled in by modreloc.py)\n\t.word\t0\t\t\t\t@ number of entries\n\t.ltorg")
    A("")
    A("@ ---- initialisation: relocate the image (once), then call the module's code.  r10 = environment string, r11 = podule base / instantiation, r12 = private word")
    A("\t.balign\t4\ninit:\n\tstmfd\tsp!, {r4-r11, lr}")
    A("\tadrl\tr4, _start\t\t\t@ where the image is now (PC relative)\n\tldr\tr5, link_addr\t\t\t@ where the linker put it (0), or where it already is (a second initialisation)\n\tsubs\tr6, r4, r5\n\tbeq\trelocated")
    A("\tadrl\tr7, reloc_info\n\tldr\tr8, [r7]\n\tldr\tr9, [r7, #4]\n\tadd\tr8, r4, r8\nrloop:\tcmp\tr9, #0\n\tbeq\trdone\n\tldr\tr0, [r8], #4\n\tldr\tr1, [r4, r0]\n\tadd\tr1, r1, r6\n\tstr\tr1, [r4, r0]\n\tsub\tr9, r9, #1\n\tb\trloop")
    A("rdone:\tmov\tr0, #1\n\tmov\tr1, r4\n\tldr\tr2, =__image_end\n\tsub\tr2, r2, #1\n\tswi\tXOS_SynchroniseCodeAreas\nrelocated:")
    if m["init"]:
        A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone" % m["init"])
    else: A("\tmov\tr0, #0\n\tb\tdone")
    A("\nfinal:\n\tstmfd\tsp!, {r4-r11, lr}")
    if m["final"]:
        A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone" % m["final"])
    else: A("\tmov\tr0, #0\n\tb\tdone")
    # commands
    if cmds:
        A("")
        for i in range(len(cmds)): A("cmd%d:\n\tmov\tr2, #%d\n\tb\tcmd_common" % (i, i))
        A("cmd_common:\t\t\t\t\t@ r0 = argument string, r1 = number of parameters, r2 = the number of the command, r12 = private word\n\tstmfd\tsp!, {r4-r11, lr}\n\tmov\tr3, r12\n\tmov\tr5, r0\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4" % m["cmd_handler"])
        A("\tcmp\tr0, #0\n\tbeq\tcmd_ok\n\tcmp\tr0, r5\t\t\t\t@ the handler gave back the argument string: help_PRINT_BUFFER\n\tbeq\tcmd_ok\n\tcmn\tr0, #1\n\tbeq\tcmd_ok\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\t\t\t@ V set: an error, r0 = the error block\n\tldmfd\tsp!, {r4-r11, pc}\ncmd_ok:\tmov\tr0, #0\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r4-r11, pc}")
    # service calls
    if has_svc:
        A("")
        nums = sorted(set(m["service_numbers"]))
        if nums:
            A("\t.balign\t4\nservtab:\n\t.word\t0\t\t\t\t@ flags\n\t.word\tursservice - _start")
            for n in nums: A("\t.word\t%#x" % n)
            A("\t.word\t0\t\t\t\t@ end of the table\n\t.word\tservtab - _start\t\t@ the anchor: the word before the entry point")
        A("service:\n\tmov\tr0, r0\t\t\t\t@ the magic instruction for the kernel's service call despatcher")
        if nums:
            A("\t" + "\n\t".join(("teq\tr1, #%#x" if k == 0 else "teqne\tr1, #%#x") % n for k, n in enumerate(nums)) + "\n\tmovne\tpc, lr")
        A("ursservice:\n\tstmfd\tsp!, {r0-r11, lr}\n\tmov\tr0, r1\t\t\t\t@ service number\n\tmov\tr1, sp\t\t\t\t@ the registers r0 - r9 as a block\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tldmfd\tsp!, {r0-r11, pc}" % m["service"])
    # SWIs
    if has_swi:
        A("")
        A("swi_entry:\t\t\t\t\t@ r11 = SWI number - chunk base, r0 - r9 = the SWI's registers, r12 = private word\n\tstmfd\tsp!, {r0-r9, lr}\n\tmov\tr0, r11\n\tmov\tr1, sp\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tcmp\tr0, #0\n\tbne\tswi_err\n\tldmfd\tsp!, {r0-r9, pc}\nswi_err:\n\tadd\tsp, sp, #4\n\tldmfd\tsp!, {r1-r9, lr}\n\tmsr\tcpsr_f, #0x10000000\n\tmov\tpc, lr" % m["swi_handler"])
    # vector / IRQ / generic veneers
    for kind, entry, handler in m["veneers"]:
        A("")
        A("\t.global\t%s\n%s:\t\t\t\t\t\t@ %s: r12 = private word; for a vector lr = the pass-on address and the kernel stacked the claim address" % (entry, entry, kind))
        A("\tstmfd\tsp!, {r0-r11, lr}\n\tmov\tr0, sp\t\t\t\t@ the registers as a block\n\tmov\tr1, r12\n\tmrs\tr6, cpsr\n\torr\tr3, r6, #3\t\t\t@ SVC mode (an interrupt handler is entered in IRQ mode: &12 -> &13)\n\tmsr\tcpsr_c, r3\n\tmov\tr7, lr\t\t\t\t@ lr_svc: the interrupted code's\n\tmov\tr4, sp\n\tbic\tsp, sp, #7")
        A("\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n\tmov\tr12, r6\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r0-r11, lr}\n\tldreq\tlr, [sp], #4\t\t\t@ 0: claim the vector: return to the address the kernel stacked\n\tmsr\tcpsr_f, r12\n\tmov\tpc, lr" % handler)
    A("")
    A("done:\t\t\t\t\t\t@ r0 = 0 (V clear) or an error pointer (V set)\n\tcmp\tr0, #0\n\tldmfdeq\tsp!, {r4-r11, pc}\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\n\tldmfd\tsp!, {r4-r11, pc}")
    return "\n".join(o) + "\n"

def generate_h(m, src, name):
    ver = m["help"][len(m["title"]):].strip() if m["help"].startswith(m["title"]) else ""
    vnum = 0
    mm = re.match(r"(\d+)\.(\d+)", ver)
    if mm: vnum = int(mm.group(1)) * 100 + int(mm.group(2)) if len(mm.group(2)) == 2 else int(mm.group(1)) * 100 + int(mm.group(2)) * 10
    guard = "_MODKIT_%s_H_" % re.sub(r"\W", "_", m["title"])
    o = []
    A = o.append
    A("/* Generated by mkmodhdr.py from %s.  DO NOT EDIT. */" % os.path.basename(src))
    A("#ifndef %s\n#define %s\n\n#include \"kernel.h\"\n" % (guard, guard))
    A("#define Module_Title\t\t\"%s\"\n#define Module_Help\t\t\"%s\"\n#define Module_VersionString\t\"%s\"\n#define Module_VersionNumber\t%d\n#ifndef Module_Date\n#define Module_Date\t\t\"%s\"\n#endif\n" % (m["title"], m["title"], ver, vnum, m["date"] or ""))
    A("#ifdef __cplusplus\nextern \"C\" {\n#endif\n")
    if m["init"]: A("_kernel_oserror *%s (const char *tail, int podule_base, void *pw);" % m["init"])
    if m["final"]: A("_kernel_oserror *%s (int fatal, int podule_base, void *pw);" % m["final"])
    if m["commands"]:
        A("_kernel_oserror *%s (const char *arg_string, int argc, int number, void *pw);" % m["cmd_handler"])
        A("#define help_PRINT_BUFFER\t\t((_kernel_oserror *) arg_string)\n")
        A("/* Command numbers, as passed to the command handler function */")
        for i, c in enumerate(m["commands"]): A("#undef CMD_%s\n#define CMD_%s (%d)" % (c["name"], c["name"], i))
        A("")
    if m["service"]: A("void %s (int service_number, _kernel_swi_regs *r, void *pw);" % m["service"])
    if m["swi_handler"]:
        A("_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);" % m["swi_handler"])
        A("#define Module_SWIChunk\t\t%#x" % m["swi_chunk"])
        for i, n in enumerate(m["swi_names"]): A("#define %s_%s\t\t(%#x)" % (m["swi_prefix"], n, m["swi_chunk"] + i))
    for kind, entry, handler in m["veneers"]:
        A("extern void %s (void);\nint %s (_kernel_swi_regs *r, void *pw);" % (entry, handler))
    if m["veneers"]:
        A("\n/* VECTOR_PASSON can be returned from vectors to pass the call on to other claimants; VECTOR_CLAIM to claim it. */\n#define VECTOR_PASSON (1)\n#define VECTOR_CLAIM (0)")
    A("\n#ifdef __cplusplus\n}\n#endif\n#endif")
    return "\n".join(o) + "\n"

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmhg"); ap.add_argument("-s", dest="asm"); ap.add_argument("-d", dest="hdr")
    a = ap.parse_args()
    try:
        m = parse(open(a.cmhg, encoding="latin-1").read())
        for w in m["warnings"]: sys.stderr.write("mkmodhdr: warning: %s\n" % w)
        asm = generate_asm(m, a.cmhg); h = generate_h(m, a.cmhg, os.path.basename(a.cmhg))
    except CmhgError as e:
        sys.exit("mkmodhdr: %s: %s" % (a.cmhg, e))
    base = os.path.splitext(a.cmhg)[0]
    open(a.asm or base + "_hdr.s", "w").write(asm)
    open(a.hdr or base + "_hdr.h", "w").write(h)
main()
