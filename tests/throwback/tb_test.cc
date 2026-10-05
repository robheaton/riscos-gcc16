/* Host test of riscos-throwback.cc: the core (cutting a diagnostic into lines, ASCII, the state machine) and the two transports, without GCC.
   The DDEUtils transport runs against a mock of _kernel_swi that records every call (with the strings its registers point to) and can be told to fail or to
   answer OS_FSControl 37 with "buffer too small"; the syslog transport runs against a real UDP socket on the loopback and its datagrams are parsed the way SysLogD
   parses them.   Build: build-and-run.sh (-DRISCOS_TB_TEST, plus -DRISCOS_TB_TEST_NATIVE for the DDEUtils variant).  The exit status is the number of failed checks.  */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdarg>
#include <map>

#include "../../new-files/gcc/config/arm/riscos-throwback.cc"

extern void riscos_tb_test_report (const char *file, int line, int severity, const char *text);
extern void riscos_tb_test_finish ();
extern void riscos_tb_test_lines (const char *text, std::vector<std::string> &lines);

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf ("FAIL line %d: ", __LINE__); printf (__VA_ARGS__); printf ("\n"); } } while (0)

#ifdef RISCOS_TB_TEST_NATIVE

/* ---- the mock of the kernel ---- */
static std::vector<const void *> ptr_table;
int riscos_tb_test_ptr (const void *p) { ptr_table.push_back (p); return (int) ptr_table.size (); }   /* token = index + 1 */
static const char *deref (int token) { return token >= 1 && token <= (int) ptr_table.size () ? (const char *) ptr_table[token - 1] : NULL; }

struct call { int swi; int r[6]; std::string s1, s2, s5; };
static std::vector<call> calls;
static int fail_swi = -1;               /* fail every call of this SWI number */
static int fail_after = 0;              /* ... after this many successful calls of it */
static int fs_small_first = 0;          /* OS_FSControl 37: answer "too small by N bytes" on the first call */
static std::string riscosify_prefix;    /* what the mock __riscosify_std adds in front of the name */
static bool riscosify_fails = false;
static bool fs_fails = false;

char *
__riscosify_std (const char *name, int, char *buffer, size_t buf_len, int *)
{
  if (riscosify_fails)
    return NULL;
  std::string out = riscosify_prefix + name;
  for (char &c : out) if (c == '/') c = '.';
  snprintf (buffer, buf_len, "%s", out.c_str ());
  return buffer + strlen (buffer);
}

static _kernel_oserror err_buf;
_kernel_oserror *
_kernel_swi (int no, const _kernel_swi_regs *in, _kernel_swi_regs *out)
{
  call c;
  c.swi = no;
  for (int i = 0; i < 6; i++) c.r[i] = in->r[i];
  /* the registers that hold strings: resolve the tokens while the strings are still alive.  */
  if (no == DDEUtils_ThrowbackSend) { const char *a = deref (in->r[2]), *b = deref (in->r[5]); c.s2 = a ? a : ""; c.s5 = b ? b : ""; }
  if (no == OS_FSControl) { const char *a = deref (in->r[1]); c.s1 = a ? a : ""; }
  calls.push_back (c);
  *out = *in;
  if (no == fail_swi && fail_after-- <= 0)
    {
      err_buf.errnum = 0x1e6;
      snprintf (err_buf.errmess, sizeof err_buf.errmess, "SWI &%x failed (mock)", no);
      return &err_buf;
    }
  if (no == OS_FSControl && in->r[0] == 37)
    {
      if (fs_fails) { err_buf.errnum = 1; snprintf (err_buf.errmess, sizeof err_buf.errmess, "no such path"); return &err_buf; }
      char *buf = (char *) deref (in->r[2]);
      std::string full = "ADFS::TestDisc.$.src." + c.s1;
      int size = in->r[5];
      if (fs_small_first > 0)
        {
          out->r[5] = -fs_small_first;           /* too small by that many bytes (what the SWI would say) */
          fs_small_first = 0;
          return NULL;
        }
      if ((int) full.size () + 1 > size) { out->r[5] = size - (int) full.size () - 1; return NULL; }
      memcpy (buf, full.c_str (), full.size () + 1);
      out->r[5] = size - (int) full.size () - 1;
    }
  return NULL;
}

