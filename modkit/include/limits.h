/* limits.h - the limits of this target (32 bit ARM, char is unsigned), from the compiler's own predefined macros: GCC's limits.h wants the system's one after it, which a module does not have. */
#ifndef _LIMITS_H
#define _LIMITS_H
#define CHAR_BIT	__CHAR_BIT__
#define SCHAR_MAX	__SCHAR_MAX__
#define SCHAR_MIN	(-SCHAR_MAX - 1)
#define UCHAR_MAX	(SCHAR_MAX * 2 + 1)
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN	0
#define CHAR_MAX	UCHAR_MAX
#else
#define CHAR_MIN	SCHAR_MIN
#define CHAR_MAX	SCHAR_MAX
#endif
#define SHRT_MAX	__SHRT_MAX__
#define SHRT_MIN	(-SHRT_MAX - 1)
#define USHRT_MAX	(SHRT_MAX * 2 + 1)
#define INT_MAX		__INT_MAX__
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX	(INT_MAX * 2U + 1U)
#define LONG_MAX	__LONG_MAX__
#define LONG_MIN	(-LONG_MAX - 1L)
#define ULONG_MAX	(LONG_MAX * 2UL + 1UL)
#define LLONG_MAX	__LONG_LONG_MAX__
#define LLONG_MIN	(-LLONG_MAX - 1LL)
#define ULLONG_MAX	(LLONG_MAX * 2ULL + 1ULL)
#define MB_LEN_MAX	1
/* the POSIX limits, with the values of GCCSDK's UnixLib */
#define _POSIX_ARG_MAX 1024
#define ARG_MAX _POSIX_ARG_MAX
#define _POSIX_CHILD_MAX 1
#define CHILD_MAX _POSIX_CHILD_MAX
#define _POSIX_LINK_MAX 1
#define LINK_MAX _POSIX_LINK_MAX
#define _POSIX_MAX_CANON 256
#define MAX_CANON _POSIX_MAX_CANON
#define _POSIX_MAX_INPUT 256
#define MAX_INPUT _POSIX_MAX_INPUT
#define _POSIX_NGROUPS_MAX 0
#define NGROUPS_MAX _POSIX_NGROUPS_MAX
#define _POSIX_OPEN_MAX 64
#define OPEN_MAX _POSIX_OPEN_MAX
#define _POSIX_NAME_MAX 252
#define NAME_MAX _POSIX_NAME_MAX
#define _POSIX_PATH_MAX 1024
#define PATH_MAX _POSIX_PATH_MAX
#define _POSIX_PIPE_BUF 512
#define PIPE_BUF _POSIX_PIPE_BUF
#define _POSIX_STREAM_MAX 8
#define STREAM_MAX _POSIX_STREAM_MAX
#define _POSIX_TZNAME_MAX 3
#define TZNAME_MAX _POSIX_TZNAME_MAX
#define _POSIX_SSIZE_MAX 32767
#define SSIZE_MAX _POSIX_SSIZE_MAX
#endif
