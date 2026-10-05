/* the little of UnixLib's internal/unix.h that stdio/fread.c and stdio/fwrite.c use */
#ifndef MODEL_INTERNAL_UNIX_H
#define MODEL_INTERNAL_UNIX_H
#include <errno.h>
struct ul_global { int fls_lbstm_on_rd; };
extern struct ul_global __ul_global;
#define __set_errno(e) (errno = (e), -1)
#endif