static void reset_mock ()
{
  calls.clear (); ptr_table.clear (); fail_swi = -1; fail_after = 0; fs_small_first = 0; riscosify_prefix = ""; riscosify_fails = false; fs_fails = false;
  riscos_tb::channel::instance ().set_transport (new riscos_tb::ddeutils_transport);
}

static std::vector<call> only (int swi)
{
  std::vector<call> v;
  for (const call &c : calls) if (c.swi == swi) v.push_back (c);
  return v;
}

static void test_native ()
{
  using namespace riscos_tb;

  /* 1. two errors in one file, a warning in another, an information line back in the first; then the end.  */
  reset_mock ();
  riscos_tb_test_report ("hello.c", 12, SEV_ERROR, "expected ';' before '}' token");
  riscos_tb_test_report ("hello.c", 20, SEV_WARNING, "unused variable 'x' [-Wunused-variable]");
  riscos_tb_test_report ("util.h", 3, SEV_INFORMATION, "declared here");
  riscos_tb_test_report ("hello.c", 21, SEV_SERIOUS_ERROR, "internal compiler error");
  riscos_tb_test_finish ();
  std::vector<call> sends = only (DDEUtils_ThrowbackSend);
  CHECK (calls[0].swi == DDEUtils_ThrowbackStart, "the first SWI is ThrowbackStart (%x)", calls[0].swi);
  CHECK (only (DDEUtils_ThrowbackStart).size () == 1, "ThrowbackStart once");
  CHECK (calls.back ().swi == DDEUtils_ThrowbackEnd, "the last SWI is ThrowbackEnd");
  CHECK (only (DDEUtils_ThrowbackEnd).size () == 1, "ThrowbackEnd once");
  /* expected sends: P(hello) E12 W20 | P(util) I3 | P(hello) S21 */
  CHECK (sends.size () == 7, "7 ThrowbackSend calls, got %d", (int) sends.size ());
  if (sends.size () == 7)
    {
      CHECK (sends[0].r[0] == 0 && sends[0].s2 == "ADFS::TestDisc.$.src.hello.c", "processing hello.c: R0=%d name '%s'", sends[0].r[0], sends[0].s2.c_str ());
      CHECK (sends[1].r[0] == 1 && sends[1].r[1] == 0 && sends[1].s2 == "ADFS::TestDisc.$.src.hello.c" && sends[1].r[3] == 12 && sends[1].r[4] == 1 && sends[1].s5 == "expected ';' before '}' token",
	     "error details: R0=%d R1=%d name '%s' line %d sev %d '%s'", sends[1].r[0], sends[1].r[1], sends[1].s2.c_str (), sends[1].r[3], sends[1].r[4], sends[1].s5.c_str ());
      CHECK (sends[2].r[0] == 1 && sends[2].r[3] == 20 && sends[2].r[4] == 0 && sends[2].s5 == "unused variable 'x' [-Wunused-variable]", "warning details");
      CHECK (sends[3].r[0] == 0 && sends[3].s2 == "ADFS::TestDisc.$.src.util.h", "processing util.h");
      CHECK (sends[4].r[0] == 2 && sends[4].r[3] == 3 && sends[4].r[4] == 0 && sends[4].s5 == "declared here", "information details: R0=%d R4=%d", sends[4].r[0], sends[4].r[4]);
      CHECK (sends[5].r[0] == 0 && sends[5].s2 == "ADFS::TestDisc.$.src.hello.c", "processing hello.c again after a message in another file");
      CHECK (sends[6].r[0] == 1 && sends[6].r[4] == 2, "serious error: severity 2");
    }

  /* 2. the same file again and again: only one "processing" message.  */
  reset_mock ();
  for (int i = 0; i < 5; i++) riscos_tb_test_report ("a.c", i + 1, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  sends = only (DDEUtils_ThrowbackSend);
  int processing = 0; for (const call &c : sends) processing += c.r[0] == 0;
  CHECK (sends.size () == 6 && processing == 1, "one processing message for 5 errors in one file: %d sends, %d processing", (int) sends.size (), processing);

  /* 3. a message of several lines: the first line has the severity, the others are information.  */
  reset_mock ();
  riscos_tb_test_report ("a.c", 7, SEV_ERROR, "no match for call\n  candidate: f(int)\n\n  candidate: f(long)  ");
  riscos_tb_test_finish ();
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () == 4, "processing + 3 lines, got %d", (int) sends.size ());
  if (sends.size () == 4)
    {
      CHECK (sends[1].r[0] == 1 && sends[1].r[4] == 1 && sends[1].s5 == "no match for call", "first line");
      CHECK (sends[2].r[0] == 2 && sends[2].r[4] == 0 && sends[2].s5 == "candidate: f(int)", "second line is information, trimmed: '%s'", sends[2].s5.c_str ());
      CHECK (sends[3].r[0] == 2 && sends[3].s5 == "candidate: f(long)", "third line, the empty one is dropped: '%s'", sends[3].s5.c_str ());
      CHECK (sends[2].r[3] == 7 && sends[3].r[3] == 7, "all lines have the line number of the diagnostic");
    }

  /* 4. things that are not files, and line 0 / negative lines.  */
  reset_mock ();
  riscos_tb_test_report ("<built-in>", 1, SEV_WARNING, "w");
  riscos_tb_test_report ("<stdin>", 1, SEV_WARNING, "w");
  riscos_tb_test_report ("", 1, SEV_WARNING, "w");
  CHECK (calls.empty (), "no SWI for <built-in>, <stdin> and empty names (%d calls)", (int) calls.size ());
  riscos_tb_test_report ("a.c", -3, SEV_WARNING, "w");
  riscos_tb_test_report ("a.c", 0, SEV_WARNING, "w");
  riscos_tb_test_finish ();
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () == 3 && sends[1].r[3] == 0 && sends[2].r[3] == 0, "negative and zero line numbers go out as 0");

  /* 5. DDEUtils not loaded: ThrowbackStart fails -> nothing else is called, ever.  */
  reset_mock ();
  fail_swi = DDEUtils_ThrowbackStart;
  riscos_tb_test_report ("a.c", 1, SEV_ERROR, "e");
  riscos_tb_test_report ("a.c", 2, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  CHECK (calls.size () == 1 && calls[0].swi == DDEUtils_ThrowbackStart, "after a failed Start no other SWI is called (%d calls)", (int) calls.size ());

  /* 6. a failing Send switches throwback off; no End is needed for a Start that ... (End is still sent: Start worked).  */
  reset_mock ();
  fail_swi = DDEUtils_ThrowbackSend; fail_after = 2;
  for (int i = 0; i < 6; i++) riscos_tb_test_report ("a.c", i + 1, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () == 3, "three Sends (the third fails), then no more: %d", (int) sends.size ());
  CHECK (only (DDEUtils_ThrowbackEnd).size () == 0, "after a failure nothing more is sent, not even the End (the transport is off)");

  /* 7. file name conversion: riscosify fails -> the name goes on as it is; canonicalise fails -> the converted name.  */
  reset_mock ();
  riscosify_fails = true;
  riscos_tb_test_report ("src/a.c", 1, SEV_ERROR, "e");
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () >= 1 && sends[0].s2 == "src/a.c", "riscosify fails: the original name '%s'", sends.empty () ? "" : sends[0].s2.c_str ());
  reset_mock ();
  riscosify_prefix = "SDFS::X.$.";
  fs_fails = true;
  riscos_tb_test_report ("src/a.c", 1, SEV_ERROR, "e");
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () >= 1 && sends[0].s2 == "SDFS::X.$.src.a.c", "OS_FSControl fails: the converted name '%s'", sends.empty () ? "" : sends[0].s2.c_str ());

  /* 8. OS_FSControl 37 says the buffer is too small: it is called again with a bigger one.  */
  reset_mock ();
  fs_small_first = 5000;
  riscos_tb_test_report ("a.c", 1, SEV_ERROR, "e");
  std::vector<call> fs = only (OS_FSControl);
  CHECK (fs.size () == 2 && fs[1].r[5] > fs[0].r[5], "a second OS_FSControl with a bigger buffer (%d calls, sizes %d, %d)", (int) fs.size (), fs.empty () ? 0 : fs[0].r[5], fs.size () > 1 ? fs[1].r[5] : 0);
  sends = only (DDEUtils_ThrowbackSend);
  CHECK (sends.size () >= 1 && sends[0].s2 == "ADFS::TestDisc.$.src.a.c", "the canonical name arrives: '%s'", sends.empty () ? "" : sends[0].s2.c_str ());
  CHECK (fs.size () >= 1 && fs[0].r[0] == 37 && fs[0].s1 == "a.c", "OS_FSControl 37 is called with the converted name '%s'", fs.empty () ? "" : fs[0].s1.c_str ());
}

