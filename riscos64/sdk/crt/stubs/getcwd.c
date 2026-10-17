/* a stub of the SDK (see riscos_stubs.c in this folder's parent): UnixLib 64 has the real function */
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
struct statvfs;
char *getcwd (char *buf, size_t size) { (void) buf; (void) size; errno = ENOSYS; return 0; }
