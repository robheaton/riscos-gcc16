/* the little of UnixLib's internal/unix.h that sys/brk.c and sys/stackalloc.c use.  struct ul_memory is the REAL one: build-and-run.sh cuts it out of the
   internal/unix.h of the sources under test into ul_memory_struct.h. */
#ifndef MODEL_INTERNAL_UNIX_H
#define MODEL_INTERNAL_UNIX_H
#include <stddef.h>
#include "ul_memory_struct.h"
struct __sul_process { void *(*sul_wimpslot) (int pid, void *newslot); int pid; };
struct ul_global { int dynamic_num; struct __sul_process *sulproc; int pthread_system_running; };
extern struct ul_memory __ul_memory;
extern struct ul_global __ul_global;
extern unsigned int __stackalloc_incr_wimpslot (unsigned int incr);
#endif
