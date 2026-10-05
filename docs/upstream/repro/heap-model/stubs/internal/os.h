/* the little of UnixLib's internal/os.h that sys/brk.c and sys/stackalloc.c use */
#ifndef MODEL_INTERNAL_OS_H
#define MODEL_INTERNAL_OS_H
#include <errno.h>
#define __DA_WIMPSLOT_ALIGNMENT (32 * 1024 - 1)       /* incl-local/features.h */
#define OS_ChangeDynamicArea 0x2A
#define __set_errno(e) (errno = (e), -1)
typedef struct { int errnum; char errmess[16]; } _kernel_oserror;
extern _kernel_oserror *__os_swi (int swinum, int *regs);
extern void __pthread_protect_unsafe (void);
#endif
