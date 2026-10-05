/* model_main.c -- UnixLib's vfscanf (the text of the patched libunixlib/stdio/scanf.c, extracted by build-model.sh) on a mini FILE, with the types of the 32-bit ARM target where that matters:
   `long' is int32_t, strtol / strtoul are the 32 bit ones (ERANGE saturates at 32 bits), size_t and ptrdiff_t are 4 bytes (build-model.sh turns the sizeof's into 4). */
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct mfile { const unsigned char *i_ptr; long i_cnt; } mfile;
#define FILE mfile
#undef EOF
#define EOF (-1)
static int __peek_char (mfile *f) { (void) f; return EOF; }
static int mf_getc (mfile *f) { if (f->i_cnt <= 0) return EOF; f->i_cnt--; return *f->i_ptr++; }
static int mf_ungetc (int c, mfile *f) { if (c == EOF) return EOF; f->i_ptr--; f->i_cnt++; return c; }
static size_t mf_fread (void *p, size_t sz, size_t n, mfile *f)
{
  size_t want = sz * n, got = want < (size_t) f->i_cnt ? want : (size_t) f->i_cnt;
  memcpy (p, f->i_ptr, got); f->i_ptr += got; f->i_cnt -= got;
  return sz ? got / sz : 0;
}
#define getc(f) mf_getc (f)
#define ungetc(c, f) mf_ungetc (c, f)
#define fread(p, s, n, f) mf_fread (p, s, n, f)
#define PTHREAD_UNSAFE
#define vfscanf ul_vfscanf

/* the 32-bit strtol / strtoul of the target */
static int32_t model_strtol32 (const char *s, char **e, int b)
{
  long long v = strtoll (s, e, b);
  if (v > INT32_MAX) v = INT32_MAX;
  if (v < INT32_MIN) v = INT32_MIN;
  return (int32_t) v;
}
static uint32_t model_strtoul32 (const char *s, char **e, int b)
{
  const char *p = s;
  while (isspace ((unsigned char) *p)) p++;
  int neg = *p == '-';
  unsigned long long v = strtoull (s, e, b);
  if (neg) { unsigned long long m = 0ULL - v; if (m > 0xffffffffULL) return 0xffffffffu; return 0u - (uint32_t) m; }
  return v > 0xffffffffULL ? 0xffffffffu : (uint32_t) v;
}
static long long model_strtoll (const char *s, char **e, int b) { return strtoll (s, e, b); }
static unsigned long long model_strtoull (const char *s, char **e, int b) { return strtoull (s, e, b); }
static long double model_strtold (const char *s, char **e) { return strtold (s, e); }

#ifndef BODY_FILE
#define BODY_FILE "body.c"
#endif
#include BODY_FILE

int ul_vsscanf (const char *buf, const char *fmt, va_list ap)
{
  mfile f; f.i_ptr = (const unsigned char *) buf; f.i_cnt = (long) strlen (buf);
  return ul_vfscanf (&f, fmt, ap);
}

int ul_sscanf (const char *buf, const char *fmt, ...)
{
  va_list ap; int r;
  va_start (ap, fmt); r = ul_vsscanf (buf, fmt, ap); va_end (ap);
  return r < 0 ? -1 : r;                 /* like UnixLib's sscanf */
}
