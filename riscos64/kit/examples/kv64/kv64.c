/* kv64.c - Kv64: a key/value store.  The entries are a linked list in the RMA (mk_rma_claim), so they live as long as the module.
   SWIs (base 0x59C40):  Kv64_Set    r0 = key, r1 = value (both NUL-terminated strings)             -> r0 = 1 if the key is new, 0 if a value was replaced
                         Kv64_Get    r0 = key, r1 = buffer (0: only the size), r2 = its size         -> r2 = the length of the value with its NUL; error if the key is not there or the buffer is too small
                         Kv64_Delete r0 = key                                                         -> r0 = 1 if it was there
                         Kv64_Count                                                                   -> r0 = the number of keys
                         Kv64_Enum   r0 = index (0 ...)                                               -> r1 = the key (valid until the next change), r2 = its value; r1 = 0 past the end
                         Kv64_Clear                                                                   -> r0 = the number removed
   Commands: *Kv_Set key value, *Kv_Get key, *Kv_List, *Kv_Clear.   cmunge64 -d kv64h.h -o kv64_hdr.o kv64.cmhg;  aarch64-riscos-gcc -mmodule -O2 kv64.c kv64_hdr.o -o Kv64,ffa */
#include <stdint.h>
#include <string.h>
#include "kernel.h"
#include "modkit64.h"
#include "kv64h.h"

struct entry { struct entry *next; char *value; char key[1]; };	/* the value is stored after the key */
static struct entry *head;
static int count;

static struct entry *find (const char *key)
{
  struct entry *e;
  for (e = head; e; e = e->next) if (!strcmp (e->key, key)) return e;
  return 0;
}
static void release (struct entry *e) { mk_rma_free (e); }

static int set (const char *key, const char *value)
{
  size_t kl = strlen (key) + 1, vl = strlen (value) + 1;
  struct entry *e = find (key), *n;
  if (e && strlen (e->value) + 1 == vl) { memcpy (e->value, value, vl); return 0; }		/* same length: in place */
  n = mk_rma_claim (sizeof *n + kl + vl);
  if (!n) return -1;
  memcpy (n->key, key, kl); n->value = n->key + kl; memcpy (n->value, value, vl);
  if (e)								/* replace: put the new entry where the old one was */
    {
      struct entry **pp;
      for (pp = &head; *pp != e; pp = &(*pp)->next) ;
      n->next = e->next; *pp = n; release (e);
      return 0;
    }
  n->next = head; head = n; count++;
  return 1;
}
static int del (const char *key)
{
  struct entry **pp;
  for (pp = &head; *pp; pp = &(*pp)->next)
    if (!strcmp ((*pp)->key, key)) { struct entry *e = *pp; *pp = e->next; release (e); count--; return 1; }
  return 0;
}
static int clear (void)
{
  int n = 0;
  while (head) { struct entry *e = head; head = e->next; release (e); n++; }
  count = 0;
  return n;
}

_kernel_oserror *kv_init (const char *tail, int podule_base, void *pw) { (void) tail; (void) podule_base; (void) pw; head = 0; count = 0; return 0; }
_kernel_oserror *kv_final (int fatal, int podule_base, void *pw) { (void) fatal; (void) podule_base; (void) pw; clear (); return 0; }

#define STR(x) ((const char *) (uintptr_t) (x))
_kernel_oserror *kv_swi (int offset, _kernel_swi_regs *r, void *pw)
{
  struct entry *e;
  (void) pw;
  switch (offset)
    {
    case Kv64_Set - Module_SWIChunk:
      if (!r->r[0] || !r->r[1]) return mk_error (0x20C40, "Kv64: key and value are needed");
      { int rc = set (STR (r->r[0]), STR (r->r[1])); if (rc < 0) return mk_error (0x20C41, "Kv64: no room in the RMA"); r->r[0] = (mk_reg) rc; }
      return 0;
    case Kv64_Get - Module_SWIChunk:
      if (!r->r[0]) return mk_error (0x20C40, "Kv64: key and value are needed");
      e = find (STR (r->r[0]));
      if (!e) return mk_error (0x20C42, "Kv64: no such key");
      {
	size_t need = strlen (e->value) + 1;
	if (r->r[1]) { if (r->r[2] < need) return mk_error (0x20C43, "Kv64: the buffer is too small"); memcpy ((char *) (uintptr_t) r->r[1], e->value, need); }
	r->r[2] = (mk_reg) need;
      }
      return 0;
    case Kv64_Delete - Module_SWIChunk:
      if (!r->r[0]) return mk_error (0x20C40, "Kv64: key and value are needed");
      r->r[0] = (mk_reg) del (STR (r->r[0]));
      return 0;
    case Kv64_Count - Module_SWIChunk: r->r[0] = (mk_reg) count; return 0;
    case Kv64_Enum - Module_SWIChunk:
      {
	uint32_t i = r->r[0];
	for (e = head; e && i; e = e->next) i--;
	r->r[1] = e ? (mk_reg) (uintptr_t) e->key : 0; r->r[2] = e ? (mk_reg) (uintptr_t) e->value : 0;
	return 0;
      }
    case Kv64_Clear - Module_SWIChunk: r->r[0] = (mk_reg) clear (); return 0;
    default: return error_BAD_SWI;
    }
}

_kernel_oserror *kv_command (const char *arg_string, int argc, int number, void *pw)
{
  char key[64];
  const char *p = arg_string;
  size_t n = 0;
  struct entry *e;
  (void) pw;
  while (*p && *p != ' ' && *p != '\t' && n < sizeof key - 1) key[n++] = *p++;
  key[n] = 0;
  while (*p == ' ' || *p == '\t') p++;
  switch (number)
    {
    case CMD_Kv_Set: if (set (key, p) < 0) return mk_error (0x20C41, "Kv64: no room in the RMA"); return 0;
    case CMD_Kv_Get: e = find (key); if (!e) return mk_error (0x20C42, "Kv64: no such key"); mk_printf ("%s\n", e->value); return 0;
    case CMD_Kv_List: for (e = head; e; e = e->next) mk_printf ("%s = %s\n", e->key, e->value); mk_printf ("%d key%s\n", count, count == 1 ? "" : "s"); return 0;
    case CMD_Kv_Clear: mk_printf ("%d removed\n", clear ()); return 0;
    }
  (void) argc;
  return 0;
}
