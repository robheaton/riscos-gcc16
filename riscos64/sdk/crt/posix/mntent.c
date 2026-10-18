/* mntent.c - see include/mntent.h: no file system list; setmntent fails with ENOENT.  The SDK of RISC OS 64. */
#include <errno.h>
#include <mntent.h>
#include <string.h>

FILE *setmntent (const char *file, const char *mode) { (void) file; (void) mode; errno = ENOENT; return 0; }
struct mntent *getmntent (FILE *f) { (void) f; return 0; }
int addmntent (FILE *f, const struct mntent *m) { (void) f; (void) m; return 1; }
int endmntent (FILE *f) { (void) f; return 1; }
char *hasmntopt (const struct mntent *m, const char *opt) { return m && m->mnt_opts ? strstr (m->mnt_opts, opt) : 0; }
