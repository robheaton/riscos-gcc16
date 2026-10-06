/* Throwback of the compiler's errors and warnings to a RISC OS text editor (-mthrowback).
   Written for the GCCSDK GCC 16 forward port; the idea, the SWI protocol and the syslog protocol of the host variant are those of the GCC 10 riscos.c
   by Nick Burrett, Alex Waugh and John Tytgat, whose diagnostic hooks do not exist any more in GCC 16.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

/* What this does.

   With -mthrowback every diagnostic that has a source location (an error, a warning, a note; not the ones about the command line) is also sent to the
   text editor that the user is working in, which shows it in a list from which the line can be opened.  It is an additional output sink of the diagnostic
   machinery (diagnostics::sink), so the ordinary text on stderr is unchanged, and every front end that reports through global_dc (C, C++, Fortran, LTO)
   gets it without any change.  arm_option_override calls riscos_throwback_init () when -mthrowback is given.

   Two transports, chosen when the compiler is built:

   * The compiler runs on RISC OS (CROSS_DIRECTORY_STRUCTURE is not defined): the SWIs of the DDEUtils module, the way every RISC OS compiler and assembler does
     it.  DDEUtils_ThrowbackStart once, then for each message DDEUtils_ThrowbackSend (reason 0: "processing this file", whenever the file changes; reason 1:
     error or warning, reason 2: information) and DDEUtils_ThrowbackEnd when the compiler ends.  The editor must have registered with DDEUtils (StrongED, Zap
     and !Edit do); if DDEUtils is not loaded, or the SWI fails for any other reason, throwback is switched off silently for the rest of the run.
   * The compiler is a cross compiler: one UDP datagram in syslog format per message, to the host named by the environment variable THROWBACK_HOST (port 514,
     or THROWBACK_PORT).  The SysLogD module of the GCCSDK, running on the RISC OS machine, turns them into DDEUtils throwback, mapping the Unix path names to the
     RISC OS ones (see its ReadMe).

   What DDEUtils (ROOL source, Sources/Programmer/DDEUtils) does with the calls: all of them fail with an error outside the desktop ("Throwback not available
   outside the desktop") and when no editor has registered itself ("No task registered for throwback"; only one task can be registered); ThrowbackStart and
   ThrowbackEnd send the Wimp messages &42580 and &42584 to that task; ThrowbackSend reason 0 sends &42581 (processing this file: R2 = name), reasons 1 and 2 send
   &42582 / &42585 (error / information in this file: R2 = name) and then &42583 / &42586 (the details: line number, severity, text), so a name goes with every error.

   The same file name is sent only once per run of messages (a "processing" message each time the file changes).  A message is cut into lines: the Wimp message that
   carries it has room for about 200 characters, and DDEUtils uses a control character as the end of the text.  Anything that is not plain ASCII is replaced.

   Set THROWBACK_DEBUG (to anything) to see on stderr why throwback does not work.  */

#ifdef RISCOS_TB_TEST
# define RISCOS_TB_STANDALONE 1
#endif

#ifdef RISCOS_TB_STANDALONE
/* Built without GCC, only the core and the transports: by the host test (RISCOS_TB_TEST, tests/throwback: the kernel is a mock), and on RISC OS by the test
   program of the hardware pack (RISCOS_TB_STANDALONE alone: the real kernel).  */
# include <algorithm>
# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <memory>
# include <string>
# include <vector>
# include <unistd.h>
#else
# include "config.h"
# define INCLUDE_MEMORY
# define INCLUDE_STRING
# define INCLUDE_VECTOR
# include "system.h"
# include "coretypes.h"
# include "tm.h"
# include "tree.h"
# include "langhooks.h"
# include "diagnostic.h"
# include "diagnostics/sink.h"
# include "diagnostics/buffering.h"
# include "diagnostics/dumping.h"
#endif

#if !defined (CROSS_DIRECTORY_STRUCTURE) || defined (RISCOS_TB_TEST_NATIVE)
# define RISCOS_TB_NATIVE 1
# include <kernel.h>
# include <swis.h>
# include <unixlib/local.h>
/* A pointer in a register of a SWI.  (The host test is not 32 bit: it keeps the pointers in a table.)  */
# ifdef RISCOS_TB_TEST
extern int riscos_tb_test_ptr (const void *);
#  define TB_REG(p) riscos_tb_test_ptr (p)
# else
#  define TB_REG(p) ((int) (p))
# endif
#else
# include <netdb.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <limits.h>
# include <time.h>
#endif

