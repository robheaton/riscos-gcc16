/* the second translation unit of cxxtest.cc: its static constructors run after those of the first (link order) */
#include <stdio.h>
struct Tracer2 { const char *n; Tracer2 (const char *s) : n (s) { printf ("ctor2 %s\n", n); } ~Tracer2 () { printf ("dtor2 %s\n", n); } };
static Tracer2 u1 ("u1");
int second_count = 7;
struct Count { Count () { second_count += 3; } } count_obj;
void second_unit (void) { static Tracer2 local ("local-in-second"); printf ("second_unit, second_count %d\n", second_count); }
