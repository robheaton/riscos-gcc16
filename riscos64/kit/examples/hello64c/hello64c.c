/* hello64c.c - Hello64 with a CMHG file: the handlers have the names and the types of a 32-bit CMHG module (the header hello64h.h is written by cmunge64.py).
   cmunge64.py -d hello64h.h -o hello64_hdr.o hello64.cmhg;  aarch64-riscos-gcc -mmodule -O2 hello64c.c hello64_hdr.o -o Hello64 */
#include "kernel.h"
#include "modkit64.h"
#include "hello64h.h"

static int calls = 100;				/* .data */
static int service_calls;			/* .bss */
static unsigned history[64];			/* .bss */
static const char *const greetings[] = { "hello", "bonjour", "hallo" };	/* .data.rel.ro: relocated by the module itself */
static const _kernel_oserror err_static = { 0x20B01, "Hello64: this error is in the module's code" };

_kernel_oserror *hello_init (const char *tail, int podule_base, void *pw)
{
  mk_printf ("Hello64: started (tail \"%s\", instance %d, history at %p, calls %d, history[5] %u)\n", tail ? tail : "", podule_base, (void *) history, calls, history[5]);
  return 0;
}

_kernel_oserror *hello_final (int fatal, int podule_base, void *pw)
{
  mk_printf ("Hello64: finished (%d calls, %d service calls)\n", calls - 100, service_calls);
  return 0;
}

_kernel_oserror *hello_swi (int swi_offset, _kernel_swi_regs *r, void *pw)
{
  calls++;
  switch (swi_offset)
    {
    case Hello64_Add - Module_SWIChunk: r->r[0] = r->r[0] + r->r[1]; return 0;
    case Hello64_Info - Module_SWIChunk:
      {
	unsigned sum = 0;
	for (int i = 0; i < 64; i++) sum += history[i];
	r->r[0] = calls - 100; r->r[1] = service_calls; r->r[2] = (mk_sreg) sum; r->r[3] = sizeof greetings / sizeof greetings[0];
	r->r[4] = greetings[1][0] == 'b' && greetings[2][3] == 'l' && !strcmp (greetings[0], "hello");
	history[(calls - 100) & 63] += (unsigned) r->r[0];
	return 0;
      }
    case Hello64_Fail - Module_SWIChunk: return (_kernel_oserror *) &err_static;
    default: return error_BAD_SWI;
    }
}

_kernel_oserror *hello_command (const char *arg_string, int argc, int number, void *pw)
{
  if (number == CMD_Hello64_Say)
    {
      mk_printf ("%s %s (%d parameter%s)\n", greetings[calls % 3], argc ? arg_string : "world", argc, argc == 1 ? "" : "s");
      return 0;
    }
  return mk_error (0x20B02, "Hello64: this error was made with mk_error");
}

void hello_service (int service_number, _kernel_swi_regs *r, void *pw)
{
  if (service_number == 0x59A81) service_calls++;
}
