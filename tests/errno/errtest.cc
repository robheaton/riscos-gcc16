// errtest.cc - the seven POSIX error numbers that 16.2.0-22 adds to UnixLib (runtime 16.2.0-14, fix level 16) and to the C++ runtime (16.2.0-6).
// Part 1 (UnixLib): the numbers 92 - 98, their strerror () texts, sys_nerr, and an unknown number.  Part 2 (libstdc++): std::system_category ().default_error_condition ()
// answers the generic category for six of these numbers since 16.2.0-6 (the older runtime did not know any of them: system category); EMULTIHOP stays in the system category on
// purpose: C++'s std::errc has no multihop error, so libstdc++ does not list it.  message () gives the same texts for all seven.
// exit status 0 = everything as expected.
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <unistd.h>

struct E { int num; const char *name; const char *text; bool generic; };    // generic: the category that libstdc++ 16.2.0-6 gives the condition
static const E codes[] = {
  { EPROTO, "EPROTO", "Protocol error", true }, { ENODATA, "ENODATA", "No data available", true }, { ENOLINK, "ENOLINK", "Link has been severed", true },
  { EMULTIHOP, "EMULTIHOP", "Multihop attempted", false }, { ECANCELED, "ECANCELED", "Operation canceled", true },
  { ENOTRECOVERABLE, "ENOTRECOVERABLE", "State not recoverable", true }, { EOWNERDEAD, "EOWNERDEAD", "Owner died", true } };

int main ()
{
  int bad = 0, cbad = 0;
  long fix = sysconf (0x4700);
  std::printf ("errtest: the runtime answers fix level %ld (16 or more has the new numbers)\n", fix);
  std::printf ("sys_nerr = %d (expect 99)\n", sys_nerr);
  if (sys_nerr != 99) bad++;
  for (const E &e : codes)
    {
      const char *s = std::strerror (e.num);
      bool ok = std::strcmp (s, e.text) == 0;
      std::printf ("  %-16s %2d  strerror '%s'  %s\n", e.name, e.num, s, ok ? "ok" : "WRONG");
      if (!ok) bad++;
    }
  if (EPROTO != 92 || ENODATA != 93 || ENOLINK != 94 || EMULTIHOP != 95 || ECANCELED != 96 || ENOTRECOVERABLE != 97 || EOWNERDEAD != 98) { std::printf ("  the numbers are not 92 - 98\n"); bad++; }
  errno = 0;
  const char *u = std::strerror (99);
  bool uk = std::strcmp (u, "Unknown Error") == 0 && errno == EINVAL;
  std::printf ("  strerror (99): '%s' errno %d  %s\n", u, errno, uk ? "ok" : "WRONG");
  if (!uk) bad++;
  std::printf ("  strerror (91): '%s' (EOVERFLOW, unchanged)  %s\n", std::strerror (91), std::strcmp (std::strerror (91), "Value too large for defined data type") == 0 ? "ok" : "WRONG");
  if (std::strcmp (std::strerror (91), "Value too large for defined data type")) bad++;
  std::printf ("UnixLib: %s\n", bad ? "FAIL" : "PASS");
  for (const E &e : codes)
    {
      std::error_condition c = std::system_category ().default_error_condition (e.num);
      bool generic = c.category () == std::generic_category () && c.value () == e.num;
      bool expected = generic == e.generic;                    // EMULTIHOP: the system category is the right answer
      std::string m = std::system_category ().message (e.num);
      bool mok = m == e.text;
      std::printf ("  %-16s system_category: condition in the %s category, message '%s'  %s\n", e.name, generic ? "generic" : "system", m.c_str (), expected && mok ? "ok" : "WRONG");
      if (!expected || !mok) cbad++;
    }
  std::printf ("libstdc++: %s%s\n", cbad ? "FAIL" : "PASS", cbad ? " (a SharedLibs-C++-armeabihf older than 16.2.0-6 does not know these numbers: all seven are then in the system category)" : "");
  std::printf ("errtest: %s\n", bad || cbad ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
  return bad || cbad ? 1 : 0;
}
