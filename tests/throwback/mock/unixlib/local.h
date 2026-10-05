/* Host mock of <unixlib/local.h>: __riscosify_std is scripted by the test.  */
#ifndef MOCK_UNIXLIB_LOCAL_H
#define MOCK_UNIXLIB_LOCAL_H
#include <stddef.h>
extern char *__riscosify_std (const char *name, int create_dir, char *buffer, size_t buf_len, int *filetype);
#endif
