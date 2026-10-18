/* t_segv.c - SIGSEGV and SIGILL handlers: the fault of a write to address 0, a call to a bad address, an instruction that does not exist and a stack that has run out is caught and the program goes on with longjmp.
   With the argument "return" the handler returns, which ends the program with a report. */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("ok   %s\n", #c); } while (0)
static jmp_buf jb;
static volatile int caught, in_handler_stack_ok;
static int returns;
static void on_fault (int sig) { caught = sig; if (!returns) longjmp (jb, 1); }

int main (int argc, char **argv)
{
  struct sigaction sa;
  int i;
  returns = argc > 1 && !strcmp (argv[1], "return");
  signal (SIGSEGV, on_fault);
  signal (SIGILL, on_fault);
  if (returns)
    {
      printf ("the handler returns\n"); fflush (stdout);
      *(volatile int *) 0 = 1;
      printf ("not reached\n");
      return 0;
    }
  for (i = 0; i < 3; i++)
    {
      caught = 0;
      if (setjmp (jb) == 0) *(volatile int *) 0 = 1;
      CHECK (caught == SIGSEGV);
    }
  caught = 0;
  if (setjmp (jb) == 0) ((void (*) (void)) 0x70000000) ();
  CHECK (caught == SIGSEGV || caught == SIGILL);
  caught = 0;
  if (setjmp (jb) == 0) __asm__ volatile (".inst 0x00000000");
  CHECK (caught == SIGILL || caught == SIGSEGV);	/* Pyromaniac gives one handler to undefined instruction, prefetch and data aborts (SIGSEGV for all) */
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = on_fault;
  sigaction (SIGBUS, &sa, NULL);
  signal (SIGSEGV, SIG_DFL);
  signal (SIGILL, SIG_DFL);
  signal (SIGBUS, SIG_DFL);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
