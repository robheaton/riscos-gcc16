/* fnmatch.h - fnmatch (): shell wildcard matching (POSIX, with the GNU flags FNM_LEADING_DIR and FNM_CASEFOLD). */
#ifndef _FNMATCH_H
#define _FNMATCH_H
#ifdef __cplusplus
extern "C" {
#endif
#define FNM_PATHNAME	(1 << 0)                                           /* a slash is matched only by a slash */
#define FNM_NOESCAPE	(1 << 1)                                           /* a backslash is an ordinary character */
#define FNM_PERIOD	(1 << 2)                                           /* a leading period (after a slash with FNM_PATHNAME) is matched only by a period */
#define FNM_LEADING_DIR	(1 << 3)                                           /* the pattern matches when it matches the start of the name up to a slash */
#define FNM_CASEFOLD	(1 << 4)                                           /* the case of letters (A - Z) is ignored */
#define FNM_NOMATCH	1
extern int fnmatch (const char *pattern, const char *name, int flags);   /* 0, or FNM_NOMATCH */
#ifdef __cplusplus
}
#endif
#endif