#endif /* RISCOS_TB_TEST_NATIVE */

/* ---- the text cutting, the same for both ---- */
static void test_lines ()
{
  std::vector<std::string> l;

  riscos_tb_test_lines ("plain text", l);
  CHECK (l.size () == 1 && l[0] == "plain text", "plain");

  l.clear (); riscos_tb_test_lines ("with \xe2\x80\x98" "quotes\xe2\x80\x99 and \xe2\x80\x9c" "double\xe2\x80\x9d \xe2\x80\x93 dash \xe2\x80\xa6 dots \xe2\x80\xa2 bullet \xc2\xa0" "nbsp", l);
  CHECK (l.size () == 1 && l[0] == "with 'quotes' and \"double\" - dash ... dots * bullet  nbsp", "typographic characters: '%s'", l.empty () ? "" : l[0].c_str ());

  l.clear (); riscos_tb_test_lines ("caf\xc3\xa9" " \xe4\xb8\xad \xf0\x9f\x98\x80 end", l);
  CHECK (l.size () == 1 && l[0] == "caf? ? ? end", "other characters become one '?' each: '%s'", l.empty () ? "" : l[0].c_str ());

  l.clear (); riscos_tb_test_lines ("tab\there\x01\x7f" "x\r\ny", l);
  CHECK (l.size () == 2 && l[0] == "tab here ?x" && l[1] == "y", "control characters: '%s' '%s'", l.size () > 0 ? l[0].c_str () : "", l.size () > 1 ? l[1].c_str () : "");

  l.clear (); riscos_tb_test_lines ("a stray lead byte \xe2 and a truncated one \xe2\x80", l);
  CHECK (l.size () == 1 && l[0] == "a stray lead byte ? and a truncated one ?", "invalid UTF-8: '%s'", l.empty () ? "" : l[0].c_str ());

  /* a long text breaks at the last space before 220, every line is at most 220 characters.  */
  std::string longtext;
  for (int i = 0; i < 40; i++) longtext += "word" + std::to_string (i) + " ";
  l.clear (); riscos_tb_test_lines (longtext.c_str (), l);
  bool ok = !l.empty ();
  std::string joined;
  for (const std::string &s : l) { ok = ok && s.size () <= 220 && !s.empty () && s[0] != ' ' && s.back () != ' '; joined += (joined.empty () ? "" : " ") + s; }
  CHECK (ok && l.size () >= 2, "long text: %d lines, all <= 220, trimmed", (int) l.size ());
  std::string expect = longtext; while (!expect.empty () && expect.back () == ' ') expect.pop_back ();
  CHECK (joined == expect, "long text: nothing lost when it is cut at spaces");

  /* one word longer than a line is cut at 220.  */
  l.clear (); riscos_tb_test_lines (std::string (500, 'x').c_str (), l);
  CHECK (l.size () == 3 && l[0].size () == 220 && l[1].size () == 220 && l[2].size () == 60, "a 500 character word: %d lines (%d, %d)", (int) l.size (), l.size () > 0 ? (int) l[0].size () : 0, l.size () > 1 ? (int) l[1].size () : 0);

  /* at most 6 lines; the sixth ends in "...".  */
  std::string many;
  for (int i = 0; i < 20; i++) many += "line" + std::to_string (i) + "\n";
  l.clear (); riscos_tb_test_lines (many.c_str (), l);
  CHECK (l.size () == 6 && l[5] == "line5...", "20 lines are cut to 6 with '...': %d, '%s'", (int) l.size (), l.size () > 5 ? l[5].c_str () : "");
  l.clear (); riscos_tb_test_lines (std::string (220 * 8, 'y').c_str (), l);
  CHECK (l.size () == 6 && l[5].size () <= 220 && l[5].substr (l[5].size () - 3) == "...", "an overlong word run: 6 lines, the last <= 220 ending in '...'");

  /* exactly 6 lines: no "...".  */
  l.clear (); riscos_tb_test_lines ("1\n2\n3\n4\n5\n6", l);
  CHECK (l.size () == 6 && l[5] == "6", "exactly 6 lines are not cut");

  l.clear (); riscos_tb_test_lines ("", l);
  CHECK (l.empty (), "empty text: no lines");
  l.clear (); riscos_tb_test_lines (" \n \t \n", l);
  CHECK (l.empty (), "white space only: no lines");
}

