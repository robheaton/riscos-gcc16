/* riscos_start.c - the C start-up of a hosted program: copy the command line into argv[], set the limits of the heap (sbrk) and of the stack, run the constructors, call main and exit.
   The command line from OS_GetEnv is the whole line, the program first ("RAM::RamDisc0.$.hello a "b c" d"): words are separated by spaces, a word in double quotes may hold spaces, and a doubled quote inside is one quote, and DEL n / DEL r are a line feed / carriage return. */
typedef unsigned long size_t;
extern int main (int, char **, char **);
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern void *memcpy (void *, const void *, size_t);
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
/* libstdc++ tests a weak reference to pthread_cancel to find out whether threads are there, and calls the other pthread functions through weak references too: link the thread code (it is small) into every program, or std::call_once and the like fail. */
extern int __ul64_nthreads;
static int *const keep_pthread __attribute__ ((used)) = &__ul64_nthreads;
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
char *program_invocation_name, *program_invocation_short_name;	/* glibc's names for argv[0] and its last part (gnulib's getprogname and error () use them) */
static char argbuf[4096];
static char *argvec[256];
static char *envvec[130];
extern char **environ;
/* The environment: the variables that the program which started this one exported (process.c sets the system variables, and Unix$Environ with their names). */
static void init_environ (void)
{
  static char names[2048], vals[8192];
  unsigned long r[10] = { (unsigned long) "Unix$Environ", (unsigned long) names, sizeof names - 1, 0, 3 };
  size_t used = 0;
  int n = 0;
  char *p;
  envvec[0] = 0;
  environ = envvec;
  if (swi_x (0x23, r) || r[2] >= sizeof names) return;
  names[r[2]] = 0;
  for (p = names; *p && n < 127; )
    {
      char *e = p, *v;
      unsigned long q[10];
      size_t nl;
      while (*e && *e != ' ') e++;
      nl = (size_t) (e - p);
      if (*e) *e++ = 0;
      v = vals + used;
      if (used + nl + 2 < sizeof vals)
	{
	  memcpy (v, p, nl); v[nl] = '=';
	  q[0] = (unsigned long) p; q[1] = (unsigned long) (v + nl + 1); q[2] = sizeof vals - used - nl - 2; q[3] = 0; q[4] = 3;
	  if (!swi_x (0x23, q) && q[2] < sizeof vals - used - nl - 2) { v[nl + 1 + q[2]] = 0; envvec[n++] = v; used += nl + 1 + q[2] + 1; }
	}
      p = e;
    }
  envvec[n] = 0;
}
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
	  if (argc < 255) argvec[argc++] = &argbuf[i];
	  if (cmd[0] == '"')
	    {
	      cmd++;
	      for (;;)								/* up to the closing quote; "" inside is a quote (how system (), posix_spawn and the shell write an argument that has a quote in it) */
		{
		  if ((unsigned char) cmd[0] < 32 && cmd[0] != 9) break;			/* (a tab inside quotes belongs to the argument) */
		  if (cmd[0] == '"') { if (cmd[1] == '"') { if (i < sizeof argbuf - 1) argbuf[i++] = '"'; cmd += 2; continue; } cmd++; break; }
		  if (cmd[0] == 127 && cmd[1])				/* a line feed or a carriage return cannot be in a command line: DEL n, DEL r (and DEL DEL for a DEL) stand for them; DEL t is a tab; DEL q is a quote in an argument that has a { (the operating system looks for { > file } in the line and loses its way with "" there) */
		    {
		      if (i < sizeof argbuf - 1) argbuf[i++] = cmd[1] == 'n' ? '\n' : cmd[1] == 'r' ? '\r' : cmd[1] == 'q' ? '"' : cmd[1] == 't' ? '\t' : cmd[1];
		      cmd += 2;
		      continue;
		    }
		  if (i < sizeof argbuf - 1) argbuf[i++] = *cmd;
		  cmd++;
		}
	    }
	  else
	    while ((unsigned char) cmd[0] > 32) { if (i < sizeof argbuf - 1) argbuf[i++] = *cmd; cmd++; }
	  argbuf[i++] = 0;
	}
    }
  argvec[argc] = 0;
  init_tls ();
  init_environ ();
  run_init_arrays ();
  if (__ul64_init) __ul64_init (argc, argvec);				/* UnixLib 64, if it is linked in (riscos64/unixlib) */
  {								/* the shell starts a program by its full path but the name it was given is argv[0] (Sh$Argv0, read once and removed; after UnixLib's init, which makes argv[0] a full path): error messages say "sed:", not "/work/sed/sed:" */
    static char a0[256];
    unsigned long q[10] = { (unsigned long) "Sh$Argv0", (unsigned long) a0, sizeof a0 - 1, 0, 3 };
    if (argc > 0 && !swi_x (0x23, q) && q[2] > 0 && q[2] < sizeof a0 - 1)
      {
	unsigned long d[10] = { (unsigned long) "Sh$Argv0", (unsigned long) "", (unsigned long) -1, 0, 4, 0, 0, 0, 0, 0 };
	a0[q[2]] = 0; argvec[0] = a0;
	swi_x (0x24, d);					/* OS_SetVarVal: length -1 removes it */
      }
    { extern int unsetenv (const char *); unsetenv ("Sh$Argv0"); }					/* and from environ: a program that this one starts (env sed ...) would be given it again as its name */
  }
  program_invocation_name = argc > 0 ? argvec[0] : (char *) "";
  program_invocation_short_name = program_invocation_name;
  { char *q; for (q = program_invocation_name; *q; q++) if (*q == '/' || *q == '.' || *q == ':') program_invocation_short_name = q + 1; }
  exit (main (argc, argvec, envvec));
}
