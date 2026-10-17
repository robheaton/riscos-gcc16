/* t10svc - the test of Mod64P (mod64p.S), test pack 0.7: how a 64-bit kernel calls the service entry of a module and how a service is claimed.  Run after RMLoad Mod64P.
   It sends five services with OS_ServiceCall (&59B01-&59B05), each with its own marker values in r0 and r2-r8, and writes down what OS_ServiceCall returns; then it asks the module, SWI by SWI, what its entry
   was given (x0-x5, x12, x30 and the 16 words at x1 when the number was in x0).  Nothing here can hurt: the module reads memory only where x1 looks like an address. */
#include "t64.h"
extern u64 swi_x (u64 swi, u64 *regs);

static int lookup (const char *name, u64 *num)
{
  u64 r[10] = { 0 };
  int v;
  r[1] = (u64) name;
  v = (int) swi_x (0x39, r);				/* OS_SWINumberFromString */
  if (v) { t_puts ("INFO OS_SWINumberFromString of "); t_puts (name); t_puts (": failed\n"); return 0; }
  *num = r[0];
  return 1;
}
static void clear (u64 *r) { int i; for (i = 0; i < 10; i++) r[i] = 0; }
static void info2 (const char *a, int n, const char *b, u64 val)	/* INFO "a n b" = &val */
{
  char k[96];
  u64 la = strlen (a), lb = strlen (b);
  memcpy (k, a, la); k[la] = ' '; k[la + 1] = (char) ('0' + n); k[la + 2] = ' '; memcpy (k + la + 3, b, lb + 1);
  t_info (k, val);
}
static void rec_word (int rec, int w, u64 val)		/* INFO record R word WW = &val */
{
  char k[40] = "record ";
  k[7] = (char) ('0' + rec); k[8] = ' '; k[9] = 'w'; k[10] = 'o'; k[11] = 'r'; k[12] = 'd'; k[13] = ' '; k[14] = (char) ('0' + w / 10); k[15] = (char) ('0' + w % 10); k[16] = 0;
  t_info (k, val);
}

int main (void)
{
  u64 r[10], sw[5], slot = 0, count;
  static const char *const names[5] = { "Mod64P_Setup", "Mod64P_Get", "Mod64P_Count", "Mod64P_Add", "Mod64P_Add32" };
  int i, k, w, v, ok64, ok32;
  t_puts ("RISC OS 64 test t10svc\n");
  for (i = 0; i < 5; i++) if (!lookup (names[i], &sw[i])) { t_check (0, "the SWIs of Mod64P are there"); return t_summary ("t10svc"); }
  t_check (1, "the SWIs of Mod64P are there");

  clear (r); r[0] = 40; r[1] = 2; v = (int) swi_x (sw[3], r); ok64 = (v == 0 && r[0] == 42);
  clear (r); r[0] = 40; r[1] = 2; v = (int) swi_x (sw[4], r); ok32 = (v == 0 && (r[0] & 0xFFFFFFFF) == 42);
  slot = ok64 ? 8 : ok32 ? 4 : 0;
  t_puts ("INFO the SWI register block has slots of "); t_puts (ok64 ? "64 bits" : ok32 ? "32 bits" : "an unknown size"); t_putc ('\n');
  t_check (ok64 || ok32, "one of the two slot sizes works");
  if (!slot) return t_summary ("t10svc");
  clear (r); r[0] = slot; swi_x (sw[0], r);

  /* five services: 0 is only recorded, 1 to 4 each claim in a different way (see mod64p.S) */
  for (k = 0; k < 5; k++)
    {
      clear (r);
      r[0] = 0xA0 + k; r[1] = 0x59B01 + k; r[2] = 0xC2; r[3] = 0xC3; r[4] = 0xC4; r[5] = 0xC5; r[6] = 0xC6; r[7] = 0xC7; r[8] = 0xC8;
      v = (int) swi_x (0x30, r);			/* OS_ServiceCall */
      info2 ("OS_ServiceCall for service no.", k, "V", (u64) v);
      info2 ("OS_ServiceCall for service no.", k, "r0 after", r[0]);
      info2 ("OS_ServiceCall for service no.", k, "r1 after (0 = claimed)", r[1]);
      info2 ("OS_ServiceCall for service no.", k, "r2 after", r[2]);
      info2 ("OS_ServiceCall for service no.", k, "r3 after", r[3]);
    }
  clear (r); swi_x (sw[2], r); count = r[0];
  t_info ("services seen by the entry of Mod64P (the five sent, and none other with a number in this range)", count);
  t_check (count >= 5, "the kernel called the service entry of Mod64P for all five services");

  for (k = 0; k < 5; k++)
    {
      u64 first;
      for (w = 0; w < 32; w += 8)
	{
	  clear (r); r[0] = (u64) k; r[1] = (u64) w;
	  v = (int) swi_x (sw[1], r);
	  if (v || (r[0] == (u64) -1 && w > 24)) { t_puts ("INFO Mod64P_Get failed\n"); break; }
	  for (i = 0; i < 8 && w + i < 28; i++) rec_word (k, w + i, r[i]);
	  if (w == 0) first = r[0];
	}
      (void) first;
    }
  t_puts ("INFO  words: 0 = which register held the number (2 = x0, 1 = x1), 1-6 = x0-x5, 7 = x12, 8 = x30, 9-24 = the 16 words at x1, 26 = the record, 27 = 1 if the words were read\n");
  return t_summary ("t10svc");
}
