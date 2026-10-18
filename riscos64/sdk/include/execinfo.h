#ifndef _EXECINFO_H
#define _EXECINFO_H
#ifdef __cplusplus
extern "C" {
#endif
int backtrace (void **, int);		/* no frames are reported: returns 0 */
#ifdef __cplusplus
}
#endif
#endif
