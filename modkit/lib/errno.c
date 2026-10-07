/* errno.c - the variable errno (the library's functions set it; one per module: there are no threads). */
int errno;
/* the BSD name for the same variable: the veneers of the TCP/IP libraries (socklib: socket, bind ...) of the RISC OS sources store the error number in __errno, and <sys/errno.h> there says  #define errno __errno */
#define STR(x) #x
#define XSTR(x) STR (x)                                                  /* (the host build of the library tests renames errno: the alias must follow) */
extern int __errno __attribute__ ((alias (XSTR (errno))));
