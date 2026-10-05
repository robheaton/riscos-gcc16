/* mock_os.c -- a model of the little of RISC OS that ARMEABISupport's mmap/page allocator code uses: dynamic areas with a page-mapped pool (OS_DynamicArea 0/1/21/22/25), the RMA (OS_Module 6/7),
   a few helpers.  Everything the allocator code of the module asks the OS is answered here, and every leak can be counted: dynamic areas, claimed physical pages, RMA blocks.
   The model is as strict as the documentation says: a claim of a PMP page index at or above the size of the pool fails, a page can only be mapped inside the logical window of the area (the maximum size,
   cut down to the OS clamp), and a failing list is applied up to the failing entry (the worst case for the caller).  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "kernel.h"
#include "types.h"
#include "memory.h"
#include "swi.h"
#include "main.h"
#include "abort.h"
#include "shm.h"
#include "debug.h"
#include "mock_os.h"

_kernel_oserror mock_err[16] = {
  { 0x81dc20, "bad reason" }, { 0x81dc21, "bad param" }, { 0x81dc22, "no memory" }, { 0x81dc23, "bad process" }, { 0x81dc24, "in use" }, { 0x81dc25, "page map error" },
  { 0x81dc26, "EACCES" }, { 0x81dc27, "EEXIST" }, { 0x81dc28, "EINVAL" }, { 0x81dc29, "ENAMETOOLONG" }, { 0x81dc2a, "ENOENT" }, { 0x81dc2b, "ENOSPC" }, { 0x81dc2c, "EBADF" }, { 0x81dc2d, "ENOMEM" },
  { 0x81dc2e, "EOPSYS" }, { 0, "" } };
static _kernel_oserror os_err_claim = { 0x1b0, "PMP: claim failed (no memory or page index out of range)" };
static _kernel_oserror os_err_map = { 0x1b1, "PMP: map failed (page outside the window, or not claimed)" };
static _kernel_oserror os_err_addr = { 0x1b2, "Unable to allocate logical address space" };
static _kernel_oserror os_err_rma = { 0x1b3, "RMA: no room" };
static _kernel_oserror os_err_da = { 0x1b4, "unknown dynamic area" };

struct mock_state mock;                       /* the knobs and the counters */
extern armeabisupport_globals global;
static app_object apps[4];

#define MAXDA 512
struct mock_da { int used; unsigned number; char name[40]; unsigned base; size_t max_pages, window_pages; int *phys; int *dapage; size_t nclaimed; };
static struct mock_da das[MAXDA];

void mock_reset(void)
{
  memset(&global, 0, sizeof global); memset(apps, 0, sizeof apps);     /* the module starts every scenario empty */
  for (int i = 0; i < MAXDA; i++) { if (das[i].used) { munmap((void *)(uintptr_t) das[i].base, das[i].window_pages << 12); free(das[i].phys); free(das[i].dapage); } memset(&das[i], 0, sizeof das[i]); }
  memset(&mock, 0, sizeof mock);
  mock.phys_free = 1000000;                    /* pages of 4 KB: ~3.8 GB */
  mock.clamp_pages = 128 * 256;                /* the OS clamp on the maximum size of a dynamic area: 128 MB */
  mock.next_base = 0x6ee67000u; mock.addr_end = 0x6ee67000u + 2080u * 1024 * 1024;   /* ~2.08 GB of address space for dynamic areas */
  mock.next_number = 256;
  mock.partial_claims = 1;
  mock.verbose = getenv("MOCK_TRACE") != NULL;
}
static struct mock_da *find(int n) { for (int i = 0; i < MAXDA; i++) if (das[i].used && das[i].number == (unsigned) n) return &das[i]; return NULL; }

/* ---------------- the RMA ---------------- */
_kernel_oserror *rma_claim(int size, void **ret)
{
  *ret = NULL;
  if (mock.verbose) fprintf(stderr, "   [rma_claim %d (fail_after=%d) from %p]\n", size, mock.rma_fail_after, __builtin_return_address(0));
  if (size <= 0 || size > 64 * 1024 * 1024 || mock.rma_fail_after == 1) return &os_err_rma;
  if (mock.rma_fail_after > 1) mock.rma_fail_after--;
  void *p = malloc((size_t) size);
  if (!p) return &os_err_rma;
  memset(p, 0xA5, (size_t) size);              /* like uninitialised RMA: a garbage fill, so that a field that is not set shows up */
  *ret = p; mock.rma_live++; mock.rma_bytes += size;
  return NULL;
}
_kernel_oserror *rma_free(void *p) { if (p) { free(p); mock.rma_live--; } return NULL; }

