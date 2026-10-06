/* modver.c -- print the title and the help string (name, version, date) of a loaded RISC OS module: modver ARMEABISupport.  Read-only: OS_Module 18 (look up a module by name) and a guarded read of the module's own
   header (word 4 = title offset, word 5 = help string offset).  Used by the ARMEABISupport module tests to show WHICH build of the module is running.
   modver NAME --has TEXT   (1.1) also checks that the help string contains TEXT: exit status 0 if it does, 3 if it does not (an interlock for Obey files: "run the loops only if the fixed SharedUnixLibrary
   1.16-vforkfix2 is the one that is loaded"); 1 if the module is not loaded, 2 if its header cannot be read. */
#define _GNU_SOURCE
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <swis.h>

static sigjmp_buf jb;
static void segv(int s) { (void) s; siglongjmp(jb, 1); }

static __attribute__((noinline)) int read_header(unsigned base, char *title, char *help, size_t n)
{
  struct sigaction sa, old; memset(&sa, 0, sizeof sa); sa.sa_handler = segv; sigaction(SIGSEGV, &sa, &old);
  int ok = 0;
  if (sigsetjmp(jb, 1) == 0) {
    const unsigned *h = (const unsigned *) base;
    strncpy(title, (const char *) base + h[4], n - 1); title[n - 1] = 0;
    strncpy(help, (const char *) base + h[5], n - 1); help[n - 1] = 0;
    for (char *c = help; *c; c++) if (*c == '\t') *c = ' ';
    ok = 1;
  }
  sigaction(SIGSEGV, &old, NULL);
  return ok;
}

int main(int argc, char **argv)
{
  const char *name = argc > 1 ? argv[1] : "ARMEABISupport";
  unsigned num = 0, inst = 0, base = 0;
  _kernel_oserror *e = _swix(OS_Module, _INR(0, 1) | _OUTR(1, 3), 18, name, &num, &inst, &base);
  if (e) { printf("modver: module \"%s\" not found: %s\n", name, e->errmess); return 1; }
  char title[128], help[256];
  int has = argc > 3 && !strcmp(argv[2], "--has");
  if (read_header(base, title, help, sizeof title)) printf("modver: %s: %s   (module number %u, at 0x%08x)\n", title, help, num, base);
  else { printf("modver: module \"%s\" is loaded (module number %u, at 0x%08x) but its header cannot be read from a user program\n", name, num, base); return has ? 2 : 0; }
  if (has) {
    int found = strstr(help, argv[3]) != NULL;
    printf("modver: the help string %s \"%s\"\n", found ? "contains" : "does NOT contain", argv[3]);
    return found ? 0 : 3;
  }
  return 0;
}
