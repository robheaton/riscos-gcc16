/* alloca + stack adjustment: exercises dwarf2cfi.cc / ira.cc RISC OS hunks. */
#include <alloca.h>
#include <string.h>
extern void use(char *, int);
void with_alloca(const char *s, int n) {
    char *p = alloca(n + 1);
    memcpy(p, s, n); p[n] = 0;
    use(p, n);
}
