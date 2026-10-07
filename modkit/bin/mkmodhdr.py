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
  international-help-file: "NAME"  the Messages file (as MessageTrans names it; adjacent strings are joined) that the international: texts come from: header word 11, #define Module_MessagesFile in the C header
  module-is-runnable:              the module has a start entry: *RMRun Module args (OS_Module Enter) calls it in USER mode; it takes the top of the application memory (OS_GetEnv) as its stack and calls
                                   int main (int argc, char **argv) - argv[0] is the title, the arguments are the words of the command tail ("..." groups) - and ends the program with its result (libmodkit's
                                   __modlib_start, exit () and atexit () work then).  The module is initialised first, as any module is
  flags of a command (no value):   international: (help-text and invalid-syntax are tokens of the Messages file), add-syntax: (the syntax text follows the help text: *Help shows both), configure: / status:
                                   (a *Configure / *Status command), fs-command: (a command of this filing system module).  help: is refused, as CMunge refuses it
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
def strip_comments(text):
    """a ';' that is not inside a string starts a comment that runs to the end of the line (the line end stays)"""
    out = []; i = 0; n = len(text); inq = False
    while i < n:
        c = text[i]
        if inq:
            out.append(c)
            if c == "\\" and i + 1 < n: out.append(text[i + 1]); i += 2; continue
            if c == '"' or c == "\n": inq = False
        elif c == '"': inq = True; out.append(c)
        elif c == ";":
            while i < n and text[i] != "\n": i += 1
            continue
        else: out.append(c)
        i += 1
    return "".join(out)

def depth_and_last(s):
    """the depth of the parentheses that are open at the end of S (not counting the ones inside strings) and its last character that is not white space"""
    depth = 0; inq = False; last = ""; i = 0
    while i < len(s):
        c = s[i]
        if inq:
            if c == "\\": i += 2; continue
            if c == '"': inq = False
        elif c == '"': inq = True; last = c
        else:
            if c == "(": depth += 1
            elif c == ")": depth -= 1
            if c not in SPACE: last = c
        i += 1
    return depth, last

def logical_lines(text):
    """[key (lower case), the text after the colon]: a line goes on in the next one when the next one starts with white space, when it ends in a comma or has a parenthesis open, and - for the command
    table - when it holds the name of the handler and nothing else (the entries then follow, at the start of the line or not)"""
    out, cur = [], None
    for raw in strip_comments(text).split("\n"):
        line = raw.rstrip("\r")
        if not line.strip(SPACE): continue
        if cur is not None:
            depth, last = depth_and_last(cur[1])
            more = depth > 0 or last == "," or line[0] in " \t"
            if not more and cur[0] == "command-keyword-table" and re.fullmatch(r"[ \t\n\r\v\f]*[A-Za-z_][A-Za-z0-9_]*[ \t\n\r\v\f]*", cur[1]) and not re.match(r"^[A-Za-z][A-Za-z0-9_-]*[ \t\n\r\v\f]*:", line): more = True
            if more: cur[1] += "\n" + line; continue
        m = re.match(r"^([A-Za-z][A-Za-z0-9_-]*)[ \t\n\r\v\f]*:[ \t\n\r\v\f]*(.*)$", line)
        if not m: raise CmhgError("cannot parse the line: %r" % line)
        cur = [m.group(1).lower(), m.group(2)]; out.append(cur)
    return out

def unquote_first(s):
    """the CMHG of the RISC OS build has names in quotes (a macro gives "RTC"): the quotes of the first word, or of the whole value, are taken away"""
    s = s.strip()
    if s.startswith('"'):
        j = s.find('"', 1)
        if j > 0: return s[1:j] + s[j + 1:]
    return s

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
        cmd = dict(name=name, min=0, max=0, gstrans=0, help=None, syntax=None, flags=0, intl=False, add_syntax=False, status=False, fs=False)
        if pos < len(rest) and rest[pos] == "(":
            pos += 1
            while True:
                while pos < len(rest) and rest[pos] in " \t\n\r,": pos += 1
                if rest[pos] == ")": pos += 1; break
                m = re.match(r"([A-Za-z][\w-]*)\s*:?\s*", rest[pos:]); key = m.group(1).lower(); pos += m.end()
                if key in ("min-args", "max-args", "gstrans-map"):
                    m = re.match(r"(0x[0-9A-Fa-f]+|&[0-9A-Fa-f]+|\d+)", rest[pos:])
                    if not m: raise CmhgError("a number was expected near '%s'" % rest[pos:pos + 30])
                    v = m.group(1); pos += m.end()
                    v = int(v[1:], 16) if v[0] == "&" else int(v, 0)
                    cmd[{"min-args": "min", "max-args": "max", "gstrans-map": "gstrans"}[key]] = v
                elif key == "international": cmd["intl"] = True
                elif key == "add-syntax": cmd["add_syntax"] = True
                elif key in ("configure", "status"): cmd["status"] = True
                elif key == "fs-command": cmd["fs"] = True
                elif key in ("help-text", "invalid-syntax"):
                    b, pos = parse_string_literals(rest, pos); cmd["help" if key == "help-text" else "syntax"] = b
                else: raise CmhgError("command option %r is not supported" % key)
        if cmd["min"] > 255: raise CmhgError("min-args: must be between 0 and 255 in command %s" % name)
        if cmd["max"] > 255: raise CmhgError("max-args: must be between 0 and 255 in command %s" % name)
        if cmd["gstrans"] > 255: raise CmhgError("gstrans-map: may only describe 8 bits in command %s" % name)
        if cmd["add_syntax"] and cmd["intl"]: raise CmhgError("add-syntax: and international: are mutually exclusive in command %s" % name)
        cmds.append(cmd)
    if not cmds: raise CmhgError("the command-keyword-table has no commands")
    return handler, cmds

