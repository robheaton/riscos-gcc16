/* sys/ioctl.h - socketioctl () and the requests that the Internet module understands (the values of the RISC OS TCPIPLibs, which are 4.4BSD's except for the interface "get" requests). */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H
#ifdef __cplusplus
extern "C" {
#endif
#define IOCPARM_MASK 0x1fff
#define IOC_VOID 0x20000000UL
#define IOC_OUT 0x40000000UL
#define IOC_IN 0x80000000UL
#define IOC_INOUT (IOC_IN | IOC_OUT)
#define _IOC(inout, group, num, len) ((unsigned long) ((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))
#define _IO(g, n) _IOC (IOC_VOID, (g), (n), 0)
#define _IOR(g, n, t) _IOC (IOC_OUT, (g), (n), sizeof (t))
#define _IOW(g, n, t) _IOC (IOC_IN, (g), (n), sizeof (t))
#define _IOWR(g, n, t) _IOC (IOC_INOUT, (g), (n), sizeof (t))
#define FIONREAD _IOR ('f', 127, int)        /* the bytes that can be read */
#define FIONBIO _IOW ('f', 126, int)         /* non-blocking I/O on or off */
#define FIOASYNC _IOW ('f', 125, int)        /* asynchronous I/O: the Internet event Event_Internet is generated when there is input */
#define SIOCATMARK _IOR ('s', 7, int)
#define FIOCLEX _IO ('f', 1)
#define FIONCLEX _IO ('f', 2)
#define FIOSETOWN _IOW ('f', 124, int)
#define FIOGETOWN _IOR ('f', 123, int)
#define FIONWRITE _IOR ('f', 119, int)       /* the bytes still to be written */
#define SIOCSHIWAT _IOW ('s', 0, int)
#define SIOCGHIWAT _IOR ('s', 1, int)
#define SIOCSLOWAT _IOW ('s', 2, int)
#define SIOCGLOWAT _IOR ('s', 3, int)
/* the interface requests (struct ifreq and struct ifconf are in <net/if.h>).  The numbers of the "get" requests are NOT 4.4BSD's: the RISC OS Internet module has its own (96 - 99, 95), as in the RISC OS TCPIPLibs */
#define SIOCSIFADDR _IOW ('i', 12, struct ifreq)
#define SIOCGIFADDR _IOWR ('i', 99, struct ifreq)
#define SIOCSIFDSTADDR _IOW ('i', 14, struct ifreq)
#define SIOCGIFDSTADDR _IOWR ('i', 98, struct ifreq)
#define SIOCSIFFLAGS _IOW ('i', 16, struct ifreq)
#define SIOCGIFFLAGS _IOWR ('i', 17, struct ifreq)
#define SIOCGIFBRDADDR _IOWR ('i', 97, struct ifreq)
#define SIOCSIFBRDADDR _IOW ('i', 19, struct ifreq)
#define SIOCGIFCONF _IOWR ('i', 96, struct ifconf)
#define SIOCGIFNETMASK _IOWR ('i', 95, struct ifreq)
#define SIOCSIFNETMASK _IOW ('i', 22, struct ifreq)
#define SIOCGIFMETRIC _IOWR ('i', 23, struct ifreq)
#define SIOCSIFMETRIC _IOW ('i', 24, struct ifreq)
#define SIOCGIFMTU _IOWR ('i', 39, struct ifreq)
#define SIOCSIFMTU _IOW ('i', 40, struct ifreq)
#define SIOCSPGRP _IOW ('s', 8, int)
#define SIOCGPGRP _IOR ('s', 9, int)
extern int socketioctl (int s, unsigned long cmd, void *arg);
extern int ioctl (int fd, unsigned long request, ...);               /* on a socket (since 16.2.0-23): the requests of the Internet module (FIONBIO, FIONREAD, FIOASYNC, the SIOC ones) with a pointer argument; on a file: FIONREAD (the bytes left); anything else is ENOTTY */
#ifdef __cplusplus
}
#endif
#endif
