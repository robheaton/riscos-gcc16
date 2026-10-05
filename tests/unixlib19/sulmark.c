/* sulmark.c -- write a line into the SulLog file that the traced SharedUnixLibrary (SharedULib-116fix2t) writes to (see sultrace.h): so that an Obey file can put stage marks between the program lines.
   usage: sulmark --init        make sure the log exists (create it when it does not: an existing log is NOT emptied, *Remove it first), write a line, check that the length grew; exit 0 when it works
          sulmark TEXT ...      append the text (the arguments joined by spaces) as one line, in the log's format with the time
          sulmark --ul          exit 0 only if the TRACED libunixlib (the ULTRACE build, loaded from the pack's lib folder by RunTraceUL1) is the one in use: only that library answers sysconf (0x4702) with 0x7ACE
   NOTE: it is a UnixLib program: run it only AFTER the traced module has been loaded with RMLoad (the first UnixLib program of a boot loads the installed SharedUnixLibrary on demand, and the traced
   module could then no longer be loaded).  The module's trace does not create the log (a file that does not exist is not opened for update): run  sulmark --init  before anything else that matters.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include "sultrace.h"
#include <unistd.h>

static unsigned log_length(int *exists)
{
  unsigned type = 0, len = 0;
  if (_swix(OS_File, _INR(0, 1) | _OUTR(0, 0) | _OUT(4), 5, SUL_LOG, &type, &len) != NULL) type = 0;
  *exists = type == 1;
  if (!*exists) len = 0;                               /* OS_File 5 leaves r4 alone for an object that does not exist: it held garbage (RunTraceSul1, 2026-10-04 11:19) */
  return len;
}

int main(int argc, char **argv)
{
  if (argc == 2 && !strcmp(argv[1], "--init")) {
    int ex = 0; unsigned before = log_length(&ex);
    if (!ex) {
      unsigned h = 0;
      if (_swix(OS_Find, _INR(0, 1) | _OUT(0), 0x80, SUL_LOG, &h) != NULL || h == 0) { fprintf(stderr, "sulmark: cannot create %s\n", SUL_LOG); return 3; }
      _swix(OS_Find, _INR(0, 1), 0, h);
    }
    sl_mark("sulmark: the log is ready", before);
    unsigned after = log_length(&ex);
    if (!ex || after <= before) { fprintf(stderr, "sulmark: the log %s cannot be written (length %u before, %u after)\n", SUL_LOG, before, after); return 4; }
    printf("sulmark: the log %s works (%u bytes before, %u after)\n", SUL_LOG, before, after);
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--ul")) {
    long v = sysconf(0x4702);                          /* -1 (EINVAL) in the release library; the ULTRACE build answers 0x7ACE */
    if (v != 0x7ACE) { printf("sulmark: the TRACED libunixlib is NOT the one in use (sysconf (0x4702) = %ld; fix level %ld)\n", v, sysconf(0x4700)); return 5; }
    sl_mark("sulmark: the traced libunixlib is in use", (unsigned) sysconf(0x4700));
    printf("sulmark: the traced libunixlib is in use (fix level %ld)\n", sysconf(0x4700));
    return 0;
  }
  char b[300], *p = b;
  for (int i = 1; i < argc && p < b + 150; i++) { if (i > 1) *p++ = ' '; for (const char *q = argv[i]; *q && p < b + 150; ) *p++ = *q++; }   /* (sl_mark cuts the tag at 150 characters anyway) */
  *p = 0;
  sl_mark(b, 0);
  return 0;
}
