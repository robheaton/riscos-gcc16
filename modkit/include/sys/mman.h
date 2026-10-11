/* sys/mman.h - the constants of mmap () as GCCSDK's UnixLib has them (a module that implements memory mapping for UnixLib programs, ARMEABISupport, uses them); the functions are not here: a module has no mmap. */
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#include <sys/types.h>
#define PROT_NONE 0x00
#define PROT_READ 0x04
#define PROT_WRITE 0x02
#define PROT_EXEC 0x01
#define MAP_FAILED ((void *) -1)
#define MAP_FILE 0x0001
#define MAP_ANON 0x0002
#define MAP_ANONYMOUS MAP_ANON
#define MAP_TYPE 0x000f
#define MAP_COPY 0x0020
#define MAP_SHARED 0x0010
#define MAP_PRIVATE 0x0000
#define MAP_FIXED 0x0100
#define MAP_NOEXTEND 0x0200
#define MAP_HASSEMPHORE 0x0400
#define MAP_INHERIT 0x0800
#define MS_ASYNC 1
#define MS_INVALIDATE 2
#define MS_SYNC 4
#define MADV_NORMAL 0
#define MADV_RANDOM 1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED 3
#define MADV_DONTNEED 4
#define MADV_FREE 5
#define MREMAP_MAYMOVE 1
#endif
