/* rawarg.c 1.0 -- NO UnixLib, no C library: what does the loader (SOMRun) hand to a program?  Prints the OS_GetEnv command string and the DDEUtils extended command line (size and text) exactly as found: the first
   reader of the DDEUtils line takes it away, and UnixLib programs read it at start-up, so only a program like this one can see it.  Static ELF, entered by SOMRun in USR mode with sp = the top of the application space.  */
typedef unsigned int u32;

static void wr0 (const char *s)
{
  register const char *r0 __asm__ ("r0") = s;
  __asm__ volatile ("swi 0x20002" : "+r" (r0) : : "memory", "cc");	/* XOS_Write0 */
}
static void wrc (int c)
{
  register int r0 __asm__ ("r0") = c;
  __asm__ volatile ("swi 0x20000" : "+r" (r0) : : "memory", "cc");	/* XOS_WriteC */
}
static void wrnum (u32 v)
{
  char b[12];
  int i = 0;
  if (v == 0) b[i++] = '0';
  while (v) { b[i++] = '0' + v % 10; v /= 10; }
  while (i) wrc (b[--i]);
}
static void wrtext (const char *s, u32 max)
{
  for (u32 i = 0; i < max && s[i]; i++)
    {
      unsigned char c = (unsigned char) s[i];
      if (c < 32 || c > 126) { wrc ('<'); wrnum (c); wrc ('>'); }
      else wrc (c);
    }
}
static inline const char *os_getenv (void)
{
  register const char *r0 __asm__ ("r0");
  register u32 r1 __asm__ ("r1");
  register u32 r2 __asm__ ("r2");
  __asm__ volatile ("swi 0x10" : "=r" (r0), "=r" (r1), "=r" (r2) : : "memory", "cc");	/* OS_GetEnv: r0 = the command string, r1 = the top of the application space, r2 = the time */
  return r0;
}
static inline int dde_getclsize (void)
{
  register int r0 __asm__ ("r0");
  __asm__ volatile ("swi 0x62583\n\tmovvs r0, #-1" : "=r" (r0) : : "memory", "cc");	/* XDDEUtils_GetCLSize */
  return r0;
}
static inline void dde_getcl (char *buffer)
{
  register char *r0 __asm__ ("r0") = buffer;
  __asm__ volatile ("swi 0x62584" : "+r" (r0) : : "memory", "cc");	/* XDDEUtils_GetCl: takes the text away */
}
static inline void os_exit (void)
{
  register u32 a0 __asm__ ("r0") = 0;
  register u32 a1 __asm__ ("r1") = 0x58454241;
  register u32 a2 __asm__ ("r2") = 0;
  __asm__ volatile ("swi 0x11" : : "r" (a0), "r" (a1), "r" (a2));		/* OS_Exit */
}
static u32 slen (const char *s) { u32 n = 0; while (s[n]) n++; return n; }

static char buf[4096];
__attribute__ ((used)) static char keep[16] = "rawarg";	/* a little initialised data: the loader wants a data segment with something in it */

void _start (void)
{
  const char *cmd = os_getenv ();
  wr0 ("rawarg 1.0: OS_GetEnv command string, length ");
  wrnum (slen (cmd));
  wr0 (": [");
  wrtext (cmd, 1000);
  wr0 ("]\r\n");

  int sz = dde_getclsize ();
  wr0 ("rawarg: DDEUtils command line size: ");
  if (sz < 0) wr0 ("(DDEUtils is not there)");
  else wrnum ((u32) sz);
  wr0 ("\r\n");
  if (sz > 0)
    {
      buf[0] = 0;
      dde_getcl (buf);
      wr0 ("rawarg: DDEUtils command line text, length ");
      wrnum (slen (buf));
      wr0 (": [");
      wrtext (buf, 4000);
      wr0 ("]\r\n");
    }
  os_exit ();
  for (;;) ;
}
