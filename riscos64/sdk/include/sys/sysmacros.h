/* sys/sysmacros.h - device numbers (glibc layout).  The SDK of RISC OS 64. */
#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#define major(dev) ((unsigned) (((dev) >> 8) & 0xfff))
#define minor(dev) ((unsigned) (((dev) & 0xff) | (((dev) >> 12) & 0xfff00)))
#define makedev(ma, mi) ((((unsigned long) (ma) & 0xfff) << 8) | ((unsigned long) (mi) & 0xff) | (((unsigned long) (mi) & 0xfff00) << 12))
#endif
