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
                                   a name can have a function of its own, NAME/FN, with the same arguments (the other SWIs go to swi-handler-code, which may then be left out: a SWI that has neither is error_BAD_SWI)
  swi-decoding-code: FN            int FN (_kernel_swi_regs *r, void *pw): OS_SWINumberFromString / OS_SWINumberToString for the chunk (r0 - r3 as the kernel gives them; a name is the whole SWI name with its prefix, and
                                   the function writes the whole name), or NAME/NUMBER: two functions,
                                   int NAME (const char *name, void *pw) and int NUMBER (int number, char *buffer, int offset, int limit, void *pw); needs a swi-chunk-base-number:
  command table:                   command-keyword-table: -  has no handler of the table; a command can have  handler: FN  (same arguments as the table's handler) or  no-handler:  (no code at all: a command that
                                   only has a help text); a command without either, in a table without a handler, has no handler
  options of the veneers, ENTRY/FN (options):   generic-veneers: private-word: rN (the function gets rN, not r12, and r12 is kept for the caller), carry-capable: (FN may return VENEER_SETCARRY, 2:
                                   return with C set and V clear); vector-handlers: error-capable: (FN may return VECTOR_ERROR (error): claim the vector, V set, r0 = the error); irq-handlers: none
  module-is-not-reentrant:         accepted, no effect (as in CMunge)
  irq-handlers:, vector-handlers:, generic-veneers: ENTRY/FN, ...     int FN (_kernel_swi_regs *r, void *pw): the veneer saves every register, switches to SVC mode, calls FN with the register block (r0 - r9, r10,
                                   r11) and the private word and returns to the original mode; FN returns 0 to CLAIM a vector (the veneer returns to the claim address the kernel stacked), non-zero to pass on
                                   (for a callback: always non-zero, the veneer returns with MOV pc, lr)
  international-help-file: "NAME"  the Messages file (as MessageTrans names it; adjacent strings are joined) that the international: texts come from: header word 11, #define Module_MessagesFile in the C header
  module-is-c-plus-plus:           the module is written in C++ (compile with -fno-exceptions -fno-rtti): the initialisation veneer calls libmodkit's __modlib_cxx_init (the static constructors) before the
                                   initialisation code, the finalisation veneer calls __modlib_cxx_fini (the destructors of the static objects and .fini_array) after the finalisation code (lib/cxxrt.c)
  module-is-runnable:              the module has a start entry: *RMRun Module args (OS_Module Enter) calls it in USER mode; it takes the top of the application memory (OS_GetEnv) as its stack and calls
                                   int main (int argc, char **argv) - argv[0] is the title, the arguments are the words of the command tail ("..." groups) - and ends the program with its result (libmodkit's
                                   __modlib_start, exit () and atexit () work then).  The module is initialised first, as any module is
  flags of a command (no value):   international: (help-text and invalid-syntax are tokens of the Messages file), add-syntax: (the syntax text follows the help text: *Help shows both), configure: / status:
                                   (a *Configure / *Status command), fs-command: (a command of this filing system module).  help: is refused, as CMunge refuses it