#ifndef RISCOS_TB_TEST_NATIVE

/* ---- the syslog transport against a real UDP socket ---- */

/* The parser of SysLogD (gcc4/riscos/syslogd/main.c), copied in structure: "<pri>" first, then "throwback " somewhere, then FILE ':' LINE ':' MSG.  */
struct parsed { int pri; std::string file; int line; std::string msg; bool ok; };
static parsed parse_like_syslogd (const std::string &dg)
{
  parsed p; p.ok = false; p.pri = -1; p.line = -1;
  size_t from = 0;
  if (dg.size () > 1 && dg[0] == '<')
    {
      size_t i = 1;
      while (i < dg.size () && isdigit ((unsigned char) dg[i])) i++;
      if (i < dg.size () && dg[i] == '>') { p.pri = atoi (dg.substr (1, i - 1).c_str ()); from = i + 1; }
    }
  size_t tag = dg.find ("throwback ", from);
  if (tag == std::string::npos) return p;
  std::string rest = dg.substr (tag + strlen ("throwback"));
  size_t k = 0; while (k < rest.size () && isspace ((unsigned char) rest[k])) k++;
  size_t colon1 = rest.find (':', k);
  if (colon1 == std::string::npos) return p;
  p.file = rest.substr (k, colon1 - k);
  size_t colon2 = rest.find (':', colon1 + 1);
  if (colon2 == std::string::npos) return p;
  p.line = atoi (rest.substr (colon1 + 1, colon2 - colon1 - 1).c_str ());
  size_t m = colon2 + 1; while (m < rest.size () && isspace ((unsigned char) rest[m])) m++;
  p.msg = rest.substr (m);
  p.ok = true;
  return p;
}

