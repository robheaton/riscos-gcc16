/* a stub of the SDK (see riscos_stubs.c in this folder's parent): UnixLib 64 has the real function */
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
struct statvfs;
void *readdir (void *dir) { (void) dir; errno = EBADF; return 0; }
