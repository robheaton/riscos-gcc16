/* daprobet.c -- daprobe.c (tests/unixlib17) with LOG LINES of its own, for the hunt of the freeze that RunSul7 stage c1 caused (a vfork child that ends without exec, then a vfork + exec of daprobe -m: the machine froze
   after the exec'd daprobe had printed its line).  Every mark goes to the SulLog file (sultrace.h) that the TRACED SharedUnixLibrary writes its lines to: at the start of main, before main returns, in an atexit
   handler, in a destructor.  The dynamic area heap ("daprobe heap", __dynamic_da_name) is kept: the clean RunTrace1 used a program without one.
   daprobe.c itself:
   READ-ONLY look at the dynamic areas and at the limits the OS puts on their maximum size.  Changes nothing.
   Prints: the clamps of OS_DynamicArea 8 (reasons 0 + R5 = -1 and R5 > 0), the RAM, the sum of the maximum sizes of all dynamic areas and a table of the areas (number, name, base, current
   size, maximum size, flags) - including "UnixLib stacks" (the one range of address space that ALL EABI stacks share) and the heap area of this program.  */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dautil.h"
#include "sultrace.h"

const char *const __dynamic_da_name = "daprobe heap";

static const char *mbs(unsigned v, char *buf)
{
  if ((int) v == -1) return "-1";
  snprintf(buf, 32, "%u MB", v >> 20);
  return buf;
}

/* daprobe -m : only the pool of ARMEABISupport's mmap areas ("mmap#N", 100 MB of address space each): how many exist and which, as  number(KB in use) , the KB in use in all of them
   (an area that is only a leftover has 0), and the free pool (the OS reports at most 2047 MB of it) */
static int mmap_pool(void)
{
  int n = 0; char list[768] = ""; unsigned long long total = 0, used = 0;
  for (int a = da_next(-1), guard = 0; a != -1 && guard < 500; a = da_next(a), guard++) {
    struct da_info t;
    if (da_get(a, &t) || strncmp(t.name, "mmap#", 5)) continue;
    n++; total += t.max; used += t.size;
    if (strlen(list) < sizeof list - 40) { char b[64]; snprintf(b, sizeof b, " %.20s(%uK)", t.name + 5, t.size >> 10); strcat(list, b); }
  }
  struct da_info f, s; unsigned long long freemb = 0, stk = 0;
  if (!da_find("Free pool", &f)) freemb = f.size >> 20;
  if (!da_find("UnixLib stacks", &s)) stk = s.size >> 10;
  printf("mmap pool: %d area(s), %llu MB of address space reserved:%s; %llu KB in use; free pool %llu MB; UnixLib stacks %llu KB in use\n", n, total >> 20, n ? list : " (none)", used >> 10, freemb, stk);
  return 0;
}

static void mark_atexit(void) { sl_mark("daprobet: the atexit handler runs", 0); }
static __attribute__((destructor)) void mark_destructor(void) { sl_mark("daprobet: the destructor runs", 0); }

int main(int argc, char **argv)
{
  sl_mark("daprobet: main starts", (unsigned) argc);
  atexit(mark_atexit);
  if (argc > 1 && !strcmp(argv[1], "-m")) {
    int rc = mmap_pool();
    fflush(stdout);
    sl_mark("daprobet: mmap_pool done, stdout flushed, main returns", (unsigned) rc);
    return rc;
  }
  char b1[32], b2[32];
  printf("daprobe 1.4   (fix level of libunixlib: %ld)\n", sysconf(0x4700));
  unsigned c1, c2;
  if (da_clamps(&c1, &c2)) printf("OS_DynamicArea 8 (read the clamps): not available on this OS\n");
  else {
    printf("OS_DynamicArea 8: clamp on the maximum size of areas created with R5 = -1: %s (0x%08x);  with R5 > 0 (what every UnixLib heap and the stack range ask): %s (0x%08x)\n",
           mbs(c1, b1), c1, mbs(c2, b2), c2);
    printf("   (-1 = the RAM limit of the machine; anything above the RAM limit is the same as -1; an area that asks for more than the clamp is silently given the clamp as its maximum)\n");
  }
  unsigned pagesz = 0, npages = 0;
  if (!_swix(OS_ReadMemMapInfo, _OUTR(0, 1), &pagesz, &npages)) printf("RAM: %u pages of %u bytes = %u MB\n", npages, pagesz, (unsigned) ((unsigned long long) npages * pagesz >> 20));
  printf("max OS_DynamicArea 2 = the maximum as asked at creation; max OS_ReadDynamicArea = the real one (what getrlimit RLIMIT_DATA reports); gap = distance to the next area's base\n");
  printf("%4s  %-28s %10s %10s %9s %9s %9s  %s\n", "area", "name", "base", "size KB", "max2 MB", "maxRD MB", "gap MB", "flags");
  unsigned long long summax = 0; int count = 0; struct da_info all[200]; int na = 0;
  for (int n = da_next(-1), guard = 0; n != -1 && guard < 200; n = da_next(n), guard++) if (!da_get(n, &all[na])) na++;
  for (int k = 0; k < na; k++) {
    unsigned next = 0xFFFFFFFFu;                       /* the lowest base above this one: the room this area really has */
    for (int j = 0; j < na; j++) if (all[j].base > all[k].base && all[j].base < next) next = all[j].base;
    char nm[40]; strcpy(nm, all[k].name); for (char *c = nm; *c; c++) if (*c < 32) *c = '/';
    if (next == 0xFFFFFFFFu) printf("%4d  %-28s 0x%08x %10u %9u %9u %9s  0x%08x\n", all[k].number, nm, all[k].base, all[k].size >> 10, all[k].max >> 20, all[k].emax >> 20, "-", all[k].flags);
    else printf("%4d  %-28s 0x%08x %10u %9u %9u %9u  0x%08x\n", all[k].number, nm, all[k].base, all[k].size >> 10, all[k].max >> 20, all[k].emax >> 20, (next - all[k].base) >> 20, all[k].flags);
    summax += all[k].emax; count++;
  }
  printf("%d areas; sum of their real maximum sizes %llu MB\n", count, summax >> 20);
  struct da_info st;
  if (!da_find("UnixLib stacks", &st)) printf("the shared range of address space for ALL EABI stacks (\"UnixLib stacks\"): maximum asked %u MB, real maximum %u MB, in use at this moment up to %u KB\n", st.max >> 20, st.emax >> 20, st.size >> 10);
  else printf("no area called \"UnixLib stacks\"\n");
  /* a few OS variables that say what this system is */
  const char *vars[] = { "Boot$OSVersion", "Boot$Dir", "SharedLibs$Dir", "SharedLibs$Path", "ARMEABISupport$Dir", NULL };
  for (int k = 0; vars[k]; k++) {
    char buf[256]; int len = 0;
    if (_swix(OS_ReadVarVal, _INR(0, 4) | _OUT(2), vars[k], buf, sizeof buf - 1, 0, 3, &len) || len <= 0) printf("%s: (not set)\n", vars[k]);
    else { buf[len] = 0; printf("%s: %s\n", vars[k], buf); }
  }
  return 0;
}
