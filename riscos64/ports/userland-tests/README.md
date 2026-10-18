Scripts for the job `port_final` of `tools/run_on_service.py` (the packages `!GNU` in `gnu/` and `!CoreUtils` in `cu/` of the work folder, `GNU$Dir` and `CoreUtils$Dir` set to them):
`uxt` runs diff, cmp, patch, gzip, tar, find and xargs; `cu_t` runs the coreutils; `final` runs both; the shell tests are `ports/sh/tests/conf`.
Fixed for them: `fork` is `vfork` in find and xargs (patches/findutils-4.10.0-riscos64.patch also makes the child of xargs leave the parent's table alone); pipe () is a scrap file (posix/pipe.c, enough for the
"write errno in the child, read it in the parent" use of vfork); a close of a pipe end in a vfork child is ignored; dup2 onto 0-2 in a vfork child is accepted; /dev/null is a scrap file; sysconf (_SC_ARG_MAX) is 32768;
GCC's limits.h is followed by the C library's (NAME_MAX, PATH_MAX, _POSIX_*).
