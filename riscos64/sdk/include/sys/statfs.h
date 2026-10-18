/* sys/statfs.h - statfs (made from statvfs).  The SDK of RISC OS 64. */
#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int __val[2]; } __fsid_t_riscos;
struct statfs
{
  long f_type, f_bsize;
  unsigned long f_blocks, f_bfree, f_bavail, f_files, f_ffree;
  __fsid_t_riscos f_fsid;
  long f_namelen, f_frsize, f_flags, f_spare[4];
};
int statfs (const char *, struct statfs *);
int fstatfs (int, struct statfs *);
#ifdef __cplusplus
}
#endif
#endif