namespace riscos_tb {

/* The severities of DDEUtils.  */
enum severity
{
  SEV_INFORMATION = -1,
  SEV_WARNING = 0,
  SEV_ERROR = 1,
  SEV_SERIOUS_ERROR = 2
};

/* One diagnostic: where it is, how bad, and what it says.  */
struct entry
{
  std::string m_file;
  int m_line;
  int m_severity;
  std::string m_text;
};

/* The longest text that is sent in one message, and the most lines a diagnostic is allowed (more are cut off with "...").  DDEUtils puts the text into a Wimp
   message of at most 256 bytes (the header and the line number and severity use 28 of them, and the text is cut off at 227 characters without a warning).  */
static const size_t max_line = 220;
static const size_t max_lines = 6;

static bool
debug_p ()
{
  return getenv ("THROWBACK_DEBUG") != NULL;
}

/* Append the character at P (a UTF-8 sequence) to OUT as plain ASCII, and move P past it.  Typographic quotes and dashes become what they look like; any other
   character that is not ASCII becomes '?'; the control characters become a space (DDEUtils ends the text at one), except the newline, which is kept.  */
static void
append_ascii (std::string &out, const unsigned char *&p)
{
  unsigned char c = *p++;
  if (c < 0x80)
    {
      out += (c == '\n' || c >= 32) ? (char) c : ' ';
      if (c == 127)
	out[out.size () - 1] = '?';
      return;
    }

  int extra = (c >= 0xf0) ? 3 : (c >= 0xe0) ? 2 : (c >= 0xc0) ? 1 : 0;
  unsigned long cp = (extra == 3) ? (c & 0x07) : (extra == 2) ? (c & 0x0f) : (extra == 1) ? (c & 0x1f) : 0;
  for (int i = 0; i < extra && (*p & 0xc0) == 0x80; i++)
    cp = (cp << 6) | (*p++ & 0x3f);

  switch (cp)
    {
    case 0x2018: case 0x2019: case 0x201a: case 0x2032:
      out += '\'';
      break;
    case 0x201c: case 0x201d: case 0x201e: case 0x2033:
      out += '"';
      break;
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2212:
      out += '-';
      break;
    case 0x2022:
      out += '*';
      break;
    case 0x2026:
      out += "...";
      break;
    case 0xa0:
      out += ' ';
      break;
    default:
      out += '?';
      break;
    }
}

/* Cut the text of a diagnostic into the lines of throwback messages: at the newlines, and at the last space before MAX_LINE characters; at most MAX_LINES of
   them.  Empty lines and the white space around the lines are dropped.  */
static void
make_lines (const char *text, std::vector<std::string> &lines)
{
  std::string ascii;
  for (const unsigned char *p = (const unsigned char *) text; *p; )
    append_ascii (ascii, p);

  bool cut = false;
  size_t pos = 0;
  while (pos < ascii.size ())
    {
      size_t nl = ascii.find ('\n', pos);
      std::string line = ascii.substr (pos, nl == std::string::npos ? std::string::npos : nl - pos);
      pos = (nl == std::string::npos) ? ascii.size () : nl + 1;

      while (!line.empty ())
	{
	  size_t b = line.find_first_not_of (' ');
	  if (b == std::string::npos)
	    break;
	  line.erase (0, b);
	  std::string chunk = line;
	  if (chunk.size () > max_line)
	    {
	      size_t sp = chunk.rfind (' ', max_line);
	      size_t at = (sp == std::string::npos || sp < max_line / 2) ? max_line : sp;
	      chunk = line.substr (0, at);
	      line.erase (0, at);
	    }
	  else
	    line.clear ();
	  size_t e = chunk.find_last_not_of (' ');
	  chunk.erase (e + 1);
	  if (chunk.empty ())
	    continue;
	  if (lines.size () == max_lines)
	    {
	      cut = true;
	      break;
	    }
	  lines.push_back (chunk);
	}
      if (cut)
	break;
    }

  if (cut)
    {
      std::string &last = lines.back ();
      if (last.size () + 3 > max_line)
	last.erase (max_line - 3);
      last += "...";
    }
}

/* A way of getting a message to the editor.  */
class transport
{
public:
  virtual ~transport () {}
  /* Called before the first message; false if throwback is not possible.  */
  virtual bool start () = 0;
  /* One line of a message.  FILE is the name the compiler knows the file by.  Returns false when throwback has stopped working.  */
  virtual bool send (const std::string &file, int line, int severity, const std::string &text) = 0;
  /* Called when the compiler ends, after START returned true.  */
  virtual void finish () = 0;
};

#ifdef RISCOS_TB_NATIVE

/* DDEUtils.  */
class ddeutils_transport : public transport
{
public:
  bool start () override;
  bool send (const std::string &file, int line, int severity, const std::string &text) override;
  void finish () override;

