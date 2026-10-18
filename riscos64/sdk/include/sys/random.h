#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
ssize_t getrandom (void *, size_t, unsigned);
#ifdef __cplusplus
}
#endif
#endif