/* ---------------- dynamic areas ---------------- */
_kernel_oserror *dynamic_area_create(const char *name, int max_size, unsigned type, dynamicarea_block *ret)
{
  (void) type;
  struct mock_da *d = NULL;
  for (int i = 0; i < MAXDA; i++) if (!das[i].used) { d = &das[i]; break; }
  if (!d) return &os_err_addr;
  size_t max_pages = (size_t) (((unsigned) max_size + 4095u) >> 12);
  size_t window = max_pages < mock.clamp_pages ? max_pages : mock.clamp_pages;   /* the OS cuts the window down to the clamp, silently */
  unsigned long long bytes = (unsigned long long) window << 12;
  if (mock.next_base + bytes > mock.addr_end) return &os_err_addr;
  void *win = mmap((void *)(uintptr_t) mock.next_base, (size_t) bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
  if (win != (void *)(uintptr_t) mock.next_base) { if (win != MAP_FAILED) munmap(win, (size_t) bytes); return &os_err_addr; }
  memset(d, 0, sizeof *d);
  d->used = 1; d->number = mock.next_number++; strncpy(d->name, name, sizeof d->name - 1);
  d->base = mock.next_base; mock.next_base += (unsigned) bytes;
  d->max_pages = max_pages; d->window_pages = window;
  d->phys = malloc(max_pages * sizeof(int)); d->dapage = malloc(max_pages * sizeof(int));
  for (size_t i = 0; i < max_pages; i++) { d->phys[i] = -1; d->dapage[i] = -1; }
  mock.da_live++; mock.da_created++;
  ret->number = d->number; ret->base = (eabi_PTR)(uintptr_t) d->base; ret->size = 0; ret->max_size = max_pages;
  return NULL;
}
_kernel_oserror *dynamic_area_remove(unsigned n)
{
  struct mock_da *d = find((int) n);
  if (!d) return &os_err_da;
  munmap((void *)(uintptr_t) d->base, d->window_pages << 12);
  mock.phys_free += (long) d->nclaimed;
  free(d->phys); free(d->dapage);
  d->used = 0; mock.da_live--;
  return NULL;
}
_kernel_oserror *dynamic_area_extend(unsigned n, size_t by) { (void) n; (void) by; return &os_err_da; }

/* OS_DynamicArea 21: claim / release physical pages of a PMP.  entry: pmp_page_index, physical_page_number (-2 kernel's choice, -1 release), flags */
_kernel_oserror *dynamic_pmp_claim_release(int da, pmp_phy_page_entry *list, int n)
{
  struct mock_da *d = find(da);
  if (!d) return &os_err_da;
  mock.claim_calls++;
  if (mock.partial_claims == 0) {              /* atomic variant: check first */
    for (int i = 0; i < n; i++) if (list[i].physical_page_number != -1 && (list[i].pmp_page_index < 0 || (size_t) list[i].pmp_page_index >= d->max_pages)) return &os_err_claim;
  }
  for (int i = 0; i < n; i++) {
    int idx = list[i].pmp_page_index;
    if (idx < 0 || (size_t) idx >= d->max_pages) return &os_err_claim;
    if (list[i].physical_page_number == -1) {                       /* release */
      if (d->phys[idx] == -1) return &os_err_claim;                  /* nothing there */
      if (d->dapage[idx] != -1) mprotect((void *)(uintptr_t) (d->base + ((unsigned) d->dapage[idx] << 12)), 4096, PROT_NONE);
      d->phys[idx] = -1; d->dapage[idx] = -1; d->nclaimed--; mock.phys_free++;
    } else {                                                          /* claim */
      if (d->phys[idx] != -1) return &os_err_claim;
      if (mock.phys_free <= 0) return &os_err_claim;
      mock.phys_free--; d->phys[idx] = 1; d->nclaimed++;
    }
  }
  return NULL;
}
/* OS_DynamicArea 22: map / unmap.  entry: da_page_number, pmp_page_index (-1 unmap), flags */
_kernel_oserror *dynamic_pmp_map_unmap(int da, pmp_log_page_entry *list, int n)
{
  struct mock_da *d = find(da);
  if (!d) return &os_err_da;
  mock.map_calls++;
  for (int i = 0; i < n; i++) {
    int dapage = list[i].da_page_number, idx = list[i].pmp_page_index;
    if (dapage < 0 || (size_t) dapage >= d->window_pages) return &os_err_map;      /* outside the logical window */
    if (idx == -1) {                                                    /* unmap whatever is at this da page */
      for (size_t k = 0; k < d->max_pages; k++) if (d->dapage[k] == dapage) d->dapage[k] = -1;   /* O(n) but only for the tests */
      mprotect((void *)(uintptr_t) (d->base + ((unsigned) dapage << 12)), 4096, PROT_NONE);
    } else {
      if (idx < 0 || (size_t) idx >= d->max_pages || d->phys[idx] == -1) return &os_err_map;
      d->dapage[idx] = dapage;
      mprotect((void *)(uintptr_t) (d->base + ((unsigned) dapage << 12)), 4096, PROT_READ | PROT_WRITE);
    }
  }
  return NULL;
}
/* OS_DynamicArea 25: page status */
_kernel_oserror *dynamic_pmp_page_info(int da, pmp_page_info_entry *list, int n)
{
  struct mock_da *d = find(da);
  if (!d) return &os_err_da;
  for (int i = 0; i < n; i++) {
    int idx = list[i].pmp_page_index;
    if (idx < 0 || (size_t) idx >= d->max_pages) return &os_err_claim;
    list[i].physical_page_number = d->phys[idx];
    list[i].da_page_number = d->dapage[idx];
    list[i].flags = 0;
  }
  return NULL;
}
_kernel_oserror *dynamic_pmp_resize(int da, int page_diff, int *changed) { (void) da; (void) page_diff; (void) changed; return &os_err_da; }

/* ---------------- heap and filters: not used by the mmap / page allocators ---------------- */
_kernel_oserror *heap_init(void *b, size_t s) { (void) b; (void) s; return &os_err_da; }
_kernel_oserror *heap_claim(void *b, size_t s, void **r) { (void) b; (void) s; *r = NULL; return &os_err_da; }
_kernel_oserror *heap_release(void *b, void *p) { (void) b; (void) p; return &os_err_da; }
_kernel_oserror *heap_extend(void *b, size_t by) { (void) b; (void) by; return &os_err_da; }
_kernel_oserror *heap_extend_block(void *b, void **p, size_t by) { (void) b; (void) p; (void) by; return &os_err_da; }
_kernel_oserror *heap_block_size(void *b, void *p, size_t *s) { (void) b; (void) p; *s = 0; return &os_err_da; }
void report_text(const char *t) { if (mock.verbose) fputs(t, stderr); }
_kernel_oserror *get_app_base(unsigned *r) { *r = 0x1000000; return NULL; }
_kernel_oserror *get_os_permissions(uint32_t in, uint32_t mask, uint32_t *out) { (void) mask; if (out) *out = in; return NULL; }
int get_access_permissions(unsigned a) { return (int) a; }

/* ---------------- the applications ---------------- */
app_object *mock_app(int n) { return &apps[n]; }
_kernel_oserror *app_find(app_object **r) { *r = &apps[mock.cur_app]; return NULL; }
_kernel_oserror *app_new(app_object **r) { *r = &apps[mock.cur_app]; return NULL; }
_kernel_oserror *abort_deregister(void *routine, unsigned r12) { (void) routine; (void) r12; return NULL; }
void *get_mmap_abort_handler(void) { return (void *) 0; }
bool shm_deref_object(shm_object *s) { (void) s; return 0; }
void shm_dump_object_list(void) {}
void shm_cleanup_app(app_object *a) { (void) a; }
void log_printf(const char *f, ...) { (void) f; }
void reporter_printf(const char *f, ...) { (void) f; }
void report_swi_error(const char *fn, int line, _kernel_oserror *e) { if (mock.verbose) fprintf(stderr, "%s:%d %s\n", fn, line, e->errmess); }
armeabisupport_globals global;

/* ---------------- what the tests look at ---------------- */
void mock_stats(struct mock_stats *s)
{
  memset(s, 0, sizeof *s);
  for (int i = 0; i < MAXDA; i++) if (das[i].used) { s->areas++; s->claimed += das[i].nclaimed; for (size_t k = 0; k < das[i].max_pages; k++) if (das[i].dapage[k] != -1) s->mapped++; }
  s->rma_live = mock.rma_live; s->phys_used = 1000000 - mock.phys_free;
}
