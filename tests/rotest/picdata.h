/* Position-independent-code test data/functions (picdata.c).  Linked into the executable
   in the plain builds and built into libfooNN.so in the shared-library builds, so the same
   checks exercise GOT access, data relocations and callbacks either way. */
#ifndef PICDATA_H
#define PICDATA_H

struct pd_node { struct pd_node *next; int v; };

extern int pd_counter;                    /* exported variable */
extern int *pd_ptr;                       /* pointer to a static (data relocation) */
extern int (*pd_funcs[3])(int);           /* table of function pointers (relocations) */
extern const char *pd_names[3];           /* table of string pointers (relocations) */
extern struct pd_node *pd_head;           /* linked list built from statics */
extern const int *const pd_cptrs[2];      /* const table of pointers (.data.rel.ro) */
extern int pd_ext;                        /* defined by the main program, used by pd_read_ext() */

int pd_sum_list(void);
int pd_call(int which, int x);
const char *pd_name(int i);
int pd_via_ptr(void);
int pd_read_ext(void);
int pd_apply(int (*cb)(int), int x);      /* calls back into the main program */
int pd_cptr_sum(void);
int pd_switch(int x);                     /* dense switch: PIC jump table */
int pd_static_count(void);                /* function-local static */
#endif