def parse(text):
    m = dict(title=None, help=None, date=None, init=None, final=None, service=None, service_numbers=[], commands=None, cmd_handler=None, swi_chunk=0, swi_prefix=None, swi_names=[], swi_handler=None,
             veneers=[], runnable=False, mfile=None, warnings=[])
    for key, rest in logical_lines(text):
        r = rest.strip()
        if key == "title-string": m["title"] = unquote_first(r)
        elif key == "help-string": m["help"] = unquote_first(r)
        elif key == "date-string": m["date"] = unquote_first(r)
        elif key == "initialisation-code": m["init"] = r
        elif key == "finalisation-code": m["final"] = r
        elif key == "service-call-handler":
            parts = r.replace(",", " ").split(); m["service"] = parts[0]
            for p in parts[1:]:
                m["service_numbers"].append(int(p[1:], 16) if p[0] == "&" else int(p, 0))
        elif key == "command-keyword-table": m["cmd_handler"], m["commands"] = parse_command_table(rest)
        elif key == "swi-chunk-base-number":
            m["swi_chunk"] = int(r[1:], 16) if r[0] == "&" else int(r, 0)
            if m["swi_chunk"] == 0 or m["swi_chunk"] & 0x3f: raise CmhgError("swi-chunk-base-number: 0x%08x is not a SWI chunk (a multiple of 64, not 0)" % m["swi_chunk"])
            if m["swi_chunk"] & 0x20000: raise CmhgError("swi-chunk-base-number: 0x%08x has the X bit set (&20000)" % m["swi_chunk"])
        elif key == "swi-decoding-table":
            parts = [p.strip('"') for p in re.split(r"[\s,]+", r) if p]; m["swi_prefix"], m["swi_names"] = parts[0], parts[1:]
        elif key == "swi-handler-code": m["swi_handler"] = r
        elif key in ("irq-handlers", "vector-handlers", "generic-veneers"):
            if "(" in r: raise CmhgError("%s: handler options such as private-word: and carry-capable: are not supported" % key)
            for ent in [p for p in re.split(r"[\s,]+", r) if p]:
                e, h = ent.split("/", 1) if "/" in ent else (ent, ent + "_handler")             # CMunge: the handler of NAME is NAME_handler
                m["veneers"].append((key, e, h))
        elif key == "module-is-runnable": m["runnable"] = True
        elif key == "international-help-file": m["mfile"], _ = parse_string_literals(rest, 0)
        else: raise CmhgError("%s: is not supported" % key)
    for k in ("title", "help"):
        if not m[k]: raise CmhgError("%s-string: is missing" % k)
    # the SWIs of a module: a chunk, a handler and a decoding table go together (CMunge: a prefix of the module's title when there is no table)
    if m["swi_handler"] is not None and not m["swi_chunk"]: raise CmhgError("swi-handler-code: needs a swi-chunk-base-number:")
    if m["swi_chunk"] and m["swi_handler"] is None: raise CmhgError("swi-chunk-base-number: needs a swi-handler-code:")
    if m["swi_prefix"] is not None and not m["swi_chunk"]: raise CmhgError("swi-decoding-table: needs a swi-chunk-base-number:")
    if m["swi_handler"] is not None and m["swi_prefix"] is None: m["swi_prefix"] = m["title"]
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

SPACE = " \t\n\r\v\f"

