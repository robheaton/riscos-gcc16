#ifndef MOCK_OS_H
#define MOCK_OS_H
#include "types.h"
struct mock_state { long phys_free; size_t clamp_pages; unsigned next_base; unsigned long long addr_end; unsigned next_number; int partial_claims; int verbose; int cur_app;
                    int rma_live; long rma_bytes; int rma_fail_after; int da_live, da_created; long claim_calls, map_calls; };
struct mock_stats { int areas; unsigned long claimed, mapped; int rma_live; long phys_used; };
extern struct mock_state mock;
void mock_reset(void);
void mock_stats(struct mock_stats *);
app_object *mock_app(int n);
#endif
