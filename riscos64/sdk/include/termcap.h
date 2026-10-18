/* termcap.h - the termcap interface over a built-in database ("riscos": the VDU drivers; see crt/posix/termcap.c).  The SDK of RISC OS 64. */
#ifndef _RISCOS64_TERMCAP_H
#define _RISCOS64_TERMCAP_H
#ifdef __cplusplus
extern "C" {
#endif
extern char PC;
extern char *UP, *BC;
extern short ospeed;
int tgetent (char *bp, const char *name);
int tgetflag (const char *id);
int tgetnum (const char *id);
char *tgetstr (const char *id, char **area);
char *tgoto (const char *cap, int col, int row);
int tputs (const char *str, int affcnt, int (*putc_) (int));
#ifdef __cplusplus
}
#endif
#endif