def split_help(help_string):
    """CMunge's DateStamp: the help-string is a name (the words up to the first one that starts with a digit), a version (digits, a dot, digits and what follows up to a blank) and a rest.  Returns
    (name, version, rest, version number).  The version number is the digits of the version before and after the dot read as one number (1.23 is 123, 1.5 is 15)."""
    s = help_string.lstrip(SPACE); n = len(s); pos = 0; name = ""
    bad = CmhgError("Malformed help-string found: %s" % help_string)
    while True:
        j = pos
        while j < n and s[j] not in SPACE: j += 1
        name += s[pos:j]; pos = j
        while pos < n and s[pos] in SPACE: pos += 1
        more = pos < n and not ("0" <= s[pos] <= "9")
        if more: name += " "
        else: break
    if pos >= n or not ("0" <= s[pos] <= "9"): raise bad
    j = pos; digits = ""
    while j < n and "0" <= s[j] <= "9": digits += s[j]; j += 1
    if j >= n or s[j] != ".": raise bad
    j += 1
    while j < n and "0" <= s[j] <= "9": digits += s[j]; j += 1
    while j < n and s[j] not in SPACE: j += 1
    version = s[pos:j]; pos = j
    while pos < n and s[pos] in SPACE: pos += 1
    return name, version, s[pos:], int(digits) & 0xFFFFFFFF

def help_line(m):
    """the help line of the module: the name, one or two tabs (to column 16), the version, the date in brackets, the rest; underscores are blanks"""
    name, version, rest, _ = split_help(m["help"])
    tabs = "\t" if ((len(name) + 8) & ~7) >= 16 else "\t\t"
    line = name + tabs + version + ((" (" + m["date"] + ")") if m["date"] else "") + ((" " + rest) if rest else "")
    return line.replace("_", " ")

def representable(i):
    """the part of I that one ARM immediate (8 bits at an even position) can hold, from the lowest set bit up: CMunge's representable ()"""
    mask = 255
    while (i & mask) & ~(mask << 2) & 0xFFFFFFFF == 0:
        mask = ((mask << 2) | (mask >> 30)) & 0xFFFFFFFF
        if mask == 255: break                                              # I is 0: every window is empty
    return i & mask

