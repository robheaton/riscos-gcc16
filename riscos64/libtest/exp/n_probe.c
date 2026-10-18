#include <stdio.h>
#include <string.h>
extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
static void err (const char *what, unsigned long e, unsigned long *r)
{
  if (e) { unsigned *b = (unsigned *) r[0]; printf ("%s: error %x \"%s\"\n", what, b ? b[0] : 0, b ? (char *) (b + 1) : "?"); }
}
int main (void)
{
  unsigned long r[10] = { 2, 1, 0 };	/* AF_INET, SOCK_STREAM, protocol */
  unsigned long e = swi_x (0x41200, r);
  long s;
  printf ("Socket_Creat -> %s r0=%lx\n", e ? "error" : "ok", r[0]);
  err ("creat", e, r);
  if (e) { printf ("no Internet module that answers: nothing more to probe\n"); return 0; }
  s = (long) r[0];
  {
    unsigned long q[10] = { (unsigned long) s, 0xffff, 4, 0, 4 };	/* setsockopt SOL_SOCKET SO_REUSEADDR */
    int one = 1;
    q[3] = (unsigned long) &one;
    e = swi_x (0x4120C, q);
    printf ("Setsockopt -> %s\n", e ? "error" : "ok"); err ("setsockopt", e, q);
  }
  {
    /* bind to 127.0.0.1:0 : struct sockaddr_in { u8 len; u8 family; u16 port; u32 addr; u8 zero[8] } */
    unsigned char sa[16] = { 16, 2, 0, 0, 127, 0, 0, 1 };
    unsigned long q[10] = { (unsigned long) s, (unsigned long) sa, 16 };
    e = swi_x (0x41201, q);
    printf ("Bind -> %s\n", e ? "error" : "ok"); err ("bind", e, q);
    q[0] = (unsigned long) s; q[1] = 4;
    e = swi_x (0x41202, q);
    printf ("Listen -> %s\n", e ? "error" : "ok"); err ("listen", e, q);
    {
      unsigned char name[16] = { 0 };
      unsigned long len = 16;
      unsigned long n[10] = { (unsigned long) s, (unsigned long) name, (unsigned long) &len };
      e = swi_x (0x4120F, n);
      printf ("Getsockname -> %s len %lu: %d.%d.%d.%d port %d\n", e ? "error" : "ok", len, name[4], name[5], name[6], name[7], name[2] * 256 + name[3]); err ("getsockname", e, n);
    }
  }
  {
    unsigned long q[10] = { (unsigned long) s };
    e = swi_x (0x41210, q);
    printf ("Close -> %s\n", e ? "error" : "ok");
  }
  {
    const char *host = "localhost";
    unsigned long q[10] = { (unsigned long) host };
    e = swi_x (0x46001, q);	/* Resolver_GetHost */
    printf ("Resolver_GetHost localhost -> %s r0=%lx r1=%lx\n", e ? "error" : "ok", q[0], q[1]); err ("gethost", e, q);
    if (!e && q[1]) { unsigned long *h = (unsigned long *) q[1]; printf ("hostent words: %lx %lx %lx %lx\n", h[0], h[1], h[2], h[3]); }
  }
  return 0;
}
