/* riscos_start.c - the C start-up of a hosted program: copy the command line into argv[], set the limits of the heap (sbrk) and of the stack, run the constructors, call main and exit.
   The command line from OS_GetEnv is the whole line, the program first ("RAM::RamDisc0.$.hello a "b c" d"): words are separated by spaces, a word in double quotes may hold spaces. */
extern int main (int, char **, char **);
extern void __ul64_init (int, char **) __attribute__ ((weak));
extern void exit (int);

extern void (*__preinit_array_start[]) (void);
extern void (*__preinit_array_end[]) (void);
extern void (*__init_array_start[]) (void);
extern void (*__init_array_end[]) (void);
/* picolibc's __libc_init_array is empty in this build: the constructors (C++ static objects, __attribute__ ((constructor)), the registration of the unwind tables by crtbegin.o) are run here */
static void run_init_arrays (void)
{
  void (**f) (void);
  for (f = __preinit_array_start; f != __preinit_array_end; f++) (*f) ();
  for (f = __init_array_start; f != __init_array_end; f++) (*f) ();
}
extern char __heap_start[];
/* Thread-local storage for the one thread: the linker script puts .tdata and .tbss in the image and reserves __tls_space for the block; the initial values are copied there, and tpidr_el0 points at the thread control
   block that AArch64 expects just before it (16 bytes, rounded up to the alignment of the TLS segment).  The anchors make sure that both sections exist, so that the script can ask for their alignment. */
extern char __tdata_start[], __tdata_end[], __tbss_start[], __tbss_end[], __tls_space[], __tls_align[];
__thread char __ul64_tls_anchor_d = 1;
__thread char __ul64_tls_anchor_b;
static void init_tls (void)
{
  unsigned long a = (unsigned long) __tls_align, i;
  unsigned long tcb = (16 + a - 1) & ~(a - 1);
  for (i = 0; i < (unsigned long) (__tdata_end - __tdata_start); i++) __tls_space[i] = __tdata_start[i];
  for (i = (unsigned long) (__tbss_start - __tdata_start); i < (unsigned long) (__tbss_end - __tdata_start); i++) __tls_space[i] = 0;
  __asm__ volatile ("msr tpidr_el0, %0" : : "r" ((unsigned long) __tls_space - tcb));
}

unsigned long __riscos_ramlimit;
char *__riscos_heap_end;				/* sbrk may not go beyond it */
static char argbuf[1024];
static char *argvec[64];
static char *envvec[1];
#define STACK_RESERVE (128 * 1024)			/* the stack grows down from the RAM limit; the heap leaves this much for it */

void __riscos_start (const char *cmd, unsigned long ramlimit)
{
  int argc = 0;
  unsigned i = 0;
  __riscos_ramlimit = ramlimit;
  __riscos_heap_end = (char *) ((ramlimit - STACK_RESERVE) & ~15UL);
  if (cmd)
    {
      for (;;)
	{
	  while (cmd[0] == ' ') cmd++;
	  if ((unsigned char) cmd[0] < 32) break;
	  if (argc < 63) argvec[argc++] = &argbuf[i];
	  if (cmd[0] == '"')
	    {
	      cmd++;
	      while ((unsigned char) cmd[0] >= 32 && cmd[0] != '"') { if (i < sizeof argbuf - 1) argbuf[i++] = *cmd; cmd++; }
	      if (cmd[0] == '"') cmd++;
	    }
	  else
	    while ((unsigned char) cmd[0] > 32) { if (i < sizeof argbuf - 1) argbuf[i++] = *cmd; cmd++; }
	  argbuf[i++] = 0;
	}
    }
  argvec[argc] = 0;
  init_tls ();
  run_init_arrays ();
  if (__ul64_init) __ul64_init (argc, argvec);				/* UnixLib 64, if it is linked in (riscos64/unixlib) */
  exit (main (argc, argvec, envvec));
}
