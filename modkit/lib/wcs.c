/* wcs.c - the wide string and memory functions of <wchar.h> (C95): the same as string.h's, on wchar_t.  In the C locale wcscoll is wcscmp and wcsxfrm a copy. */
#include <wchar.h>
#include <stdlib.h>

size_t wcslen (const wchar_t *s) { size_t n = 0; while (s[n]) n++; return n; }
wchar_t *wcscpy (wchar_t *d, const wchar_t *s) { wchar_t *r = d; while ((*d++ = *s++)) ; return r; }
wchar_t *wcsncpy (wchar_t *d, const wchar_t *s, size_t n) { wchar_t *r = d; for (; n && *s; n--) *d++ = *s++; for (; n; n--) *d++ = 0; return r; }
wchar_t *wcscat (wchar_t *d, const wchar_t *s) { wcscpy (d + wcslen (d), s); return d; }
wchar_t *wcsncat (wchar_t *d, const wchar_t *s, size_t n) { wchar_t *r = d; d += wcslen (d); for (; n && *s; n--) *d++ = *s++; *d = 0; return r; }
int wcscmp (const wchar_t *a, const wchar_t *b) { while (*a && *a == *b) { a++; b++; } return (*a > *b) - (*a < *b); }
int wcsncmp (const wchar_t *a, const wchar_t *b, size_t n) { for (; n; n--, a++, b++) { if (*a != *b) return (*a > *b) - (*a < *b); if (!*a) return 0; } return 0; }
int wcscoll (const wchar_t *a, const wchar_t *b) { return wcscmp (a, b); }
size_t wcsxfrm (wchar_t *d, const wchar_t *s, size_t n) { size_t l = wcslen (s); if (n) { size_t k = l < n - 1 ? l : n - 1; for (size_t i = 0; i < k; i++) d[i] = s[i]; d[k] = 0; } return l; }
wchar_t *wcschr (const wchar_t *s, wchar_t c) { for (;; s++) { if (*s == c) return (wchar_t *) s; if (!*s) return NULL; } }
wchar_t *wcsrchr (const wchar_t *s, wchar_t c) { const wchar_t *r = NULL; for (;; s++) { if (*s == c) r = s; if (!*s) return (wchar_t *) r; } }
size_t wcsspn (const wchar_t *s, const wchar_t *set) { size_t n = 0; while (s[n] && wcschr (set, s[n])) n++; return n; }
size_t wcscspn (const wchar_t *s, const wchar_t *set) { size_t n = 0; while (s[n] && !wcschr (set, s[n])) n++; return n; }
wchar_t *wcspbrk (const wchar_t *s, const wchar_t *set) { for (; *s; s++) if (wcschr (set, *s)) return (wchar_t *) s; return NULL; }
wchar_t *wcsstr (const wchar_t *h, const wchar_t *n)
{
  size_t l = wcslen (n);
  if (!l) return (wchar_t *) h;
  for (; *h; h++) if (*h == *n && wcsncmp (h, n, l) == 0) return (wchar_t *) h;
  return NULL;
}
wchar_t *wcstok (wchar_t *s, const wchar_t *delim, wchar_t **save)
{
  wchar_t *t;
  if (!s) s = *save;
  if (!s) return NULL;
  s += wcsspn (s, delim);
  if (!*s) { *save = NULL; return NULL; }
  t = s + wcscspn (s, delim);
  if (*t) { *t = 0; *save = t + 1; } else *save = NULL;
  return s;
}
wchar_t *wmemchr (const wchar_t *s, wchar_t c, size_t n) { for (; n; n--, s++) if (*s == c) return (wchar_t *) s; return NULL; }
int wmemcmp (const wchar_t *a, const wchar_t *b, size_t n) { for (; n; n--, a++, b++) if (*a != *b) return (*a > *b) - (*a < *b); return 0; }
wchar_t *wmemcpy (wchar_t *d, const wchar_t *s, size_t n) { for (size_t i = 0; i < n; i++) d[i] = s[i]; return d; }
wchar_t *wmemmove (wchar_t *d, const wchar_t *s, size_t n)
{
  if (d < s) for (size_t i = 0; i < n; i++) d[i] = s[i];
  else for (size_t i = n; i--; ) d[i] = s[i];
  return d;
}
wchar_t *wmemset (wchar_t *d, wchar_t c, size_t n) { for (size_t i = 0; i < n; i++) d[i] = c; return d; }
