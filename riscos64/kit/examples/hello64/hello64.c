/* hello64.c - a small C module for RISC OS 64 made with the kit: two commands, three SWIs, a service call, an error return, .data, .bss and a table of pointers (which the module relocates itself).
   aarch64-riscos-gcc -mmodule -O2 hello64.c hello64_hdr.S -o Hello64   makes Hello64,ffa. */
#include "modkit64.h"

#define SWIBASE 0x59A80
#define SVC_NUMBER 0x59A81

static int calls = 100;				/* .data */
static int service_calls;			/* .bss (the kit clears it when the module starts) */
static uint32_t history[64];			/* .bss */
static const char *const greetings[] = { "hello", "bonjour", "hallo" };	/* a table of pointers: .data.rel.ro, relocated by the module itself */
static const mk_oserr err_static = { 0x20B01, "Hello64: this error is in the module's code" };

mk_oserr *module_init (const char *tail, int instance, void *pw)
{
  mk_printf ("Hello64: started (tail \"%s\", instance %d, history at %p, calls %d, history[5] %u)\n", tail ? tail : "", instance, (void *) history, calls, history[5]);
  return 0;
}

mk_oserr *module_final (int fatal, int podule, void *pw)
{
  mk_printf ("Hello64: finished (%d calls, %d service calls)\n", calls - 100, service_calls);
  return 0;
}

mk_oserr *module_swi (int offset, mk_reg *regs, void *pw)
{
  calls++;
  switch (offset)
    {
    case 0: regs[0] = regs[0] + regs[1]; return 0;				/* Hello64_Add: r0 = r0 + r1 */
    case 1:									/* Hello64_Info: r0 = calls, r1 = service calls, r2 = the sum of the history (bss), r3 = the number of greetings, r4 = a pointer from the table is right */
      {
	uint32_t sum = 0;
	for (int i = 0; i < 64; i++) sum += history[i];
	regs[0] = (mk_reg) (calls - 100); regs[1] = (mk_reg) service_calls; regs[2] = sum; regs[3] = sizeof greetings / sizeof greetings[0];
	regs[4] = greetings[1][0] == 'b' && greetings[2][3] == 'l' && !strcmp (greetings[0], "hello");
	history[(calls - 100) & 63] += (uint32_t) regs[0];
	return 0;
      }
    case 2: return (mk_oserr *) &err_static;					/* Hello64_Fail: an error block in the module's own code; the kit copies it where a program can read it */
    default: return MK_NOT_KNOWN;
    }
}

mk_oserr *module_command (int command, const char *args, int argc, void *pw)
{
  if (command == 0)								/* Hello64_Say [<name>] */
    {
      mk_printf ("%s %s (%d parameter%s)\n", greetings[calls % 3], argc ? args : "world", argc, argc == 1 ? "" : "s");
      return 0;
    }
  return mk_error (0x20B02, "Hello64: this error was made with mk_error");	/* Hello64_Error */
}

void module_service (int service, mk_reg *regs, void *pw)
{
  if (service == SVC_NUMBER) service_calls++;
}