  /* The name that the editor is given for FILE (public for the test program).  */
  std::string riscos_name (const std::string &file);

private:
  bool swi (int number, _kernel_swi_regs *regs);

  /* The file that the editor was last told about (as the compiler calls it), and its full RISC OS name.  */
  std::string m_file;
  std::string m_name;
  bool m_have_file = false;
  bool m_started = false;
};

/* Call a SWI; the registers go in and out in REGS.  An error switches throwback off.  */
bool
ddeutils_transport::swi (int number, _kernel_swi_regs *regs)
{
  if (_kernel_oserror *err = _kernel_swi (number, regs, regs))
    {
      if (debug_p ())
	fprintf (stderr, "throwback: SWI &%x failed: %s\n", (unsigned) number, err->errmess);
      return false;
    }
  return true;
}

/* The full RISC OS name of FILE, which is a Unix style name (or a RISC OS one): the form of the name that the compiler used to open the file (__riscosify_std is what
   open () uses), then OS_FSControl 37, which turns "@", "&", "%" and the special fields and path variables into the real thing.  The editor needs this because it
   does not run in the directory of the compiler.  If a conversion does not work, the name goes on as far as it got.  */
std::string
ddeutils_transport::riscos_name (const std::string &file)
{
  char converted[1024];
  if (!__riscosify_std (file.c_str (), 0, converted, sizeof converted, NULL))
    return file;

  std::vector<char> full (1024);
  for (int attempt = 0; attempt < 2; attempt++)
    {
      _kernel_swi_regs r;
      memset (&r, 0, sizeof r);
      r.r[0] = 37;
      r.r[1] = TB_REG (converted);
      r.r[2] = TB_REG (&full[0]);
      r.r[5] = (int) full.size ();
      if (_kernel_swi (OS_FSControl, &r, &r))
	break;
      if (r.r[5] >= 0)
	return std::string (&full[0]);
      /* The buffer was too small by -R5 bytes.  */
      full.resize (full.size () - r.r[5] + 1);
    }
  return std::string (converted);
}

bool
ddeutils_transport::start ()
{
  _kernel_swi_regs r;
  memset (&r, 0, sizeof r);
  m_started = swi (DDEUtils_ThrowbackStart, &r);
  return m_started;
}

bool
ddeutils_transport::send (const std::string &file, int line, int severity, const std::string &text)
{
  _kernel_swi_regs r;

  if (!m_have_file || file != m_file)
    {
      m_name = riscos_name (file);
      m_file = file;
      m_have_file = true;

      memset (&r, 0, sizeof r);
      r.r[0] = 0;				/* processing this file */
      r.r[2] = TB_REG (m_name.c_str ());
      if (!swi (DDEUtils_ThrowbackSend, &r))
	return false;
    }

  memset (&r, 0, sizeof r);
  r.r[0] = (severity == SEV_INFORMATION) ? 2 : 1;	/* information / error details */
  r.r[1] = 0;
  r.r[2] = TB_REG (m_name.c_str ());
  r.r[3] = line;
  r.r[4] = (severity == SEV_INFORMATION) ? 0 : severity;
  r.r[5] = TB_REG (text.c_str ());
  return swi (DDEUtils_ThrowbackSend, &r);
}

void
ddeutils_transport::finish ()
{
  if (m_started)
    {
      _kernel_swi_regs r;
      memset (&r, 0, sizeof r);
      swi (DDEUtils_ThrowbackEnd, &r);
      m_started = false;
    }
}

#else /* a cross compiler */

/* The syslog priorities (facility user) of the messages that SysLogD turns into throwback.  */
static const int pri_serious = 1 * 8 + 3;
static const int pri_warning = 1 * 8 + 4;
static const int pri_information = 1 * 8 + 6;

/* UDP datagrams in syslog format to the machine that runs SysLogD.  */
class syslog_transport : public transport
{
public:
  ~syslog_transport () { finish (); }
  bool start () override;
  bool send (const std::string &file, int line, int severity, const std::string &text) override;
  void finish () override;

private:
  int m_socket = -1;
  std::string m_hostname;
};

bool
syslog_transport::start ()
{
  const char *host = getenv ("THROWBACK_HOST");
  if (host == NULL)
    {
      if (debug_p ())
	fprintf (stderr, "throwback: THROWBACK_HOST is not set\n");
      return false;
    }

  struct hostent *hp = gethostbyname (host);
  if (hp == NULL || hp->h_addrtype != AF_INET || hp->h_length != (int) sizeof (struct in_addr))
    {
      if (debug_p ())
	fprintf (stderr, "throwback: cannot find the host %s\n", host);
      return false;
    }

  int port = 514;
  if (const char *p = getenv ("THROWBACK_PORT"))
    port = atoi (p);
  else if (struct servent *serv = getservbyname ("syslog", "udp"))
    port = ntohs (serv->s_port);

  struct sockaddr_in name;
  memset (&name, 0, sizeof name);
  memcpy (&name.sin_addr, hp->h_addr_list[0], hp->h_length);
  name.sin_family = AF_INET;
  name.sin_port = htons (port);

  m_socket = socket (AF_INET, SOCK_DGRAM, 0);
  if (m_socket < 0 || connect (m_socket, (struct sockaddr *) &name, sizeof name) < 0)
    {
      if (debug_p ())
	fprintf (stderr, "throwback: cannot open a socket to %s port %d\n", host, port);
      finish ();
      return false;
    }

  char hostname[100];
  if (gethostname (hostname, sizeof hostname) < 0)
    strcpy (hostname, "unknown");
  hostname[sizeof hostname - 1] = '\0';
  m_hostname = hostname;
  return true;
}

bool
syslog_transport::send (const std::string &file, int line, int severity, const std::string &text)
{
  if (m_socket < 0)
    return false;

  char path[PATH_MAX];
  if (realpath (file.c_str (), path) == NULL)
    snprintf (path, sizeof path, "%s", file.c_str ());

  char stamp[20];
  time_t now = time (NULL);
  struct tm tmbuf;
  if (localtime_r (&now, &tmbuf) == NULL
      || strftime (stamp, sizeof stamp, "%b %e %H:%M:%S", &tmbuf) == 0)
    strcpy (stamp, "Jan  1 00:00:00");

  int pri = (severity == SEV_INFORMATION) ? pri_information
	    : (severity == SEV_WARNING) ? pri_warning : pri_serious;

  /* <PRI>Mmm dd hh:mm:ss host throwback FILE:LINE: TEXT  -- what SysLogD looks for.  A ':' in the file name would end it early there, as it does for the
     old compiler.  */
  std::string msg;
  char head[64 + PATH_MAX];
  snprintf (head, sizeof head, "<%d>%s %s throwback %s:%d: ", pri, stamp, m_hostname.c_str (), path, line);
  msg = head;
  msg += text;
  if (msg.size () > 1000)
    msg.erase (1000);

  if (::send (m_socket, msg.data (), msg.size (), 0) < 0)
    {
      if (debug_p ())
	fprintf (stderr, "throwback: the datagram could not be sent\n");
      return false;
    }
  return true;
}

void
syslog_transport::finish ()
{
  if (m_socket >= 0)
    {
      close (m_socket);
      m_socket = -1;
    }
}

#endif

/* The one throwback channel of this compiler run: starts the transport when the first message comes, cuts the messages into lines, switches itself off when the
   transport says it does not work, and ends the transport when the compiler exits.  */
class channel
{
public:
  static channel &instance ()
  {
    static channel *the_channel = new channel;
    return *the_channel;
  }

