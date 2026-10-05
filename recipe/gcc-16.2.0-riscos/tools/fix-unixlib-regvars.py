#!/usr/bin/env python3
"""fix-unixlib-regvars.py IN OUT -- rewrite the inline SWI wrappers of UnixLib's incl-local/internal/os.h so that no OUTPUT register variable is read after the asm.

The wrappers declare their results as local register variables (register T x __asm ("r1");) and read them in C after the asm.  GCC documents that a local register variable
is guaranteed to be in its register only where it is an operand of an asm; everywhere else the variable IS the hard register (the expander gives every SSA name of it the
hard register as its location).  When the value is used after a call, the call has overwritten the register.  GCC 16 -O2 did exactly that to SWI_DDEUtils_GetCLSize in
__unixinit: arg_size became the result of the strlen () that follows it (com_size), so every UnixLib program started with arguments longer than its own name under the DDEUtils
module (a text editor loads it) got its arguments cut to the length of the name, in a heap block too small for them.

The rewrite: the input register variables stay (they are only used as operands); an input whose register the SWI changes becomes an in-out operand ("+r"); every output is an ordinary
variable that the asm itself fills (MOV %[out], rN at the end of the template, early clobber); a register the SWI changes that is not an operand goes into the clobber list.
Everything else (names, templates, post code, comments) is kept as it was."""
import re, sys

def split_top(s, sep):
    out, depth, cur, i, n = [], 0, "", 0, len(s)
    instr = False
    while i < n:
        c = s[i]
        if instr:
            cur += c
            if c == '\\': cur += s[i + 1]; i += 1
            elif c == '"': instr = False
        elif c == '"': instr = True; cur += c
        elif c in "([": depth += 1; cur += c
        elif c in ")]": depth -= 1; cur += c
        elif c == sep and depth == 0: out.append(cur); cur = ""
        else: cur += c
        i += 1
    out.append(cur)
    return out

def find_asm(body):
    """the extended asm statement of a function body: (start, end) of '__asm__ volatile (' ... ');'"""
    start = body.index("__asm__ volatile (")
    i = body.index("(", start)
    depth, instr, j = 0, False, i
    while True:
        c = body[j]
        if instr:
            if c == '\\': j += 1
            elif c == '"': instr = False
        elif c == '"': instr = True
        elif c == '(': depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0: break
        j += 1
    return start, j + 2        # past ");"

OPER = re.compile(r'^\s*(?:\[(\w+)\]\s*)?"([^"]*)"\s*\((.*)\)\s*$', re.S)

def parse_ops(s):
    ops = []
    for part in split_top(s, ','):
        if not part.strip(): continue
        m = OPER.match(part)
        if not m: raise SystemExit("cannot parse operand: %r" % part)
        ops.append((m.group(1), m.group(2), m.group(3).strip()))
    return ops

def fmt_op(op):
    lab, con, expr = op
    return ('[%s] ' % lab if lab else '') + '"%s" (%s)' % (con, expr)

# registers the SWI changes although the old asm did not say so (OS_GBPB 2 and 4 return the advanced buffer address in R2): the input becomes an in-out operand
FORCE_INOUT = {"SWI_OS_GBPB_ReadBytes": ["buf"], "SWI_OS_GBPB_WriteBytes": ["buf"]}