def generate_asm(m, src):
    title = m["title"]
    helpline = help_line(m)
    o = []
    A = o.append
    A("@ Generated by mkmodhdr.py from %s.  The module header and the veneers of the module; see modkit/README.md.  DO NOT EDIT." % os.path.basename(src))
    A("\t.syntax\tunified\n\t.arm")
    A("\t.equ\tXOS_SynchroniseCodeAreas, %#x" % const("XOS_SynchroniseCodeAreas", 0x2006E))
    if m["runnable"]: A("\t.equ\tOS_GetEnv, 0x10")
    A("\t.section\t\".text.header\",\"ax\"\n\t.global\t_start\n_start:")
    cmds = m["commands"] or []
    has_svc = m["service"] is not None; has_swi = m["swi_handler"] is not None
    A("\t.word\t%s" % ("start - _start\t\t\t@ start code (module-is-runnable)" if m["runnable"] else "0\t\t\t\t@ start code (none)"))
    A("\t.word\tinit - _start\n\t.word\t%s" % ("final - _start" if m["final"] else "0\t\t\t\t@ finalisation (none)"))
    A("\t.word\t%s" % ("service - _start\t\t@ service call handler" if has_svc else "0\t\t\t\t@ service call handler (none)"))
    A("\t.word\ttitle - _start\n\t.word\thelp - _start")
    A("\t.word\t%s" % ("cmdtab - _start" if cmds else "0"))
    A("\t.word\t%#x\t\t\t@ SWI chunk base" % (m["swi_chunk"] if has_swi else 0))
    A("\t.word\t%s" % ("swi_entry - _start" if has_swi else "0"))
    share = bool(has_swi and m["swi_prefix"] and m["swi_prefix"].lower() == title.lower())
    A("\t.word\t%s" % ("title - _start\t\t@ the decoding table shares the title string" if share else "swi_table - _start" if has_swi and m["swi_prefix"] else "0"))
    A("\t.word\t0\t\t\t\t@ SWI decoding code\n\t.word\t%s\n\t.word\tflags - _start" % ("msgfile - _start\t\t@ messages file (international-help-file)" if m["mfile"] is not None else "0\t\t\t\t@ messages file"))
    A("title:\n\t.asciz\t\"%s\"" % title)
    if share:
        for n in m["swi_names"]: A("\t.asciz\t\"%s\"" % n)
        A("\t.byte\t0\t\t\t\t@ end of the SWI table")
    A("help:\n" + asm_bytes(helpline.encode("latin-1")) + "\n\t.byte\t0\n\t.align\t2")
    if m["mfile"] is not None: A("msgfile:\n" + asm_bytes(m["mfile"]) + "\n\t.byte\t0")
    if has_swi and m["swi_prefix"] and not share:
        A("swi_table:\n\t.asciz\t\"%s\"" % m["swi_prefix"])
        for n in m["swi_names"]: A("\t.asciz\t\"%s\"" % n)
        A("\t.byte\t0\t\t\t\t@ end of the SWI table")
    # command table
    if cmds:
        for i, c in enumerate(cmds):
            if c["help"] is not None:
                if c["add_syntax"] and c["syntax"] is not None: A("ht%d:\n%s" % (i, asm_bytes(c["help"])))       # the syntax text follows at once: *Help shows both, the error only the syntax
                else: A("ht%d:\n%s\n\t.byte\t0" % (i, asm_bytes(c["help"])))
            if c["syntax"] is not None: A("is%d:\n%s\n\t.byte\t0" % (i, asm_bytes(c["syntax"])))
        A("\t.align\t2\ncmdtab:")
        for i, c in enumerate(cmds):
            info = c["min"] | (c["gstrans"] << 8) | (c["max"] << 16) | (int(c["fs"]) << 31) | (int(c["status"]) << 30) | (int(c["intl"]) << 28)
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
    if m["final"]:
        A("\nfinal:\n\tstmfd\tsp!, {r4-r11, lr}")
        A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone" % m["final"])
    if m["runnable"]:
        A("")
        A("@ ---- module-is-runnable: entered in USER mode by *RMRun / OS_Module Enter with r0 = the command tail and r12 = the private word; the stack is the top of the application memory (OS_GetEnv).\n"
          "@ __modlib_start (tail, title) of libmodkit builds argc / argv, calls main and ends the program with its result (OS_Exit): it does not come back.\n"
          "\t.balign\t4\nstart:\n\tmov\tr4, r0\n\tswi\tOS_GetEnv\n\tbic\tsp, r1, #7\n\tmov\tr0, r4\n\tadrl\tr1, title\n\tbl\t__modlib_start")
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
            big = any(representable(n) != n for n in nums)                           # a number that one ARM immediate cannot hold is built in lr (as CMunge does)
            lines = ["str\tlr, [sp, #-4]!"] if big else []
            for k, n in enumerate(nums):
                part = representable(n)
                if part == n: lines.append(("teq\tr1, #%#x" if k == 0 else "teqne\tr1, #%#x") % n)
                else:
                    lines.append("mov\tlr, #%#x" % part); left = n & ~part
                    while left:
                        part = representable(left); lines.append("orr\tlr, lr, #%#x" % part); left &= ~part
                    lines.append("teq\tr1, lr" if k == 0 else "teqne\tr1, lr")
            lines += ["ldmfdne\tsp!, {pc}", "ldr\tlr, [sp], #4"] if big else ["movne\tpc, lr"]
            A("\t" + "\n\t".join(lines))
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
        if kind == "generic-veneers":
            # as CMunge's: 0 = return to the caller with the registers as the handler left them in the block and the flags as they were; anything else = return with V set and r0 = that value
            A("\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n\tcmp\tr0, #0\n\tstrne\tr0, [sp]\t\t\t@ the error block goes back into r0\n\torrne\tr6, r6, #0x10000000\t\t@ and V is set\n\tmsr\tcpsr_f, r6\n\tldmfd\tsp!, {r0-r11, pc}" % handler)
        else:
            A("\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n\tmov\tr12, r6\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r0-r11, lr}\n\tldreq\tlr, [sp], #4\t\t\t@ 0: claim the vector: return to the address the kernel stacked\n\tmsr\tcpsr_f, r12\n\tmov\tpc, lr" % handler)
    A("")
    A("done:\t\t\t\t\t\t@ r0 = 0 (V clear) or an error pointer (V set)\n\tcmp\tr0, #0\n\tldmfdeq\tsp!, {r4-r11, pc}\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\n\tldmfd\tsp!, {r4-r11, pc}")
    return "\n".join(o) + "\n"

def generate_h(m, src, name):
    hname, hver, _, vnum = split_help(m["help"])
    guard = "_MODKIT_%s_H_" % re.sub(r"\W", "_", m["title"])
    o = []
    A = o.append
    A("/* Generated by mkmodhdr.py from %s.  DO NOT EDIT. */" % os.path.basename(src))
    A("#ifndef %s\n#define %s\n\n#include \"kernel.h\"\n" % (guard, guard))
    A("#define Module_Title\t\t\"%s\"\n#define Module_Help\t\t\"%s\"\n#define Module_VersionString\t\"%d.%02d\"\n#define Module_VersionNumber\t%d\n#ifndef Module_Date\n#define Module_Date\t\t\"%s\"\n#endif\n" % (m["title"], hname, vnum // 100, vnum % 100, vnum, m["date"] or ""))
    if m["mfile"] is not None:
        q = "".join(("\\" + chr(c)) if c in (34, 92) else chr(c) for c in m["mfile"])      # the name as a C string
        A("#define Module_MessagesFile\t\"%s\"\n" % q)
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
        A("extern void %s (void);\n%s %s (_kernel_swi_regs *r, void *pw);" % (entry, "_kernel_oserror *" if kind == "generic-veneers" else "int", handler))        # (CMunge: a generic handler returns an error)
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
