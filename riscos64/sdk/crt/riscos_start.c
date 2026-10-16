/* riscos_start.c - the C start-up of a hosted program: copy the command line into argv[], set the limits of the heap (sbrk) and of the stack, run the constructors, call main and exit.
   The command line from OS_GetEnv is the whole line, the program first ("RAM::RamDisc0.$.hello a "b c" d"): words are separated by spaces, a word in double quotes may hold spaces. */
extern int main (int, char **, char **);
extern void exit (int);
extern void __libc_init_array (void);
extern char __heap_start[];

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
  __libc_init_array ();
  exit (main (argc, argvec, envvec));
}
