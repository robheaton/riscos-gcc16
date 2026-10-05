/* tbsink.c -- a throwback RECEIVER for tests: a small Wimp task that registers itself with DDEUtils as the task that is sent the throwback messages (what StrongED, Zap and
   !Edit do) and writes everything that arrives to a log file, with the fields decoded and the first bytes as hex.  Only one task can be registered with DDEUtils at a time:
   quit your editor's throwback first (or the editor), or this one is told "Another task is registered for throwback" (it says so in the log and ends).
   It ends when the file named by the system variable TbSink$Stop appears, when the Wimp tells it to quit, or after TbSink$Seconds seconds (default 600).
   The log is TbSink$Log (default RAM::RamDisc0.$.TbSinkLog).  Run it as an application (!TbSink: double-click, or *Filer_Run) - not from a Task window.
   Messages (DDEUtils 1.75, Sources/Programmer/DDEUtils): &42580 start, &42581 processing file (+20: name), &42582 errors in file (+20: name), &42583 error details (+20: line, +24:
   severity, +28: text), &42584 end, &42585 information for file (+20: name), &42586 information details (+20: line, +24: 0, +28: text).  */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <kernel.h>

#define DDE_Register     0x42585
#define DDE_UnRegister   0x42586

static FILE *logf;
static unsigned start_cs;

static unsigned now_cs (void)
{
  int t = 0;
  _swix (OS_ReadMonotonicTime, _OUT (0), &t);
  return (unsigned) t;
}

static const char *var (const char *name, const char *dflt)
{
  static char buf[4][512];
  static int n;
  char *b = buf[n++ & 3];
  int len = 0;
  _kernel_oserror *e = _swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), name, b, 511, 0, 3, &len);
  if (e || len <= 0 || len >= 511) return dflt;
  b[len] = 0;
  return b;
}

static void say (const char *fmt, ...)
{
  va_list ap;
  fprintf (logf, "[%5u.%02u] ", (now_cs () - start_cs) / 100, (now_cs () - start_cs) % 100);
  va_start (ap, fmt);
  vfprintf (logf, fmt, ap);
  va_end (ap);
  fputc ('\n', logf);
  fflush (logf);
}

static const char *safe (const char *s, int max)       /* the string at s, at most max bytes, printable */
{
  static char out[4][300];
  static int n;
  char *o = out[n++ & 3];
  int i = 0;
  for (; i < max && i < 290 && s[i]; i++) o[i] = (s[i] >= 32 && s[i] < 127) ? s[i] : '?';
  o[i] = 0;
  return o;
}

static void describe (const int *m)
{
  int size = m[0], code = m[4];
  const char *d = (const char *) m;
  int room = size - 20;
  if (room < 0) room = 0;
  char hex[200];
  int hn = 0;
  for (int i = 0; i < size && i < 56 && hn < 190; i++) hn += sprintf (hex + hn, "%02x%s", (unsigned char) d[i], (i % 4 == 3) ? " " : "");
  switch (code)
    {
    case 0x42580: say ("&42580 START          size %d from task %d (ref %d)", size, m[1], m[2]); break;
    case 0x42581: say ("&42581 PROCESSING file '%s'", safe (d + 20, room)); break;
    case 0x42582: say ("&42582 ERRORS IN   file '%s'", safe (d + 20, room)); break;
    case 0x42583: say ("&42583 ERROR DETAILS  line %d  severity %d  '%s'", m[5], m[6], safe (d + 28, size - 28 > 0 ? size - 28 : 0)); break;
    case 0x42584: say ("&42584 END"); break;
    case 0x42585: say ("&42585 INFO FOR    file '%s'", safe (d + 20, room)); break;
    case 0x42586: say ("&42586 INFO DETAILS   line %d  level %d  '%s'", m[5], m[6], safe (d + 28, size - 28 > 0 ? size - 28 : 0)); break;
    default: say ("&%x (other message) size %d", code, size); break;
    }
  say ("        raw: %s", hex);
}

int main (void)
{
  const char *logname = var ("TbSink$Log", "RAM::RamDisc0.$.TbSinkLog");
  const char *stop = var ("TbSink$Stop", "RAM::RamDisc0.$.TbSinkStop");
  int seconds = atoi (var ("TbSink$Seconds", "600"));
  logf = fopen (logname, "w");
  if (!logf) return 1;
  start_cs = now_cs ();
  say ("tbsink 1.0 starting; log %s, stop file %s, at most %d s", logname, stop, seconds);

  static const int messages[] = { 0x42580, 0x42581, 0x42582, 0x42583, 0x42584, 0x42585, 0x42586, 0 };
  int version = 0, task = 0;
  _kernel_oserror *e = _swix (Wimp_Initialise, _INR (0, 3) | _OUTR (0, 1), 310, 0x4B534154, "Throwback test", messages, &version, &task);
  if (e) { say ("Wimp_Initialise failed: %s", e->errmess); fclose (logf); return 1; }
  say ("Wimp_Initialise: version %d, task handle %d", version, task);

  e = _swix (DDE_Register, _IN (0), task);
  if (e) say ("ThrowbackRegister FAILED: %s", e->errmess);
  else say ("registered with DDEUtils: the throwback messages come here now");
  int registered = e == NULL;

  int block[64];
  unsigned deadline = now_cs () + 100u * (unsigned) seconds;
  int running = registered;
  while (running)
    {
      int ev = 0;
      /* mask: only null events (0) and messages (17, 18, 19) are wanted (bits 1 - 16 set; bits 20 - 31 are reserved and must be 0: 0xFFF1FFFE made the Wimp say "Bad parameter passed to Wimp in R0", RunTb12); null events every half second */
      e = _swix (Wimp_PollIdle, _INR (0, 2) | _OUT (0), 0x0001FFFE, block, now_cs () + 50, &ev);
      if (e) { say ("Wimp_PollIdle failed: %s", e->errmess); break; }
      if (ev == 17 || ev == 18)
	{
	  if (block[4] == 0) { say ("Message_Quit"); running = 0; }
	  else if (block[4] >= 0x42580 && block[4] <= 0x4259F) describe (block);
	  else say ("message &%x ignored", block[4]);
	}
      else if (ev == 0)
	{
	  int type = 0;
	  _swix (OS_File, _INR (0, 1) | _OUT (0), 17, stop, &type);
	  if (type != 0) { say ("the stop file exists"); running = 0; }
	  if ((int) (now_cs () - deadline) > 0) { say ("time is up"); running = 0; }
	}
    }

  if (registered)
    {
      e = _swix (DDE_UnRegister, _IN (0), task);
      say ("ThrowbackUnRegister: %s", e ? e->errmess : "ok");
    }
  _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
  say ("tbsink stopped");
  fclose (logf);
  return 0;
}
