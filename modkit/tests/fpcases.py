"""fpcases.py -- the answers for tests/hwpack/fpsvc.c (the module FpSvc of the hardware packs and of tests/sim-fpsvc.py): the header fpcases.h with the cases, worked out on the host.

  formatting   glibc's snprintf (through ctypes) for 40 values x 14 formats chosen by a fixed random sequence: the strings the module must produce
  parsing      glibc's strtod and strtof: bits, where the number ends, ERANGE: the usual texts, numbers half way between two doubles (worked out exactly with fractions, with and without a tiny
               excess behind 800 digits), hexadecimal numbers, and junk
  arithmetic   the hash of a loop of 2000 double and float operations (Python floats are IEEE doubles), and a recurrence of 100 steps for the generic veneer
  heavy        %.1000f of 1e300 and %.1100e of 5e-324 (Python's % is exact), and 800 digit numbers

make_header () returns the text.  Everything is deterministic."""
import ctypes, math, random, struct
from decimal import Decimal, getcontext
from fractions import Fraction

libc = ctypes.CDLL(None, use_errno=True)
libc.snprintf.restype = ctypes.c_int
libc.strtod.restype = ctypes.c_double
libc.strtod.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
libc.strtof.restype = ctypes.c_float
libc.strtof.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
getcontext().prec = 2000

def d2u(x): return struct.unpack("<Q", struct.pack("<d", x))[0]
def u2d(u): return struct.unpack("<d", struct.pack("<Q", u))[0]
def f2u(x): return struct.unpack("<I", struct.pack("<f", x))[0]
def canon_d(u): return 0x7FF8000000000000 if (u & 0x7FF0000000000000) == 0x7FF0000000000000 and (u & 0xFFFFFFFFFFFFF) else u
def canon_f(u): return 0x7FC00000 if (u & 0x7F800000) == 0x7F800000 and (u & 0x7FFFFF) else u

def cstr(s):
    """a C string literal (octal escapes, so that a digit after an escape is not part of it)"""
    out = ['"']
    for ch in s.encode("latin-1"):
        if ch == 0x22: out.append('\\"')
        elif ch == 0x5C: out.append("\\\\")
        elif 32 <= ch < 127: out.append(chr(ch))
        else: out.append("\\%03o" % ch)
    out.append('"')
    return "".join(out)

def glibc_format(fmt, x):
    buf = ctypes.create_string_buffer(4000)
    n = libc.snprintf(buf, 4000, fmt.encode(), ctypes.c_double(x))
    assert n < 4000
    return buf.value.decode("latin-1")

def glibc_parse(text):
    """(double bits, float bits, double end, float end, double range error, float range error) by glibc"""
    raw = text.encode("latin-1") + b"\0"
    buf = ctypes.create_string_buffer(raw, len(raw))
    base = ctypes.addressof(buf)
    res = []
    for fn, conv in ((libc.strtod, lambda v: canon_d(d2u(v))), (libc.strtof, lambda v: canon_f(f2u(v)))):
        end = ctypes.c_void_p()
        ctypes.set_errno(0)
        v = fn(ctypes.c_void_p(base), ctypes.byref(end))
        err = ctypes.get_errno() == 34
        res.append((conv(v), (end.value or base) - base, err))
    return res[0][0], res[1][0], res[0][1], res[1][1], res[0][2], res[1][2]

FVALUES = [0.1, 1 / 3, 2 / 3, 1e22, 1e23, 123456789.125, 1.7976931348623157e308, 2.2250738585072014e-308, 5e-324, -0.0, math.inf, -math.inf, 0.5, 1.5, 2.5, 9.9995, 99999.5, 1e-5, 123e-20,
           math.pi, math.e, 0.000123456, 100.0, 1e15, 1e16, 2.0 ** 63, 2.0 ** 64, 0.3, 1.0, 12345.6789, 4.35, 2.675, 1e-310, -123.456, 0.0, 1e100, 255.0, 0.999999, 9.999999999, 6.02214076e23]
FBITS = [d2u(v) for v in FVALUES] + [0x7FF8000000000000, 0xFFF8000000000000]
FFORMATS = ["%f", "%.0f", "%.3f", "%10.4f|", "%-12.2f|", "%+.1f", "% f", "%012.3f", "%e", "%.0e", "%.10e", "%E", "%g", "%.3g", "%.12g", "%.17g", "%G", "%#g", "%#.0f", "%a", "%.3a", "%A", "%20.10e|",
            "%.20f", "%5.1f", "%.0g", "%#.3g", "%-+10.3e|", "%010.2g", "%.1a", "%#a", "% .0e", "%.40f", "%F", "%+g"]

