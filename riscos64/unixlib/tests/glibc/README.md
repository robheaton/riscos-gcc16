# Tests compared with glibc (Phase 2: iconv, locales, collation, strftime)

Each program prints text that must be identical when it runs on the host with glibc and on RISC OS 64 with UnixLib 64 (Pyromaniac):

| Program | What | Run as | Known differences |
|---|---|---|---|
| `t_iconv.c` | about 1.4 million conversions between UTF-8, UTF-16/32, UCS-2/4, ASCII, Latin-1 and 23 8-bit sets; prints one hash per section (add any argument to print every line) | no argument | none |
| `t_lang.c` | `setlocale`, `nl_langinfo` (all items), `localeconv`, `strftime` (`%c %x %X %r` and the names), `strcoll` | `t_lang LOCALE` | `frac_digits` etc. print 127 for "unset" on x86 and are mapped in the program |
| `t_coll.c` | 190 strings sorted with `strcoll` (accents, case, punctuation, digits, Greek, Cyrillic, CJK), `strxfrm` consistency | `t_coll LOCALE` | root order only: sv, da, nb, fi, pl, cs, hu, tr, es (`ñ`), ca, cy, fr_CA differ (tailored orders are not applied); invalid UTF-8 bytes and Hangul compare equal to nothing (glibc: ordering of ties differs) |
| `t_setlocale.c` | per-category `setlocale`, composite names, environment variables, `strftime` flags/widths, `strptime` with locale formats | no argument | glibc's answers for `setlocale (LC_ALL, "")` with several `LC_*` variables are not reproduced (UnixLib 64 follows the usual rules) |
| `t_printf.c` | `printf` with LC_NUMERIC: the decimal point and the `'` flag (ints, floats, long long, long double, widths, zero padding, precision) | `t_printf LOCALE` | picolibc prints at most 17 significant digits (`1e100`: not in the test) ; `%5%` |
| `t_strftime.c` | the glibc flags of `strftime`: `%-d %_H %05d %^a %10A %e %k %l %s %G %V` ... | no argument | none |

The locale data (`sdk/crt/posix/locale_tab.h`) comes from `tools/gen-locale-data.sh` (glibc's locales, 36 of them), the collation weights (`collate_tab.h`) from `tools/gen-collate-data.py`
(glibc's `iso14651_t1_common`), the 8-bit character sets of `iconv` (`iconv_tables.h`) from `tools/gen-iconv-tables.py` (Python's codecs).
To compare: build the program with `gcc` (host) and with `aarch64-riscos-gcc -munixlib`, run both (the host with `LOCPATH` pointing to locales made with `localedef` if they are not installed) and `diff` the output.
