/* sys/un.h - sockaddr_un (no Unix domain sockets on RISC OS 64: socket () fails with ENOSYS).  The SDK of RISC OS 64. */
#ifndef _RISCOS64_SYS_UN_H
#define _RISCOS64_SYS_UN_H
#include <sys/socket.h>
struct sockaddr_un { sa_family_t sun_family; char sun_path[108]; };
#endif