static int rx = -1;
static std::vector<std::string> receive_all ()
{
  std::vector<std::string> v;
  char buf[2048];
  for (;;)
    {
      ssize_t n = recv (rx, buf, sizeof buf, MSG_DONTWAIT);
      if (n < 0) break;
      v.push_back (std::string (buf, n));
    }
  return v;
}

static void reset_syslog ()
{
  riscos_tb::channel::instance ().set_transport (new riscos_tb::syslog_transport);
}

static void test_syslog ()
{
  using namespace riscos_tb;

  rx = socket (AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in a; memset (&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl (INADDR_LOOPBACK); a.sin_port = 0;
  CHECK (bind (rx, (struct sockaddr *) &a, sizeof a) == 0, "bind");
  socklen_t al = sizeof a; getsockname (rx, (struct sockaddr *) &a, &al);
  char port[16]; snprintf (port, sizeof port, "%d", ntohs (a.sin_port));
  setenv ("THROWBACK_HOST", "127.0.0.1", 1);
  setenv ("THROWBACK_PORT", port, 1);

  /* severities, line numbers, text; the file name is made absolute (realpath) for a file that exists.  */
  FILE *f = fopen ("tb_test_exists.c", "w"); if (f) fclose (f);
  char cwd[1024]; if (!getcwd (cwd, sizeof cwd)) strcpy (cwd, ".");
  reset_syslog ();
  riscos_tb_test_report ("tb_test_exists.c", 12, SEV_ERROR, "expected ';' before '}' token");
  riscos_tb_test_report ("tb_test_exists.c", 20, SEV_WARNING, "unused variable 'x' [-Wunused-variable]");
  riscos_tb_test_report ("no/such/file.h", 3, SEV_INFORMATION, "declared here");
  riscos_tb_test_report ("tb_test_exists.c", 21, SEV_SERIOUS_ERROR, "internal compiler error");
  riscos_tb_test_finish ();
  std::vector<std::string> dg = receive_all ();
  CHECK (dg.size () == 4, "4 datagrams, got %d", (int) dg.size ());
  if (dg.size () == 4)
    {
      parsed p0 = parse_like_syslogd (dg[0]), p1 = parse_like_syslogd (dg[1]), p2 = parse_like_syslogd (dg[2]), p3 = parse_like_syslogd (dg[3]);
      std::string abs = std::string (cwd) + "/tb_test_exists.c";
      CHECK (p0.ok && p0.pri == 11 && p0.line == 12 && p0.file == abs && p0.msg == "expected ';' before '}' token", "error datagram: '%s'", dg[0].c_str ());
      CHECK (p1.ok && p1.pri == 12 && p1.line == 20 && p1.msg == "unused variable 'x' [-Wunused-variable]", "warning datagram: '%s'", dg[1].c_str ());
      CHECK (p2.ok && p2.pri == 14 && p2.line == 3 && p2.file == "no/such/file.h" && p2.msg == "declared here", "information datagram, a file that does not exist keeps its name: '%s'", dg[2].c_str ());
      CHECK (p3.ok && p3.pri == 11 && p3.line == 21, "serious error goes out as an error: '%s'", dg[3].c_str ());
      CHECK (dg[0].compare (0, 4, "<11>") == 0 && dg[0].find (" throwback ") != std::string::npos, "the syslog layout: '%s'", dg[0].c_str ());
    }
  unlink ("tb_test_exists.c");

  /* a long message is cut into lines, each a datagram, and every datagram is below 1000 bytes.  */
  reset_syslog ();
  std::string longtext; for (int i = 0; i < 100; i++) longtext += "word" + std::to_string (i) + " ";
  riscos_tb_test_report ("a.c", 5, SEV_ERROR, longtext.c_str ());
  riscos_tb_test_finish ();
  dg = receive_all ();
  bool small = !dg.empty ();
  for (const std::string &d : dg) small = small && d.size () <= 1000 && parse_like_syslogd (d).ok;
  CHECK (small && dg.size () >= 3, "a long message: %d datagrams, all parseable and small", (int) dg.size ());

  /* a very long file name: the datagram is still cut to 1000 bytes (SysLogD reads 1024).  */
  reset_syslog ();
  riscos_tb_test_report ((std::string (1200, 'p') + ".c").c_str (), 5, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  dg = receive_all ();
  CHECK (dg.size () == 1 && dg[0].size () <= 1000, "a 1200 character file name: the datagram is cut to 1000 bytes (%d)", dg.empty () ? -1 : (int) dg[0].size ());

  /* a message with a ':' in the text still parses (the text is after the second colon).  */
  reset_syslog ();
  riscos_tb_test_report ("a.c", 5, SEV_ERROR, "note: a: b: c");
  riscos_tb_test_finish ();
  dg = receive_all ();
  CHECK (dg.size () == 1 && parse_like_syslogd (dg[0]).msg == "note: a: b: c", "colons in the text");

  /* THROWBACK_HOST not set: nothing happens, nothing crashes.  */
  unsetenv ("THROWBACK_HOST");
  reset_syslog ();
  riscos_tb_test_report ("a.c", 5, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  dg = receive_all ();
  CHECK (dg.empty (), "no THROWBACK_HOST, no datagram");

  /* an unknown host: switched off.  */
  setenv ("THROWBACK_HOST", "no-such-host.invalid", 1);
  reset_syslog ();
  riscos_tb_test_report ("a.c", 5, SEV_ERROR, "e");
  riscos_tb_test_finish ();
  dg = receive_all ();
  CHECK (dg.empty (), "an unknown host, no datagram");
  close (rx);
}

#endif /* !RISCOS_TB_TEST_NATIVE */

int main ()
{
  test_lines ();
#ifdef RISCOS_TB_TEST_NATIVE
  test_native ();
#else
  test_syslog ();
#endif
  printf ("%s: %d checks, %d failed\n",
#ifdef RISCOS_TB_TEST_NATIVE
	  "DDEUtils transport (mock kernel)",
#else
	  "syslog transport (UDP on the loopback)",
#endif
	  checks, fails);
  return fails != 0;
}