def format_cases():
    rnd = random.Random(39)
    cases = []
    for bits in FBITS:
        for fmt in rnd.sample(FFORMATS, 14):
            cases.append((fmt, bits, glibc_format(fmt, u2d(bits))))
    return cases

def exact_decimal(fr):
    """the exact decimal text of a Fraction whose denominator is a power of 2"""
    d = Decimal(fr.numerator) / Decimal(fr.denominator)
    return format(d, "f")

def midpoint_text(m, e, tail=0):
    """the number half way between m*2^e and (m+1)*2^e, exactly, as text; TAIL: a 1 after that many zeros"""
    fr = Fraction(2 * m + 1) * (Fraction(2) ** (e - 1))
    s = exact_decimal(fr)
    if tail:
        if "." not in s: s += "."
        s += "0" * (tail - 1) + "1"
    return s

FIXED_TEXTS = ["0", "-0", "1", "+1", " \t 12", "1e", "1e+", "1e5x", ".5", "5.", "-.5e1", ".", "e5", "-", "", "0x", "0xg", "0x1p", "0x1.8p+1", "0X1P-1074", "0x1p-1075", "0x1.8p-1075", "0x1p-1076",
               "0x1.fffffffffffff8p1023", "0x1.fffffffffffff7p1023", "0x1p1024", "0x1.00000000000008p0", "0x1.00000000000018p0", "inf", "-INF", "infinity", "infinit", "nan", "-nan", "NAN(abc)", "nan(",
               "1e308", "1.7976931348623157e308", "1.7976931348623158e308", "1e309", "-1e400", "1e-323", "4.9406564584124654e-324", "2.4703282292062327e-324", "2.4703282292062328e-324", "1e-325",
               "2.2250738585072011e-308", "2.2250738585072012e-308", "2.2250738585072014e-308", "9007199254740993", "9007199254740992.5", "0.1", "0.3", "123456789012345678901234567890", "1_000", "12abc",
               "1e1000000000000", "1e-1000000000000", "3.4028235e38", "3.4028236e38", "3.4028235677973366e38", "1.4e-45", "7.006492321624085e-46", "7.006492321624086e-46", "1.17549435e-38", "16777217",
               "16777216.5", "1e39", "0.000000000000000000000000000001", "8.5e-3", "2.5e+10", "-7.25", "1e22", "1e23", "5e-324", "0x.8p1", "0x0.0000000000001p-1022", "1.0000000000000002", "0.99999999999999989"]

def parse_texts():
    rnd = random.Random(40)
    texts = list(FIXED_TEXTS)
    for i in range(12):                                         # half way between two doubles, and a little above it
        m = rnd.randrange(1 << 52, 1 << 53); e = rnd.randrange(-80, 80)
        texts.append(midpoint_text(m, e)); texts.append(midpoint_text(m, e, tail=1 + rnd.randrange(900)))
    for i in range(6):                                          # half way between two floats
        m = rnd.randrange(1 << 23, 1 << 24); e = rnd.randrange(-149, 104)
        texts.append(midpoint_text(m, e)); texts.append(midpoint_text(m, e, tail=1 + rnd.randrange(300)))
    for i in range(10):                                         # hexadecimal ties
        m = rnd.randrange(1 << 52, 1 << 53); e = rnd.randrange(-1100, 960)
        texts.append("0x%xp%d" % (2 * m + 1, e - 1))
    for i in range(14):                                         # random digits with a point and an exponent
        nd = rnd.randrange(1, 30)
        digs = "".join(rnd.choice("0123456789") for _ in range(nd))
        pt = rnd.randrange(0, nd + 1)
        texts.append(digs[:pt] + "." + digs[pt:] + ("e%+d" % rnd.randrange(-330, 330) if rnd.random() < 0.7 else ""))
    return texts

