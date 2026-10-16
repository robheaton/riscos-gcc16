/* t64.h - the small runtime of the RISC OS 64 tests.  The same tests are built for the host (host/rt_host.c) to produce the expected output. */
#ifndef T64_H
#define T64_H
typedef unsigned long u64;
typedef long i64;
typedef unsigned int u32;
typedef unsigned char u8;

void t_putc (int c);                 /* '\n' is OS_NewLine on RISC OS */
void t_puts (const char *s);
void t_hex (u64 v, int digits);      /* digits of hex, no prefix */
void t_dec (i64 v);
void t_check (int ok, const char *what);     /* prints "PASS what" or "FAIL what" */
int t_summary (const char *name);    /* prints "SUMMARY name: N checks, F failed"; returns 0 if none failed, else 1 */
void t_info (const char *key, u64 value);    /* prints "INFO key = &hex" (machine dependent: not compared) */

void *memcpy (void *, const void *, unsigned long);
void *memmove (void *, const void *, unsigned long);
void *memset (void *, int, unsigned long);
int memcmp (const void *, const void *, unsigned long);
unsigned long strlen (const char *);

#define CHECK(c) t_check ((c) ? 1 : 0, #c)
#endif
