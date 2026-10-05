/* See picdata.h */
#include "picdata.h"

int pd_counter = 0;
static int pd_static = 5;
int *pd_ptr = &pd_static;

static int pd_f1(int x) { return x + 1; }
static int pd_f2(int x) { return x * 2; }
static int pd_f3(int x) { return x - 3; }
int (*pd_funcs[3])(int) = { pd_f1, pd_f2, pd_f3 };

const char *pd_names[3] = { "alpha", "beta", "gamma" };

struct pd_node pd_n3 = { 0, 3 };
struct pd_node pd_n2 = { &pd_n3, 2 };
struct pd_node pd_n1 = { &pd_n2, 1 };
struct pd_node *pd_head = &pd_n1;

static const int pd_const_tab[4] = { 10, 20, 30, 40 };
const int *const pd_cptrs[2] = { &pd_const_tab[1], &pd_const_tab[3] };

int pd_sum_list(void)
{
    int s = 0;
    for (struct pd_node *p = pd_head; p; p = p->next)
        s += p->v;
    return s;
}

int pd_call(int which, int x)
{
    pd_counter++;
    return pd_funcs[which % 3](x);
}

const char *pd_name(int i) { return pd_names[i % 3]; }

int pd_via_ptr(void) { return *pd_ptr + pd_static; }

int pd_read_ext(void) { return pd_ext + 1; }

int pd_apply(int (*cb)(int), int x)
{
    return cb(x) + pd_counter;
}

int pd_cptr_sum(void) { return *pd_cptrs[0] + *pd_cptrs[1]; }

int pd_switch(int x)
{
    switch (x) {
    case 0: return 100;
    case 1: return 211;
    case 2: return 322;
    case 3: return 433;
    case 4: return 544;
    case 5: return 655;
    case 6: return 766;
    case 7: return 877;
    case 8: return 988;
    case 9: return 1099;
    case 10: return 1210;
    case 11: return 1321;
    default: return -1;
    }
}

int pd_static_count(void)
{
    static int n = 40;
    return ++n;
}