def heavy_texts():
    rnd = random.Random(41)
    texts = []
    texts.append(midpoint_text(1, -1074))                                     # half way between 0 and the smallest subnormal: 751 digits, a tie that goes to 0
    texts.append(midpoint_text(1, -1074, tail=60))                            # a little above: the smallest subnormal
    texts.append(midpoint_text((1 << 52) + 12345, -1074))                     # a subnormal tie
    texts.append(midpoint_text((1 << 53) - 1, 970))                           # half way between the largest double and 2^1024: rounds to infinity (a tie goes to the even one, which overflows)
    m = rnd.randrange(1 << 52, 1 << 53)
    texts.append(midpoint_text(m, -1000, tail=100))
    texts.append(midpoint_text(m, -1000))
    texts.append("0." + "0" * 300 + "".join(rnd.choice("0123456789") for _ in range(790)))
    texts.append("".join(rnd.choice("0123456789") for _ in range(790)) + "e-780")
    return texts

def arith_hash():
    h = 1469598103934665603
    x, y = 1.0, 3.0
    for i in range(2000):
        k = i & 7
        if k == 0: r = x + y
        elif k == 1: r = x - y
        elif k == 2: r = x * y
        elif k == 3: r = x / y
        elif k == 4: r = struct.unpack("<f", struct.pack("<f", x))[0]
        elif k == 5: r = float(int(x)) if (x > -2e9 and x < 2e9) else 0.0
        elif k == 6: r = float(int(y)) if (y > -0.5 and y < 4e9) else 1.0
        else: r = x if x < y else y
        u = canon_d(d2u(r))
        h = ((h ^ u) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        x = x * 1.0009765625 + 0.3; y = y * 0.99951171875 - 0.1
        if x > 1e300 or x < -1e300: x = 1.5
    return h

def tick_bits(n):
    acc = 1.0
    for i in range(n): acc = acc * 1.0000001 + 0.5
    return d2u(acc)

def fnv(text):
    h = 1469598103934665603
    for b in text.encode("latin-1"): h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h

# ---- math.h: (name, C arguments, how Python works out the answer, tolerance in units in the last place; 0 = exact)
def _rint(x): return float(round(x))                          # ties to even, as rint and nearbyint
def _frexp_m(x): return math.frexp(x)[0]
def _modf_frac(x): return math.modf(x)[0]
MATH_FUNCS = [
    # name,      args, domain,  python,                 tol
    ("sqrt",      1, "pos",     math.sqrt,               0),
    ("floor",     1, "any",     math.floor,              0),
    ("ceil",      1, "any",     math.ceil,               0),
    ("trunc",     1, "any",     math.trunc,              0),
    ("rint",      1, "any",     _rint,                   0),
    ("fabs",      1, "any",     math.fabs,               0),
    ("fmod",      2, "pair",    math.fmod,               0),
    ("remainder", 2, "pair",    math.remainder,          0),
    ("copysign",  2, "pair",    math.copysign,           0),
    ("exp",       1, "expd",    math.exp,                2),
    ("exp2",      1, "exp2d",   math.exp2,               3),
    ("expm1",     1, "expd",    math.expm1,              2),
    ("log",       1, "pos",     math.log,                2),
    ("log2",      1, "pos",     math.log2,               3),
    ("log10",     1, "pos",     math.log10,              3),
    ("log1p",     1, "gtm1",    math.log1p,              2),
    ("sin",       1, "trig",    math.sin,                2),
    ("cos",       1, "trig",    math.cos,                2),
    ("tan",       1, "trig",    math.tan,                2),
    ("asin",      1, "unit",    math.asin,               2),
    ("acos",      1, "unit",    math.acos,               2),
    ("atan",      1, "any",     math.atan,               2),
    ("atan2",     2, "pair",    math.atan2,              3),
    ("sinh",      1, "hyp",     math.sinh,               4),
    ("cosh",      1, "hyp",     math.cosh,               3),
    ("tanh",      1, "any",     math.tanh,               4),
    ("asinh",     1, "any",     math.asinh,              3),
    ("acosh",     1, "ge1",     math.acosh,              3),
    ("atanh",     1, "unit",    math.atanh,              3),
    ("cbrt",      1, "any",     math.cbrt,               2),
    ("hypot",     2, "pair",    math.hypot,              2),
    ("pow",       2, "powd",    math.pow,                2),
]

def _gen_arg(rnd, dom):
    def mk(e, neg=None):
        m = rnd.random() + 1.0
        v = m * (2.0 ** e)
        return -v if (rnd.random() < 0.5 if neg is None else neg) else v
    if dom == "pos": return mk(rnd.randrange(-60, 60), False)
    if dom == "any": return mk(rnd.randrange(-20, 40))
    if dom == "pair": return mk(rnd.randrange(-10, 30))
    if dom == "expd": return rnd.uniform(-700, 700)
    if dom == "exp2d": return rnd.uniform(-1000, 1000)
    if dom == "gtm1": return rnd.choice((rnd.uniform(-0.999, 0.0), mk(rnd.randrange(-30, 20), False)))
    if dom == "trig": return mk(rnd.randrange(-20, 30))
    if dom == "unit": return rnd.uniform(-1.0, 1.0)
    if dom == "hyp": return rnd.uniform(-700, 700)
    if dom == "ge1": return 1.0 + mk(rnd.randrange(-20, 40), False)
    if dom == "powd": return mk(rnd.randrange(-20, 20), False)
    raise ValueError(dom)

def math_cases():
    """(function index, a bits, b bits, expected bits, tolerance) for 24 arguments of each function"""
    rnd = random.Random(42)
    out = []
    for fi, (name, nargs, dom, py, tol) in enumerate(MATH_FUNCS):
        n = 0
        while n < 24:
            a = _gen_arg(rnd, dom)
            if nargs == 2:
                b = rnd.uniform(-8, 8) if dom == "powd" else _gen_arg(rnd, dom)
                if name == "remainder" and b == 0: continue
            else: b = 0.0
            try:
                e = float(py(a, b)) if nargs == 2 else float(py(a))
            except (ValueError, OverflowError, ZeroDivisionError):
                continue
            if e != e or math.isinf(e) or (e != 0 and abs(e) < 1e-290): continue
            out.append((fi, d2u(a), d2u(b), d2u(e), tol)); n += 1
    return out

def make_header():
    out = ["/* fpcases.h - made by pack/fpcases.py: answers worked out on the host by glibc and Python */"]
    fc = format_cases()
    out.append("static const struct { const char *fmt; unsigned lo, hi; const char *expect; } fcases[] = {")
    for fmt, bits, exp in fc: out.append("  { %s, 0x%08XU, 0x%08XU, %s }," % (cstr(fmt), bits & 0xFFFFFFFF, bits >> 32, cstr(exp)))
    out.append("};")
    out.append("static const struct { const char *text; unsigned long long dbits; unsigned fbits; int dend, fend, derr, ferr; } pcases[] = {")
    for t in parse_texts():
        db, fb, de, fe, dr, fr_ = glibc_parse(t)
        out.append("  { %s, 0x%016XULL, 0x%08XU, %d, %d, %d, %d }," % (cstr(t), db, fb, de, fe, dr, fr_))
    out.append("};")
    out.append("static const struct { const char *text; unsigned long long dbits; int dend; } hcases[] = {")
    for t in heavy_texts():
        db, fb, de, fe, dr, fr_ = glibc_parse(t)
        out.append("  { %s, 0x%016XULL, %d }," % (cstr(t), db, de))
    out.append("};")
    out.append("static const struct { const char *name; double (*f1) (double); double (*f2) (double, double); } mfuncs[] = {")
    for name, nargs, dom, py, tol in MATH_FUNCS:
        out.append("  { %s, %s, %s }," % (cstr(name), name if nargs == 1 else "0", name if nargs == 2 else "0"))
    out.append("};")
    out.append("static const struct { unsigned char fn; unsigned char tol; unsigned long long a, b, expect; } mcases[] = {")
    for fi, a, b, e, tol in math_cases(): out.append("  { %d, %d, 0x%016XULL, 0x%016XULL, 0x%016XULL }," % (fi, tol, a, b, e))
    out.append("};")
    out.append("#define ARITH_HASH 0x%016XULL" % arith_hash())
    out.append("#define TICK_N 100")
    out.append("#define TICK_BITS 0x%016XULL" % tick_bits(100))
    s1 = "%.1000f" % 1e300; s2 = "%.1100e" % 5e-324
    out.append("#define HEAVY1_LEN %d\n#define HEAVY1_HASH 0x%016XULL" % (len(s1), fnv(s1)))
    out.append("#define HEAVY2_LEN %d\n#define HEAVY2_HASH 0x%016XULL" % (len(s2), fnv(s2)))
    return "\n".join(out) + "\n"

if __name__ == "__main__":
    import sys
    h = make_header()
    open(sys.argv[1] if len(sys.argv) > 1 else "/dev/stdout", "w").write(h)
    print("fpcases.h: %d bytes" % len(h), file=sys.stderr)