  void report (const entry &e);
  void finish ();

  /* For the test: replace the transport.  */
  void set_transport (transport *t) { delete m_transport; m_transport = t; m_state = t ? STATE_NEW : STATE_OFF; }

private:
  channel ()
  {
#ifdef RISCOS_TB_NATIVE
    m_transport = new ddeutils_transport;
#else
    m_transport = new syslog_transport;
#endif
  }

  static void at_exit () { instance ().finish (); }

  enum state { STATE_NEW, STATE_RUNNING, STATE_OFF };
  transport *m_transport;
  state m_state = STATE_NEW;
};

void
channel::report (const entry &e)
{
  if (m_state == STATE_OFF || !m_transport)
    return;

  /* Locations that are not files: "<built-in>", "<command-line>", "<stdin>".  A name with a path variable in front ("<Obey$Dir>.c.main") is a file.  */
  if (e.m_file.empty ()
      || (e.m_file[0] == '<'
	  && (e.m_file.find ('>') == std::string::npos
	      || e.m_file.find ('>') + 1 == e.m_file.size ())))
    return;

  if (m_state == STATE_NEW)
    {
      if (!m_transport->start ())
	{
	  m_state = STATE_OFF;
	  return;
	}
      m_state = STATE_RUNNING;
      atexit (at_exit);
    }

  std::vector<std::string> lines;
  make_lines (e.m_text.c_str (), lines);
  if (lines.empty ())
    lines.push_back ("(no text)");

  int line = e.m_line < 1 ? 0 : e.m_line;
  for (size_t i = 0; i < lines.size (); i++)
    /* The first line has the severity of the diagnostic; what follows is more of the same message.  */
    if (!m_transport->send (e.m_file, line, i == 0 ? e.m_severity : (int) SEV_INFORMATION, lines[i]))
      {
	m_state = STATE_OFF;
	return;
      }
}

void
channel::finish ()
{
  if (m_state == STATE_RUNNING && m_transport)
    m_transport->finish ();
  if (m_state == STATE_RUNNING)
    m_state = STATE_OFF;
}

} // namespace riscos_tb

