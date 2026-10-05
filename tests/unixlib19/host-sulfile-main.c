/* host-sulfile-main.c -- build sulfile.c's main () on the build host with a mock _swix (OS_File 5 answered with stat ()) and run it on real files:  the whole program, not only identify ().
   build:  gcc -std=gnu11 -O1 -Wall -Wextra -Ihost-stubs host-sulfile-main.c -o /tmp/host-sulfile-main      (add -DSULFILE_TEST_NOFOPEN to test the OS_File 255 fallback)      run:  /tmp/host-sulfile-main FILE   (prints the two lines, exits with the status) */
#include <stdarg.h>
#include <sys/stat.h>
#include "sulfile.c"

_kernel_oserror *_swix (int swi, unsigned flags, ...)
{
  (void) flags;
  va_list ap; va_start (ap, flags);
  int reason = va_arg (ap, int); const char *path = va_arg (ap, const char *);
  static _kernel_oserror err = { 0, "Bad parameters (mock)" };
  if (swi != OS_File) { va_end (ap); return &err; }
  if (reason == 255) {                                                            /* load named file: R2 = the buffer, R3 = 0 */
    unsigned char *buf = va_arg (ap, unsigned char *); va_end (ap);
    FILE *f = fopen (path, "rb"); if (!f) return &err;
    size_t n = fread (buf, 1, 1000000, f); (void) n; fclose (f); return NULL;
  }
  if (reason != 5) { va_end (ap); return &err; }
  unsigned *type = va_arg (ap, unsigned *), *load = va_arg (ap, unsigned *), *exec = va_arg (ap, unsigned *), *len = va_arg (ap, unsigned *), *attr = va_arg (ap, unsigned *);
  va_end (ap);
  struct stat st;
  if (stat (path, &st) != 0) { *type = 0; return NULL; }
  *type = S_ISDIR (st.st_mode) ? 2 : 1; *load = 0xFFFFFA00u; *exec = 0; *len = (unsigned) st.st_size; *attr = 0x13;     /* type FFA, WR/R */
  return NULL;
}
