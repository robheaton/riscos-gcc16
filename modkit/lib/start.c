/* start.c - the library's half of a "runnable" module (module-is-runnable in the CMHG file): the start code that cmunge puts in the module header is entered in USER mode by *RMRun / OS_Module Enter, sets the
   stack at the top of the application memory and calls __modlib_start with the command tail and the title of the module.  This builds argc / argv (the title, then the words of the tail; "..." makes one word),
   calls main () and ends the program with its result: exit () runs the atexit functions and OS_Exit gives the return code (what a C program does). */
#include <stddef.h>
#include <stdlib.h>

extern int __modlib_app;
extern int main (int argc, char **argv);

/* a runnable module with no  main  of its own (it only has module-is-runnable: so that *RMRun finds it, as cmdserv2 does) ends at once with 0 when it is run */
int __attribute__ ((weak)) main (int argc, char **argv)
{
  (void) argc; (void) argv;
  return 0;
}

void __modlib_start (const char *tail, const char *title)
{
  enum { MAXARGS = 40, TAILMAX = 255 };
  char buf[TAILMAX + 1];
  char *argv[MAXARGS + 1];
  int argc = 0;
  size_t n = 0;
  char *p;
  while (tail && (unsigned char) tail[n] >= 32 && n < TAILMAX) { buf[n] = tail[n]; n++; }          /* the tail ends at the first control character (CR, LF, NUL) */
  buf[n] = 0;
  argv[argc++] = (char *) title;
  p = buf;
  while (*p && argc < MAXARGS)
    {
      while (*p == ' ') p++;
      if (!*p) break;
      if (*p == '"')
        {
          argv[argc++] = ++p;
          while (*p && *p != '"') p++;
        }
      else
        {
          argv[argc++] = p;
          while (*p && *p != ' ') p++;
        }
      if (*p) *p++ = 0;
    }
  argv[argc] = 0;
  __modlib_app = 1;
  exit (main (argc, argv));
}