#ifdef RISCOS_TB_STANDALONE

/* Entry points for the tests.  */
void
riscos_tb_test_report (const char *file, int line, int severity, const char *text)
{
  riscos_tb::entry e;
  e.m_file = file;
  e.m_line = line;
  e.m_severity = severity;
  e.m_text = text;
  riscos_tb::channel::instance ().report (e);
}

void
riscos_tb_test_finish ()
{
  riscos_tb::channel::instance ().finish ();
}

void
riscos_tb_test_lines (const char *text, std::vector<std::string> &lines)
{
  riscos_tb::make_lines (text, lines);
}

#ifdef RISCOS_TB_NATIVE
std::string
riscos_tb_test_name (const char *file)
{
  riscos_tb::ddeutils_transport t;
  return t.riscos_name (file);
}
#endif

#else /* inside the compiler */

namespace riscos_tb {

using namespace diagnostics;

/* The diagnostics that were reported to a diagnostics::buffer (the C++ front end holds some back while it makes up its mind), until the buffer is flushed.  */
class buffered_entries : public per_sink_buffer
{
public:
  void dump (FILE *out, int indent) const final override
  {
    dumping::emit_heading (out, indent, "riscos_throwback_buffer");
    fprintf (out, "%*s%d entries\n", indent + 2, "", (int) m_entries.size ());
  }
  bool empty_p () const final override { return m_entries.empty (); }
  void move_to (per_sink_buffer &dest) final override
  {
    buffered_entries &d = static_cast<buffered_entries &> (dest);
    d.m_entries.insert (d.m_entries.end (), m_entries.begin (), m_entries.end ());
    m_entries.clear ();
  }
  void clear () final override { m_entries.clear (); }
  void flush () final override
  {
    for (const entry &e : m_entries)
      channel::instance ().report (e);
    m_entries.clear ();
  }

  std::vector<entry> m_entries;
};

/* Fortran's %C and %L ("(1)", "(2)" in the text, with the matching locus shown in the source) are not idempotent: the front end's format decoder counts the loci that
   the diagnostic has already got, so when a message is formatted a second time - for another output sink, as the context does - it comes out as "(2)".  In the
   throwback printer %C and %L are therefore numbered here, by a counter that starts again for each diagnostic.  Every other format is left to the decoder that the
   printer had.  */
static printer_fn original_decoder;
static unsigned fortran_locus_count;
static bool fortran_p;

static bool
throwback_format_decoder (pretty_printer *pp, text_info *text, const char *spec, int precision, bool wide, bool set_locus, bool hash, bool *quoted,
			  pp_token_list &formatted_token_list)
{
  if (fortran_p && (*spec == 'C' || *spec == 'L'))
    {
      if (*spec == 'L')
	(void) va_arg (*text->m_args_ptr, void *);	/* the locus that goes with it */
      pp_string (pp, fortran_locus_count++ == 0 ? "(1)" : "(2)");
      return true;
    }
  return original_decoder (pp, text, spec, precision, wide, set_locus, hash, quoted, formatted_token_list);
}

/* The sink.  */
class throwback_sink : public sink
{
public:
  throwback_sink (context &dc)
  : sink (dc),
    m_buffer (nullptr)
  {
    configure_printer ();
  }

