/* a stub of the SDK (see riscos_stubs.c in this folder's parent): UnixLib 64 has the real function */
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
struct statvfs;
int statvfs (const char *path, struct statvfs *buf) { (void) path; (void) buf; errno = ENOSYS; return -1; }
