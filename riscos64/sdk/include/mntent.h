/* mntent.h - the list of mounted file systems (there is none on RISC OS: setmntent fails).  The SDK of RISC OS 64. */
#ifndef _MNTENT_H
#define _MNTENT_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MNTTYPE_IGNORE "ignore"
#define MNTOPT_RO "ro"
#define MNTOPT_RW "rw"
#define _PATH_MNTTAB "/etc/fstab"
#define _PATH_MOUNTED "/etc/mtab"
struct mntent { char *mnt_fsname; char *mnt_dir; char *mnt_type; char *mnt_opts; int mnt_freq; int mnt_passno; };
FILE *setmntent (const char *, const char *);
struct mntent *getmntent (FILE *);
int addmntent (FILE *, const struct mntent *);
int endmntent (FILE *);
char *hasmntopt (const struct mntent *, const char *);
#ifdef __cplusplus
}
#endif
#endif
