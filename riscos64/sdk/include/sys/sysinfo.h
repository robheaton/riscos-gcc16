#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H
#ifdef __cplusplus
extern "C" {
#endif
/* One processor: the threads of the C library take turns (see <pthread.h>). */
int get_nprocs (void);
int get_nprocs_conf (void);
#ifdef __cplusplus
}
#endif
#endif
