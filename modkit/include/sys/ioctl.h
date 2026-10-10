/* sys/ioctl.h - socketioctl () and the requests that the Internet module understands (BSD values). */
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
#define SIOCGIFADDR _IOWR ('i', 33, struct ifreq)
extern int socketioctl (int s, unsigned long cmd, void *arg);
#ifdef __cplusplus
}
#endif
#endif
