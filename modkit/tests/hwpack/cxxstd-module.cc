/* cxxstd-module.cc - tests/cxx/cxxstd.cc as a module (CMHG: cxxstd.cmhg).  The text that the test prints goes into a buffer (cap_printf) and is compared with expected.h, the text that the host printed
   (the first line, from the static constructor, and the last, from the destructor, are not part of it: the module's veneers run those at load and at kill).  *CxxStd_Test says how many lines differ and
   shows the first difference; with -v it shows all the text. */
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <kernel.h>
#include "header.h"

static char capture[8192];
static unsigned cap_len;
static int cap_printf (const char *fmt, ...)
{
  va_list ap;
  int n;
  va_start (ap, fmt);
  n = vsnprintf (capture + cap_len, sizeof capture - cap_len, fmt, ap);
  va_end (ap);
  if (n > 0) cap_len = cap_len + (unsigned) n < sizeof capture ? cap_len + (unsigned) n : (unsigned) sizeof capture - 1;
  return n;
}
#define printf cap_printf
#define CXXSTD_AS_MODULE 1
#include "cxxstd.cc"
#undef printf

static const char expected[] =
#include "expected.h"
;

extern "C" _kernel_oserror *cxxstd_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; return 0; }
extern "C" _kernel_oserror *cxxstd_final (int fatal, int podule_base, void *pw) { (void) fatal; (void) podule_base; (void) pw; return 0; }
extern "C" _kernel_oserror *cxxstd_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) pw;
  if (number == CMD_CxxStd_Info) { std::printf ("CxxStd_Info: the test text so far %u bytes\n", cap_len); return 0; }
  bool verbose = argc && arg_string[0] == '-' && arg_string[1] == 'v';
  cap_len = 0; capture[0] = 0;
  cxxstd_main ();
  const char *a = capture, *b = expected;
  int line = 1, bad = 0, total = 0;
  while (*a || *b)                                                            // compare line by line
    {
      const char *ea = std::strchr (a, '\n'), *eb = std::strchr (b, '\n');
      unsigned la = ea ? (unsigned) (ea - a) : (unsigned) std::strlen (a), lb = eb ? (unsigned) (eb - b) : (unsigned) std::strlen (b);
      total++;
      if (la != lb || std::memcmp (a, b, la))
        {
          if (!bad) std::printf ("CxxStd_Test: line %d differs:\n  module: %.*s\n  host:   %.*s\n", line, (int) la, a, (int) lb, b);
          bad++;
        }
      a += la + (ea ? 1 : 0); b += lb + (eb ? 1 : 0); line++;
      if (!ea && !eb) break;
    }
  if (verbose) std::printf ("%s", capture);
  std::printf ("CxxStd_Test: %d lines, %d differ: %s\n", total, bad, bad ? "FAILED" : "ok");
  return 0;
}