def transform(func):
    fname = re.search(r"^(\w+) \(", func, re.M).group(1)
    # register declarations
    decls = []
    for m in re.finditer(r'^( *)register ([^;=]*?)\s*(\w+) __asm \("(r\d+)"\)( = [^;]*)?;\n', func, re.M):
        decls.append(dict(text=m.group(0), indent=m.group(1), type=m.group(2).strip(), name=m.group(3), reg=m.group(4), init=m.group(5)))
    if not decls: return func, False
    bs = func.index("{\n") + 2
    head, body = func[:bs], func[bs:]
    a0, a1 = find_asm(body)
    asm = body[a0:a1]
    inner = asm[asm.index("(") + 1: asm.rindex(")")]
    secs = split_top(inner, ':')
    if len(secs) < 3: raise SystemExit("unexpected asm layout in %s" % func[:80])
    tmpl = secs[0]
    outs = parse_ops(secs[1]); ins = parse_ops(secs[2])
    clob = [c.strip() for c in split_top(secs[3], ',')] if len(secs) > 3 else []
    clob = [c for c in clob if c]
    byname = {d['name']: d for d in decls}
    # outputs that are register variables without initialiser -> captured
    new_outs, captured, modregs = [], [], set()
    for lab, con, expr in outs:
        d = byname.get(expr)
        if d and d['init'] is None and con in ('=r', '=&r'):
            captured.append(d); modregs.add(d['reg'])
        else:
            new_outs.append((lab, '=&r' if con == '=r' else con, expr))      # an ordinary variable the template fills itself
    for c in clob:
        m = re.match(r'^"(r\d+)"$', c)
        if m: modregs.add(m.group(1))
    for nm in FORCE_INOUT.get(fname, []):
        if nm in byname: modregs.add(byname[nm]['reg'])
    # inputs
    new_ins, inout = [], []
    for lab, con, expr in ins:
        d = byname.get(expr)
        if d and d['init'] is not None and con == 'r' and d['reg'] in modregs:
            inout.append(('+r', d))
        else:
            new_ins.append((lab, con, expr))
    inout_regs = set(d['reg'] for _, d in inout)
    # an input register variable and an output register variable that share a register without the input being listed (r1 for objtype / filename ...) are handled above (inout)
    # clobbers: the changed registers that are not operand registers
    new_clob = [c for c in clob if not (re.match(r'^"(r\d+)"$', c) and c.strip('"') in inout_regs)]
    have = set(c.strip('"') for c in new_clob)
    for r in sorted(modregs - inout_regs, key=lambda x: int(x[1:])):
        if r not in have: new_clob.insert(0, '"%s"' % r); have.add(r)
    # but an input register variable that is NOT changed keeps being a plain input; a changed register that is an input operand of a variable that was not matched (should not happen)
    # template: append the captures
    cap = ""
    for d in captured:
        cap += "\"MOV\\t%%[%s], %s\\n\\t\"" % (d['name'], d['reg'])
    # rebuild the template literal: keep the original lines, add ours (each as a separate string literal on its own line)
    t = tmpl.rstrip()
    indent = "\t\t    "
    cap_lines = "".join("\n" + indent + "\"MOV\\t%%[%s], %s\\n\\t\"" % (d['name'], d['reg']) for d in captured)
    # the original template ends with a string literal; add after it
    t_new = t + cap_lines
    # new operand lists
    out_all = [(d['name'], '=&r', d['name']) for d in captured] + new_outs + [(None, c, d['name']) for c, d in inout]
    # operand order: labelled outputs first, then in-out
    s_out = (",\n" + indent + "  ").join(fmt_op(o) for o in out_all)
    s_in = (",\n" + indent + "  ").join(fmt_op(o) for o in new_ins)
    s_clob = ", ".join(new_clob)
    new_asm = "__asm__ volatile (" + t_new.lstrip() + "\n" + indent + ": " + s_out + "\n" + indent + ": " + s_in + "\n" + indent + ": " + s_clob + ");"
    # declarations: drop the output register declarations, declare ordinary variables in their place (before the asm)
    new_body = body[:a0] + new_asm + body[a1:]
    for d in captured:
        newdecl = "%s%s %s;\n" % (d['indent'], d['type'], d['name'])
        new_body = new_body.replace(d['text'], newdecl, 1)
    return head + new_body, True

def main():
    src = open(sys.argv[1]).read()
    parts = re.split(r'(?=\nstatic __inline__ )', src)
    out = []
    n = 0
    for p in parts:
        if p.startswith("\nstatic __inline__") and "__asm (\"r" in p:
            # a function ends at the first "\n}\n" after its start
            m = re.search(r"\n\}[ \t]*\n", p)
            end = m.end()
            f, rest = p[:end], p[end:]
            nf, changed = transform(f.lstrip("\n"))
            out.append("\n" + nf + rest)
            n += changed
        else:
            out.append(p)
    open(sys.argv[2], "w").write("".join(out))
    sys.stderr.write("rewrote %d wrappers\n" % n)
main()