Anything else is an error, not silently dropped (CMunge's error-base:, error-identifiers:, pdriver-handler:, vector-traps: and module-is-runnable: simple-app: are not supported).  All the RISC OS numbers in the output come from the RISC OS headers (riscos_consts.py)."""
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
            if not more and cur[0] == "command-keyword-table" and re.fullmatch(r"[ \t\n\r\v\f]*(-|[A-Za-z_][A-Za-z0-9_]*)[ \t\n\r\v\f]*", cur[1]) and not re.match(r"^[A-Za-z][A-Za-z0-9_-]*[ \t\n\r\v\f]*:", line): more = True
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

_NUM = re.compile(r"&([0-9A-Fa-f]+)|0[xX]([0-9A-Fa-f]+)|([0-9]+)")
def cexpr(text):
    """A number of a CMHG file after the C preprocessor (-p): a constant expression, the grammar of cmunge.c's parse_int: numbers (decimal, 0xHEX, &HEX: a & that starts a term is the hex prefix, one
    after a term is "and"), parentheses, unary - + ~, * / %, + -, << >>, & ^ |, in C's order of precedence; a number may end in u or l.  The result must fit 32 bits."""
    s = text; n = len(s); pos = [0]
    def fail(): raise CmhgError("%s is not a number" % text)
    def skip():
        while pos[0] < n and s[pos[0]] in " \t": pos[0] += 1
    def atom():
        skip()
        if pos[0] >= n: fail()
        c = s[pos[0]]
        if c == "(":
            pos[0] += 1; v = bor(); skip()
            if pos[0] >= n or s[pos[0]] != ")": fail()
            pos[0] += 1; return v
        if c == "-": pos[0] += 1; return -atom()
        if c == "+": pos[0] += 1; return atom()
        if c == "~": pos[0] += 1; return ~atom()
        m = _NUM.match(s, pos[0])
        if not m: fail()
        v = int(m.group(1), 16) if m.group(1) else int(m.group(2), 16) if m.group(2) else int(m.group(3), 10)
        pos[0] = m.end()
        while pos[0] < n and s[pos[0]] in "uUlL": pos[0] += 1
        return v
    def mul():
        v = atom()
        while True:
            skip()
            if pos[0] < n and s[pos[0]] == "*": pos[0] += 1; v *= atom()
            elif pos[0] < n and s[pos[0]] in "/%":
                op = s[pos[0]]; pos[0] += 1; d = atom()
                if d == 0: raise CmhgError("%s: division by zero" % text)
                q = abs(v) // abs(d) * (1 if (v < 0) == (d < 0) else -1)          # C: truncation towards zero
                v = q if op == "/" else v - q * d
            else: return v
    def add():
        v = mul()
        while True:
            skip()
            if pos[0] < n and s[pos[0]] == "+": pos[0] += 1; v += mul()
            elif pos[0] < n and s[pos[0]] == "-": pos[0] += 1; v -= mul()
            else: return v
    def shift():
        v = add()
        while True:
            skip()
            if s.startswith("<<", pos[0]):
                pos[0] += 2; k = add(); v = ((v & 0xFFFFFFFFFFFFFFFF) << k) & 0xFFFFFFFFFFFFFFFF if 0 <= k < 64 else 0
            elif s.startswith(">>", pos[0]):
                pos[0] += 2; k = add(); v = (v & 0xFFFFFFFFFFFFFFFF) >> k if 0 <= k < 64 else 0
            else: return v
    def band():
        v = shift()
        while True:
            skip()
            if pos[0] < n and s[pos[0]] == "&": pos[0] += 1; v &= shift()
            else: return v
    def bxor():
        v = band()
        while True:
            skip()
            if pos[0] < n and s[pos[0]] == "^": pos[0] += 1; v ^= band()
            else: return v
    def bor():
        v = bxor()
        while True:
            skip()
            if pos[0] < n and s[pos[0]] == "|": pos[0] += 1; v |= bxor()
            else: return v
    v = bor(); skip()
    if pos[0] < n: fail()
    if v < 0 or v > 0xFFFFFFFF: raise CmhgError("%s is out of range" % text)
    return v

def parse_command_table(rest):
    """'HANDLER\\n name(opts), name(opts) ...' -> (handler, [dict])"""
    m = re.match(r"\s*(-(?![\w$%])|[A-Za-z_]\w*)\s*", rest)
    if not m: raise CmhgError("command-keyword-table: needs the name of the handler function (or - when every command has its own handler: or none)")
    handler = None if m.group(1) == "-" else m.group(1); pos = m.end(); cmds = []
    while pos < len(rest):
        while pos < len(rest) and rest[pos] in " \t\n\r,": pos += 1
        if pos >= len(rest): break
        m = re.match(r"([A-Za-z_][\w$%]*)\s*", rest[pos:])
        if not m: raise CmhgError("a command name was expected near %r" % rest[pos:pos + 30])
        name = m.group(1); pos += m.end()
        cmd = dict(name=name, min=0, max=0, gstrans=0, help=None, syntax=None, flags=0, intl=False, add_syntax=False, status=False, fs=False, handler=None, nohandler=False)
        if pos < len(rest) and rest[pos] == "(":
            pos += 1
            while True:
                while pos < len(rest) and rest[pos] in " \t\n\r,": pos += 1
                if rest[pos] == ")": pos += 1; break
                m = re.match(r"([A-Za-z][\w-]*)\s*:?\s*", rest[pos:]); key = m.group(1).lower(); pos += m.end()
                if key in ("min-args", "max-args", "gstrans-map"):
                    m = re.match(r"(\((?:[^()]|\([^()]*\))*\)|0x[0-9A-Fa-f]+|&[0-9A-Fa-f]+|\d+)", rest[pos:])        # (2): a number that the preprocessor made
                    if not m: raise CmhgError("a number was expected near '%s'" % rest[pos:pos + 30])
                    v = m.group(1); pos += m.end()
                    v = cexpr(v)
                    cmd[{"min-args": "min", "max-args": "max", "gstrans-map": "gstrans"}[key]] = v
                elif key == "international": cmd["intl"] = True
                elif key == "add-syntax": cmd["add_syntax"] = True
                elif key in ("configure", "status"): cmd["status"] = True
                elif key == "fs-command": cmd["fs"] = True
                elif key == "no-handler": cmd["nohandler"] = True
                elif key == "handler":
                    m = re.match(r"([A-Za-z_]\w*)", rest[pos:])
                    if not m: raise CmhgError("handler: needs the name of a function in command %s" % name)
                    if cmd["handler"] is not None: raise CmhgError("Only supply one handler: field in command %s" % name)
                    cmd["handler"] = m.group(1); pos += m.end()
                elif key in ("help-text", "invalid-syntax"):
                    b, pos = parse_string_literals(rest, pos); cmd["help" if key == "help-text" else "syntax"] = b
                else: raise CmhgError("command option %r is not supported" % key)
        if cmd["min"] > 255: raise CmhgError("min-args: must be between 0 and 255 in command %s" % name)
        if cmd["max"] > 255: raise CmhgError("max-args: must be between 0 and 255 in command %s" % name)
        if cmd["gstrans"] > 255: raise CmhgError("gstrans-map: may only describe 8 bits in command %s" % name)
        if cmd["add_syntax"] and cmd["intl"]: raise CmhgError("add-syntax: and international: are mutually exclusive in command %s" % name)
        if cmd["nohandler"] and cmd["handler"] is not None: raise CmhgError("no-handler: and handler: are mutually exclusive in command %s" % name)
        if handler is None and cmd["handler"] is None: cmd["nohandler"] = True          # (CMunge: a command with nothing to call has no handler)
        cmds.append(cmd)
    if not cmds: raise CmhgError("the command-keyword-table has no commands")
    return handler, cmds

def parse_handlers(key, r):
    """ENTRY[/HANDLER][(options)] ... -> [(entry, handler, {private-word: n, carry-capable, error-capable})]: the options that CMunge allows - private-word: rN and carry-capable: for generic-veneers, error-capable: for vector-handlers, none for irq-handlers"""
    allow = {"generic-veneers": ("private-word", "carry-capable"), "vector-handlers": ("error-capable",), "irq-handlers": ()}[key]
    out = []; pos = 0
    while True:
        while pos < len(r) and r[pos] in " \t\n\r\v\f,": pos += 1
        if pos >= len(r): break
        m = re.match(r"([A-Za-z_]\w*)(?:/([A-Za-z_]\w*))?[ \t\n\r\v\f]*", r[pos:])
        if not m: raise CmhgError("%s: a function name was expected near %r" % (key, r[pos:pos + 30]))
        e = m.group(1); h = m.group(2) or e + "_handler"; pos += m.end()           # CMunge: the handler of NAME is NAME_handler
        opts = {}
        if pos < len(r) and r[pos] == "(":
            pos += 1
            while True:
                while pos < len(r) and r[pos] in " \t\n\r\v\f,": pos += 1
                if pos >= len(r): raise CmhgError("%s: Ran out of file when parsing handler details!" % key)
                if r[pos] == ")": pos += 1; break
                m = re.match(r"(private-word|carry-capable|error-capable):[ \t\n\r\v\f]*", r[pos:])
                if not m: raise CmhgError("%s: Unknown argument in handler: %s" % (key, r[pos:pos + 30]))
                k = m.group(1); pos += m.end()
                if k not in allow: raise CmhgError("%s: %s argument not permitted" % (key, k))
                if k in opts: raise CmhgError("%s: %s supplied twice!" % (key, k))
                if k == "private-word":
                    m = re.match(r"[Rr]([0-9]+)", r[pos:])
                    if not m: raise CmhgError("%s: private-word value must be a register!" % key)
                    n = int(m.group(1)); pos += m.end()
                    if n > 12: raise CmhgError("%s: private-word register must be r0-r12!" % key)
                    opts[k] = n
                else: opts[k] = True
            while pos < len(r) and r[pos] in " \t\n\r\v\f": pos += 1
        out.append((e, h, opts))
    return out

def one_name(key, r):
    """the value of a directive that names one function: a C identifier (CMHG's own options in brackets, such as swi-handler-code: name (flags-capable:), are not supported)"""
    m = re.match(r"[A-Za-z_]\w*", r)
    if not m: raise CmhgError("%s: needs the name of a function" % key)
    if m.end() != len(r): raise CmhgError("%s: needs one function name; '%s' is not supported" % (key, r[m.end():]))
    return r

def parse(text):
    m = dict(title=None, help=None, date=None, init=None, final=None, cxx=False, service=None, service_numbers=[], commands=None, cmd_handler=None, swi_chunk=0, swi_prefix=None, swi_names=[], swi_handler=None,
             veneers=[], runnable=False, mfile=None, warnings=[], swi_handlers=[], swi_decoder=None, swi_decoder2=None, notreent=False)
    for key, rest in logical_lines(text):
        r = rest.strip()
        if key == "title-string": m["title"] = unquote_first(r)
        elif key == "help-string": m["help"] = unquote_first(r)
        elif key == "date-string": m["date"] = unquote_first(r)
        elif key == "initialisation-code": m["init"] = one_name(key, r)
        elif key == "finalisation-code": m["final"] = one_name(key, r)
        elif key == "service-call-handler":
            parts = r.replace(",", " ").split(); m["service"] = parts[0]
            for p in parts[1:]:
                m["service_numbers"].append(cexpr(p))
        elif key == "command-keyword-table": m["cmd_handler"], m["commands"] = parse_command_table(rest)
        elif key == "swi-chunk-base-number":
            m["swi_chunk"] = cexpr(r)
            if m["swi_chunk"] == 0 or m["swi_chunk"] & 0x3f: raise CmhgError("swi-chunk-base-number: 0x%08x is not a SWI chunk (a multiple of 64, not 0)" % m["swi_chunk"])
            if m["swi_chunk"] & 0x20000: raise CmhgError("swi-chunk-base-number: 0x%08x has the X bit set (&20000)" % m["swi_chunk"])
        elif key == "swi-decoding-table":
            parts = [p.strip('"') for p in re.split(r"[\s,]+", r) if p]; m["swi_prefix"], m["swi_names"] = parts[0], []
            if "(" in r: raise CmhgError("swi-decoding-table: SWI handlers cannot be passed parameters")
            m["swi_handlers"] = []
            for p in parts[1:]:
                n, h = p.split("/", 1) if "/" in p else (p, None)
                if h is not None and not re.fullmatch(r"[A-Za-z_]\w*", h): raise CmhgError("swi-decoding-table: %r is not a function name" % h)
                m["swi_names"].append(n); m["swi_handlers"].append(h)
        elif key == "swi-handler-code": m["swi_handler"] = one_name(key, r)
        elif key == "swi-decoding-code":
            if m["swi_decoder"] is not None: raise CmhgError("Only supply one swi-decoding-code!")
            parts = [p for p in re.split(r"[\s,]+", r) if p]            # FN (one function that gets the registers) or NAME/NUMBER...: NAMETONUMBER/NUMBERTONAME
            if len(parts) != 1 or not re.fullmatch(r"[A-Za-z_]\w*(/[A-Za-z_]\w*)?", parts[0]): raise CmhgError("swi-decoding-code: needs one function name, or NAME/HANDLER (the name to number function and the number to name function)")
            m["swi_decoder"], m["swi_decoder2"] = parts[0].split("/", 1) if "/" in parts[0] else (parts[0], None)
        elif key == "module-is-not-reentrant":
            if r: raise CmhgError("module-is-not-reentrant: takes no value")
            m["notreent"] = True
        elif key in ("irq-handlers", "vector-handlers", "generic-veneers"):
            for e, h, o in parse_handlers(key, r): m["veneers"].append((key, e, h, (), o))
        elif key == "event-handler":
            # ENTRY[/HANDLER] [number ...]: a veneer for the event vector; events that are not in the list go on at once, otherwise it works as a vector-handlers veneer
            if re.search(r"\(\s*[A-Za-z]", r): raise CmhgError("Event handlers cannot be passed parameters")        # (the event numbers may be in parentheses: the preprocessor made them)
            words = [p for p in re.split(r"[\s,]+", r) if p]
            if not words: raise CmhgError("event-handler: needs the name of the handler function")
            e, h = words[0].split("/", 1) if "/" in words[0] else (words[0], words[0] + "_handler")
            m["veneers"].append((key, e, h, tuple(cexpr(w) for w in words[1:]), {}))
        elif key == "module-is-runnable":
            if r: raise CmhgError("module-is-runnable: takes no value here (CMunge's simple-app: is not supported)")
            m["runnable"] = True
        elif key == "module-is-c-plus-plus":
            if r: raise CmhgError("module-is-c-plus-plus: takes no value")
            m["cxx"] = True
        elif key == "international-help-file": m["mfile"], _ = parse_string_literals(rest, 0)
        elif key in ("library-enter-code", "library-initialisation-code"): raise CmhgError("%s: is not supported (it redirects the start-up of the Shared C Library, which a modkit module does not have)" % key)
        else: raise CmhgError("%s: is not supported" % key)
    for k in ("title", "help"):
        if not m[k]: raise CmhgError("%s-string: is missing" % k)
    # the SWIs of a module: a chunk, a handler and a decoding table go together (CMunge: a prefix of the module's title when there is no table)
    per_swi = any(h is not None for h in m["swi_handlers"])
    if (m["swi_handler"] is not None or per_swi) and not m["swi_chunk"]: raise CmhgError("swi-handler-code: needs a swi-chunk-base-number:")
    if m["swi_chunk"] and m["swi_handler"] is None and not per_swi: raise CmhgError("swi-chunk-base-number: needs a swi-handler-code:")
    if m["swi_prefix"] is not None and not m["swi_chunk"]: raise CmhgError("swi-decoding-table: needs a swi-chunk-base-number:")
    if m["swi_decoder"] is not None and not m["swi_chunk"]: raise CmhgError("swi-decoding-code: needs a swi-chunk-base-number:")
    m["swi_notable"] = m["swi_decoder"] is not None and m["swi_prefix"] is None             # decoding code and no table of names: the header has no table, so that the kernel asks the code in both directions
    if (m["swi_handler"] is not None or per_swi) and m["swi_prefix"] is None: m["swi_prefix"] = m["title"]
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
    if m.get("base"):                                                                       # -zbase: Image__RO_Base, a word that holds the address of the module (the word is relocated like any other)
        A("\t.section\t.rodata\n\t.align\t2\n\t.global\tImage__RO_Base\n\t.type\tImage__RO_Base, %object\n\t.size\tImage__RO_Base, 4\nImage__RO_Base:\n\t.word\tImage$$RO$$Base\n\t.text")
    A("\t.equ\tXOS_SynchroniseCodeAreas, %#x" % const("XOS_SynchroniseCodeAreas", 0x2006E))
    if m["runnable"]: A("\t.equ\tOS_GetEnv, 0x10")
    A("\t.section\t\".text.header\",\"ax\"\n\t.global\t_start\n_start:")
    cmds = m["commands"] or []
    has_svc = m["service"] is not None; per_swi = any(h is not None for h in m["swi_handlers"]); has_swi = m["swi_handler"] is not None or per_swi
    A("\t.word\t%s" % ("start - _start\t\t\t@ start code (module-is-runnable)" if m["runnable"] else "0\t\t\t\t@ start code (none)"))
    A("\t.word\tinit - _start\n\t.word\t%s" % ("final - _start" if (m["final"] or m["cxx"]) else "0\t\t\t\t@ finalisation (none)"))
    A("\t.word\t%s" % ("service - _start\t\t@ service call handler" if has_svc else "0\t\t\t\t@ service call handler (none)"))
    A("\t.word\ttitle - _start\n\t.word\thelp - _start")
    A("\t.word\t%s" % ("cmdtab - _start" if cmds else "0"))
    A("\t.word\t%#x\t\t\t@ SWI chunk base" % (m["swi_chunk"] if has_swi else 0))
    A("\t.word\t%s" % ("swi_entry - _start" if has_swi else "0"))
    share = bool(has_swi and m["swi_prefix"] and m["swi_prefix"].lower() == title.lower() and not m["swi_notable"])
    A("\t.word\t%s" % ("title - _start\t\t@ the decoding table shares the title string" if share else "swi_table - _start" if has_swi and m["swi_prefix"] and not m["swi_notable"] else "0"))
    A("\t.word\t%s\n\t.word\t%s\n\t.word\tflags - _start" % ("swi_decode - _start\t\t@ SWI decoding code" if m["swi_decoder"] else "0\t\t\t\t@ SWI decoding code", ("msgfile - _start\t\t@ messages file (international-help-file)" if m["mfile"] is not None else "0\t\t\t\t@ messages file")))
    A("title:\n\t.asciz\t\"%s\"" % title)
    if share:
        for n in m["swi_names"]: A("\t.asciz\t\"%s\"" % n)
        A("\t.byte\t0\t\t\t\t@ end of the SWI table")
    A("help:\n" + asm_bytes(helpline.encode("latin-1")) + "\n\t.byte\t0\n\t.align\t2")
    if m["mfile"] is not None: A("msgfile:\n" + asm_bytes(m["mfile"]) + "\n\t.byte\t0")
    if has_swi and m["swi_prefix"] and not share and not m["swi_notable"]:
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
            A("\t.word\t%s" % ("0\t\t\t\t@ no handler (no-handler:)" if c["nohandler"] else "cmd%d - _start\t\t@ code" % i))
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
    if m["cxx"]:                                                                          # module-is-c-plus-plus: the static constructors run before the module's own initialisation code (libmodkit's cxxrt.c; the private word is kept in r8)
        A("\tmov\tr8, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t__modlib_cxx_init\n\tmov\tsp, r4\n\tmov\tr12, r8")
    if m["init"]:
        A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone" % m["init"])
    else: A("\tmov\tr0, #0\n\tb\tdone")
    if m["cxx"]:                                                                          # ... and the destructors of the static objects after the module's finalisation code
        A("\nfinal:\n\tstmfd\tsp!, {r4-r11, lr}")
        if m["final"]: A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tmov\tr8, r0" % m["final"])
        else: A("\tmov\tr8, #0")
        A("\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t__modlib_cxx_fini\n\tmov\tsp, r4\n\tmov\tr0, r8\n\tb\tdone")
    elif m["final"]:
        A("\nfinal:\n\tstmfd\tsp!, {r4-r11, lr}")
        A("\tmov\tr0, r10\n\tmov\tr1, r11\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tb\tdone" % m["final"])
    if m["runnable"]:
        A("")
        A("@ ---- module-is-runnable: entered in USER mode by *RMRun / OS_Module Enter with r0 = the command tail and r12 = the private word; the stack is the top of the application memory (OS_GetEnv).\n"
          "@ __modlib_start (tail, title) of libmodkit builds argc / argv, calls main and ends the program with its result (OS_Exit): it does not come back.\n"
          "\t.balign\t4\nstart:\n\tmov\tr4, r0\n\tswi\tOS_GetEnv\n\tbic\tsp, r1, #7\n\tmov\tr0, r4\n\tadrl\tr1, title\n\tbl\t__modlib_start")
    # commands
    handled = [c for c in cmds if not c["nohandler"]]
    if handled:
        A("")
        hl = []                                                                         # the functions that the commands call (the table's own handler is the default), in the order of the commands
        for c in handled:
            h = c["handler"] or m["cmd_handler"]
            if h not in hl: hl.append(h)
        def hlabel(h): return "cmd_common" if h == m["cmd_handler"] else "cmd_h%d" % hl.index(h)
        for i, c in enumerate(cmds):
            if not c["nohandler"]: A("cmd%d:\n\tmov\tr2, #%d\n\tb\t%s" % (i, i, hlabel(c["handler"] or m["cmd_handler"])))
        for j, h in enumerate(hl):
            A("%s:%s\n\tstmfd\tsp!, {r4-r11, lr}\n\tmov\tr3, r12\n\tmov\tr5, r0\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4%s" % (hlabel(h), "\t\t\t\t\t@ r0 = argument string, r1 = number of parameters, r2 = the number of the command, r12 = private word" if j == 0 else "", h, "\n\tb\tcmd_ret" if j < len(hl) - 1 else ""))
        if len(hl) > 1: A("cmd_ret:")
        A("\tcmp\tr0, #0\n\tbeq\tcmd_ok\n\tcmp\tr0, r5\t\t\t\t@ the handler gave back the argument string: help_PRINT_BUFFER\n\tbeq\tcmd_ok\n\tcmn\tr0, #1\t\t\t\t@ configure_BAD_OPTION (-1): V set with r0 = 0, as CMunge's veneer returns it\n\tmoveq\tr0, #0\n\tmov\tr1, #0\n\tcmp\tr1, #0x80000000\t\t\t@ V set: an error, r0 = the error block\n\tldmfd\tsp!, {r4-r11, pc}\ncmd_ok:\tmov\tr0, #0\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r4-r11, pc}")
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
        if per_swi:                                                                     # swi-decoding-table: Name/function ...: the SWI's own function (the same arguments as swi-handler-code's); the other SWIs go to swi-handler-code, or are not known (error_BAD_SWI)
            dflt = m["swi_handler"] or "swi_unknown"
            swi_call = "\tadrl\tlr, swi_ret\t\t\t\t@ the functions below return here\n\tcmp\tr0, #%d\n\taddlo\tpc, pc, r0, lsl #2\n\tb\t%s\n" % (len(m["swi_names"]), dflt)
            for h in m["swi_handlers"]: swi_call += "\tb\t%s\n" % (h or dflt)
            swi_call += ("" if m["swi_handler"] else "swi_unknown:\n\tmvn\tr0, #0\t\t\t\t\t@ error_BAD_SWI\n") + "swi_ret:\n"
        else: swi_call = "\tbl\t%s\n" % m["swi_handler"]
        A("swi_entry:\t\t\t\t\t@ r11 = SWI number - chunk base, r0 - r9 = the SWI's registers, r12 = private word\n\tstmfd\tsp!, {r0-r9, lr}\n\tmov\tr0, r11\n\tmov\tr1, sp\n\tmov\tr2, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n" + swi_call + "\tmov\tsp, r4\n\tcmp\tr0, #0\n\tbne\tswi_err\n\tldmfd\tsp!, {r0-r9, pc}\nswi_err:\n\tadd\tsp, sp, #4\n\tcmn\tr0, #1\t\t\t\t\t@ error_BAD_SWI (-1)?\n\tbeq\tswi_bad\nswi_err2:\n\tldmfd\tsp!, {r1-r9, lr}\n\tmsr\tcpsr_f, #0x10000000\n\tmov\tpc, lr\nswi_bad:\t\t\t\t\t\t@ the error of the system for a SWI that is not in the module, made as CMunge's veneer makes it: SWI value out of range for module <title>\n\tadr\tr0, swi_bad_block\n\tmov\tr1, #0\n\tmov\tr2, #0\n\tadrl\tr4, title\n\tswi\t0x61506\t\t\t\t\t@ XMessageTrans_ErrorLookup: r0 -> the error, V set\n\tb\tswi_err2\nswi_bad_block:\n\t.word\t0x1e6\n\t.asciz\t\"BadSWI\"\n\t.balign\t4")
    # vector / IRQ / generic veneers
    if m["swi_decoder"]:
        A("")
        if m["swi_decoder2"] is None:
            A("swi_decode:\t\t\t\t\t@ swi-decoding-code FN: int FN (_kernel_swi_regs *r, void *pw) gets r0 - r3 as the kernel gave them (r0 < 0: r1 = the whole name with its prefix, answer the offset in r0; else r0 = the offset, r1 = buffer, r2 = where to write the whole name, r3 = the limit, answer the new r2)\n"
              "\tstmfd\tsp!, {r0-r3, r4, lr}\n\tmov\tr0, sp\n\tmov\tr1, r12\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tbl\t%s\n\tmov\tsp, r4\n\tldmfd\tsp!, {r0-r3, r4, pc}" % m["swi_decoder"])
        else:
            A("swi_decode:\t\t\t\t\t@ swi-decoding-code NAME/NUMBER: int NAME (const char *name, void *pw) for a name (r0 < 0), int NUMBER (int number, char *buffer, int offset, int limit, void *pw) for a number\n"
              "\tstmfd\tsp!, {r0-r3, r4, lr}\n\tmov\tr4, sp\n\tbic\tsp, sp, #7\n\tcmp\tr0, #0\n\tbge\tswi_decode_num\n\tmov\tr0, r1\n\tmov\tr1, r12\n\tbl\t%s\n\tmov\tsp, r4\n\tstr\tr0, [sp]\t\t\t\t@ r0 = the offset in the chunk\n\tldmfd\tsp!, {r0-r3, r4, pc}\n"
              "swi_decode_num:\n\tsub\tsp, sp, #8\n\tstr\tr12, [sp]\t\t\t\t@ the private word is the fifth argument\n\tbl\t%s\n\tadd\tsp, sp, #8\n\tmov\tsp, r4\n\tstr\tr0, [sp, #8]\t\t\t@ r2 = the new offset in the buffer\n\tldmfd\tsp!, {r0-r3, r4, pc}" % (m["swi_decoder"], m["swi_decoder2"]))
    for kind, entry, handler, events, opts in m["veneers"]:
        A("")
        A("\t.global\t%s\n%s:\t\t\t\t\t\t@ %s: r12 = private word; for a vector lr = the pass-on address and the kernel stacked the claim address" % (entry, entry, kind))
        for e, n in enumerate(events): A("\t%s\tr0, #%u" % ("teq" if e == 0 else "teqne", n))
        if events: A("\tmovne\tpc, lr")
        pw = opts.get("private-word", 12); top = "r12" if pw != 12 else "r11"               # private-word: rN - the handler gets rN, and r12 is kept for the caller
        if pw == 12: movs = "\tmov\tr0, sp\t\t\t\t@ the registers as a block\n\tmov\tr1, r12\n"
        else: movs = "\tmov\tr1, r%d\t\t\t\t@ the private word is in r%d (private-word:)\n\tmov\tr0, sp\t\t\t\t@ the registers as a block\n" % (pw, pw)
        A("\tstmfd\tsp!, {r0-%s, lr}\n%s\tmrs\tr6, cpsr\n\torr\tr3, r6, #3\t\t\t@ SVC mode (an interrupt handler is entered in IRQ mode: &12 -> &13)\n\tmsr\tcpsr_c, r3\n\tmov\tr7, lr\t\t\t\t@ lr_svc: the interrupted code's\n\tmov\tr4, sp\n\tbic\tsp, sp, #7" % (top, movs))
        if kind == "generic-veneers":
            # as CMunge's: 0 = return to the caller with the registers as the handler left them in the block and the flags as they were; anything else = return with V set and r0 = that value;
            # carry-capable: 2 = return with C set (and V clear), the registers as the handler left them
            if opts.get("carry-capable"):
                err = "\tcmp\tr0, #2\n\torreq\tr6, r6, #0x20000000\t\t@ 2: C set\n\tbiceq\tr6, r6, #0x10000000\t\t@ and V clear\n\tcmpne\tr0, #0\n"
            else: err = "\tcmp\tr0, #0\n"
            A("\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n%s\tstrne\tr0, [sp]\t\t\t@ the error block goes back into r0\n\torrne\tr6, r6, #0x10000000\t\t@ and V is set\n\tmsr\tcpsr_f, r6\n\tldmfd\tsp!, {r0-%s, pc}" % (handler, err, top))
        elif opts.get("error-capable"):
            # error-capable: 0 = claim, 1 = pass on, anything else is a pointer to an error block: claim with V set and r0 = the block
            A("\tbl\t%s\n\tmov\tsp, r4\n\tmov\tlr, r7\n\tmsr\tcpsr_c, r6\t\t\t@ the mode we were called in\n\tmov\tr12, r6\n\tcmp\tr0, #1\n\tbhi\t%s_err\n\tcmp\tr0, #0\n\tldmfd\tsp!, {r0-r11, lr}\n\tldreq\tlr, [sp], #4\t\t\t@ 0: claim the vector: return to the address the kernel stacked\n\tmsr\tcpsr_f, r12\n\tmov\tpc, lr\n%s_err:\n\tstr\tr0, [sp]\t\t\t\t@ the error block goes back into r0\n\torr\tr12, r12, #0x10000000\t\t@ V set\n\tldmfd\tsp!, {r0-r11, lr}\n\tldr\tlr, [sp], #4\t\t\t@ and the vector is claimed\n\tmsr\tcpsr_f, r12\n\tmov\tpc, lr" % (handler, entry, entry))
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
    if m.get("base"): A("extern const int Image__RO_Base;\n")
    if m["init"]: A("_kernel_oserror *%s (const char *tail, int podule_base, void *pw);" % m["init"])
    if m["final"]: A("_kernel_oserror *%s (int fatal, int podule_base, void *pw);" % m["final"])
    if m["commands"]:
        fns = ([m["cmd_handler"]] if m["cmd_handler"] else []) + [c["handler"] for c in m["commands"] if c["handler"]]
        seen = set()
        for fn in fns:
            if fn not in seen: seen.add(fn); A("_kernel_oserror *%s (const char *arg_string, int argc, int number, void *pw);" % fn)
        A("#define help_PRINT_BUFFER\t\t((_kernel_oserror *) arg_string)\n#define arg_CONFIGURE_SYNTAX\t\t((char *) 0)\n#define arg_STATUS\t\t\t((char *) 1)\n#define configure_BAD_OPTION\t\t((_kernel_oserror *) -1)\n#define configure_NUMBER_NEEDED\t\t((_kernel_oserror *) 1)\n#define configure_TOO_LARGE\t\t((_kernel_oserror *) 2)\n#define configure_TOO_MANY_PARAMS\t((_kernel_oserror *) 3)\n")
        A("/* Command numbers, as passed to the command handler function */")
        for i, c in enumerate(m["commands"]): A("#undef CMD_%s\n#define CMD_%s (%d)" % (c["name"], c["name"], i))
        A("")
    if m["service"]: A("void %s (int service_number, _kernel_swi_regs *r, void *pw);" % m["service"])
    if m["swi_decoder"]:
        if m["swi_decoder2"] is None: A("int %s (_kernel_swi_regs *r, void *pw);" % m["swi_decoder"])
        else: A("int %s (const char *name, void *pw);\nint %s (int number, char *buffer, int offset, int limit, void *pw);" % (m["swi_decoder"], m["swi_decoder2"]))
    if m["swi_handler"] or any(m["swi_handlers"]):
        if m["swi_handler"]: A("_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);" % m["swi_handler"])
        for fn in sorted(set(h for h in m["swi_handlers"] if h)): A("_kernel_oserror *%s (int swi_offset, _kernel_swi_regs *r, void *pw);" % fn)
        A("#define Module_SWIChunk\t\t%#x" % m["swi_chunk"])
        A("\n/* SWI number definitions (as CMunge writes them) */\n#define %s_00 (%#x)" % (m["swi_prefix"], m["swi_chunk"]))
        for i, n in enumerate(m["swi_names"]):
            A("#undef %s_%s\n#undef X%s_%s\n#define %s_%s\t\t(%#x)\n#define X%s_%s\t\t(%#x)" % (m["swi_prefix"], n, m["swi_prefix"], n, m["swi_prefix"], n, m["swi_chunk"] + i, m["swi_prefix"], n, m["swi_chunk"] + i + 0x20000))
        A("\n/* Special error for 'SWI values out of range for this module' */\n#define error_BAD_SWI ((_kernel_oserror *) -1)")
    for kind, entry, handler, _, _o in m["veneers"]:
        A("extern void %s (void);\n%s %s (_kernel_swi_regs *r, void *pw);" % (entry, "_kernel_oserror *" if kind == "generic-veneers" else "int", handler))        # (CMunge: a generic handler returns an error)
    if any(v[0] == "generic-veneers" and v[4].get("carry-capable") for v in m["veneers"]):
        A("\n/* VENEER_SETCARRY can be returned from a generic veneer's function that is carry-capable: return with C set, all other registers as the function left them */\n#define VENEER_SETCARRY ((_kernel_oserror *) 2)")
    if m["veneers"]:
        A("\n/* VECTOR_PASSON can be returned from vectors to pass the call on to other claimants; VECTOR_CLAIM to claim it. */\n#define VECTOR_PASSON (1)\n#define VECTOR_CLAIM (0)")
        if any(v[4].get("error-capable") for v in m["veneers"]):
            A("\n/* VECTOR_ERROR (err) can be returned from a vector handler that is error-capable: claim the vector and return with V set and r0 = the error block */\n#define VECTOR_ERROR(err) ((int) (err))")
    A("\n#ifdef __cplusplus\n}\n#endif\n#endif")
    return "\n".join(o) + "\n"

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmhg"); ap.add_argument("-s", dest="asm"); ap.add_argument("-d", dest="hdr"); ap.add_argument("--base", action="store_true", help="-zbase of CMunge: Image__RO_Base, the address of the module")
    a = ap.parse_args()
    try:
        m = parse(open(a.cmhg, encoding="latin-1").read())
        for w in m["warnings"]: sys.stderr.write("mkmodhdr: warning: %s\n" % w)
        m["base"] = a.base
        asm = generate_asm(m, a.cmhg); h = generate_h(m, a.cmhg, os.path.basename(a.cmhg))
    except CmhgError as e:
        sys.exit("mkmodhdr: %s: %s" % (a.cmhg, e))
    base = os.path.splitext(a.cmhg)[0]
    open(a.asm or base + "_hdr.s", "w").write(asm)
    open(a.hdr or base + "_hdr.h", "w").write(h)
main()