  void dump_kind (FILE *out) const final override { fprintf (out, "riscos_throwback_sink"); }

  std::unique_ptr<per_sink_buffer>
  make_per_sink_buffer () final override
  {
    return std::make_unique<buffered_entries> ();
  }
  void set_buffer (per_sink_buffer *base) final override
  {
    m_buffer = static_cast<buffered_entries *> (base);
  }

  void on_begin_group () final override {}
  void on_end_group () final override {}
  void on_report_diagnostic (const diagnostic_info &, enum kind) final override;
  void on_diagram (const diagram &) final override {}
  void after_diagnostic (const diagnostic_info &) final override {}
  bool machine_readable_stderr_p () const final override { return false; }
  bool follows_reference_printer_p () const final override { return false; }
  void update_printer () final override
  {
    m_printer = m_context.clone_printer ();
    configure_printer ();
  }
  void report_global_digraph (const lazily_created<digraphs::digraph> &) final override {}
  void report_digraph_for_logical_location (const lazily_created<digraphs::digraph> &, logical_locations::key) final override {}

private:
  /* The text goes to an editor: no colours, no URLs, and no line breaks of the compiler's own.  */
  void configure_printer ()
  {
    pp_show_color (m_printer.get ()) = false;
    m_printer->set_url_format (URL_FORMAT_NONE);
    pp_set_line_maximum_length (m_printer.get (), 0);

    printer_fn &decoder = pp_format_decoder (m_printer.get ());
    if (decoder && decoder != throwback_format_decoder)
      {
	original_decoder = decoder;
	/* "GNU Fortran", or "GNU Fortran2008" and the like.  */
	fortran_p = lang_hooks.name && !strncmp (lang_hooks.name, "GNU Fortran", 11);
	decoder = throwback_format_decoder;
      }
  }

  buffered_entries *m_buffer;
};

/* How bad is a diagnostic of kind K.  */
static int
severity_of (enum kind k)
{
  switch (k)
    {
    case kind::ice:
    case kind::ice_nobt:
    case kind::fatal:
      return SEV_SERIOUS_ERROR;
    case kind::error:
    case kind::sorry:
    case kind::permerror:
    case kind::werror:
      return SEV_ERROR;
    case kind::warning:
    case kind::anachronism:
    case kind::pedwarn:
      return SEV_WARNING;
    default:
      return SEV_INFORMATION;		/* note, debug, ... */
    }
}

void
throwback_sink::on_report_diagnostic (const diagnostic_info &diagnostic, enum kind orig_diag_kind)
{
  pretty_printer *pp = get_printer ();
  pp_output_formatted_text (pp);

  const expanded_location xloc = diagnostic_expand_location (&diagnostic);
  if (xloc.file && *xloc.file)
    {
      entry e;
      e.m_file = xloc.file;
      e.m_line = xloc.line;
      e.m_severity = severity_of (diagnostic.m_kind);
      e.m_text = pp_formatted_text (pp);

      /* Which option controls it, as the text output does: " [-Wunused-variable]".  */
      if (char *option_text = m_context.make_option_name (diagnostic.m_option_id, orig_diag_kind, diagnostic.m_kind))
	{
	  e.m_text += " [";
	  e.m_text += option_text;
	  e.m_text += "]";
	  free (option_text);
	}

      if (m_buffer)
	m_buffer->m_entries.push_back (e);
      else
	channel::instance ().report (e);
    }

  pp_clear_output_area (pp);
  fortran_locus_count = 0;
}

} // namespace riscos_tb

/* Called by arm_option_override when -mthrowback is given: add the sink.  */
void
riscos_throwback_init (void)
{
  static bool done = false;
  if (done)
    return;
  done = true;

  global_dc->add_sink (std::make_unique<riscos_tb::throwback_sink> (*global_dc));
}

#endif /* RISCOS_TB_STANDALONE */
