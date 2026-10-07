/* exit.c - the parts of <stdlib.h> that end things: atexit, exit, _Exit, abort.  A module is not a program: exit () and abort () in a module that is not "runnable" stop with a RISC OS error
   (OS_GenerateError) that says so; in a "runnable" module (module-is-runnable: its start code sets __modlib_app) the program (USER mode) ends with OS_Exit, the way a C program ends, and the module is
   a module again afterwards. */
#include <stdlib.h>

int __modlib_app;
#define MAXEXIT 32
static void (*exitfn[MAXEXIT]) (void);
static int nexit;

int atexit (void (*fn) (void))
{
  if (nexit >= MAXEXIT) return -1;
  exitfn[nexit++] = fn;
  return 0;
}

static void stop (int status, const char *what) __attribute__ ((noreturn));
static void stop (int status, const char *what)
{
  static struct { int errnum; char msg[64]; } err;
  char *p = err.msg;
  unsigned cpsr;
  __asm__ volatile ("mrs\t%0, cpsr" : "=r" (cpsr));
  if (__modlib_app && (cpsr & 0x1F) == 0x10)                                  /* the program itself (USER mode), not a command or a callback of the module that is run while it is there */
    {
      __modlib_app = 0;                                                       /* the module is a module again, and the exit functions are used up */
      nexit = 0;
      register unsigned r0 __asm__ ("r0") = 0, r1 __asm__ ("r1") = 0x58454241, r2 __asm__ ("r2") = (unsigned) status;     /* OS_Exit: "ABEX" and the return code */
      __asm__ volatile ("swi\t0x11" : : "r" (r0), "r" (r1), "r" (r2) : "memory");
    }
  err.errnum = 0x1B0;
  while (*what) *p++ = *what++;                                               /* "exit (N) was called in a module" without printf */
  *p++ = ' '; *p++ = '(';
  {
    char tmp[12], *t = tmp + sizeof tmp;
    unsigned u = status < 0 ? -(unsigned) status : (unsigned) status;
    do *--t = (char) ('0' + u % 10); while (u /= 10);
    if (status < 0) *--t = '-';
    while (t < tmp + sizeof tmp) *p++ = *t++;
  }
  for (const char *s = ") was called in a module"; (*p = *s) != 0; s++) p++;
  register unsigned r0 __asm__ ("r0") = (unsigned) &err;
  __asm__ volatile ("swi\t0x2B" : : "r" (r0) : "memory");                                 /* OS_GenerateError */
  for (;;) ;
}
void _Exit (int status) { stop (status, "exit"); }
void exit (int status)
{
  while (nexit > 0) { void (*f) (void) = exitfn[--nexit]; f (); }
  _Exit (status);
}
void abort (void) { stop (134, "abort"); }
