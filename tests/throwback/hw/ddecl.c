/* ddecl.c 1.0 -- does the DDEUtils extended command line (SWIs SetCLSize, SetCL, GetCLSize, GetCl) keep what it is given?  The sequence is the one SOManager's SOMRun uses (SetCLSize (strlen + 1), SetCL (text)) and the
   one UnixLib's start-up reads (GetCLSize, GetCl).  For every size N: store a text of N-1 characters, read the size back, read the text back, compare.  Nothing is left behind (GetCl takes the text away).  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>

static int getsize (void)
{
  int n = -1;
  _kernel_oserror *e = _swix (0x42583, _OUT (0), &n);
  return e ? -2 : n;
}

int main (int argc, char **argv)
{
  static const int sizes[] = { 2, 10, 20, 58, 59, 60, 61, 62, 100, 119, 120, 121, 124, 128, 200, 255, 256, 300, 434, 500, 1000, 2000, 0 };
  int bad = 0;
  printf ("ddecl 1.0: the DDEUtils command line SWIs (the size now: %d; it must be 0)\n", getsize ());
  for (int k = 0; sizes[k]; k++)
    {
      int N = sizes[k];
      char *s = malloc (N + 1), *b = malloc (N + 64);
      for (int i = 0; i < N - 1; i++) s[i] = 'a' + (i % 26);
      s[N - 1] = '\0';
      memset (b, 0xAA, N + 64);
      _kernel_oserror *e1 = _swix (0x42581, _IN (0), N);
      int sz1 = getsize ();
      _kernel_oserror *e2 = _swix (0x42582, _IN (0), s);
      int sz2 = getsize ();
      _kernel_oserror *e3 = _swix (0x42584, _IN (0), b);
      int sz3 = getsize ();
      size_t len = 0;
      while (len < (size_t) N + 63 && b[len] != '\0') len++;
      int ok = !e1 && !e2 && !e3 && sz1 == N && sz2 == N && sz3 == 0 && len == (size_t) N - 1 && memcmp (s, b, N - 1) == 0;
      printf ("  N=%-5d SetCLSize %s, size %d | SetCL %s, size %d | GetCl %s, text %lu chars, size now %d  %s\n", N, e1 ? e1->errmess : "ok", sz1, e2 ? e2->errmess : "ok", sz2, e3 ? e3->errmess : "ok",
	      (unsigned long) len, sz3, ok ? "OK" : "** WRONG **");
      if (!ok) bad++;
      free (s);
      free (b);
    }
  printf ("ddecl: %s\n", bad ? "THE DDEUtils COMMAND LINE LOSES OR ALTERS DATA" : "the DDEUtils command line is faithful for every size tried");
  return bad ? 1 : 0;
}
