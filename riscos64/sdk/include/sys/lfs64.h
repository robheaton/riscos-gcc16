/* sys/lfs64.h - the large file names (open64, stat64, fopen64 ...): off_t is 64 bits here, so they are the plain functions (SDK of RISC OS 64) */
#ifndef _SYS_LFS64_H
#define _SYS_LFS64_H
#if !defined _SDK_NO_LFS64_MACROS && (defined _LARGEFILE64_SOURCE || defined _GNU_SOURCE || defined _DEFAULT_SOURCE)
#define off64_t off_t
#define ino64_t ino_t
#define blkcnt64_t blkcnt_t
#define fpos64_t fpos_t
#define open64 open
#define openat64 openat
#define creat64 creat
#define lseek64 lseek
#define ftruncate64 ftruncate
#define truncate64 truncate
#define pread64 pread
#define pwrite64 pwrite
#define fopen64 fopen
#define freopen64 freopen
#define fseeko64 fseeko
#define ftello64 ftello
#define fgetpos64 fgetpos
#define fsetpos64 fsetpos
#define tmpfile64 tmpfile
#define mkstemp64 mkstemp
#define lockf64 lockf
#define stat64 stat
#define fstat64 fstat
#define lstat64 lstat
#define dirent64 dirent
#define readdir64 readdir
#define readdir64_r readdir_r
#define mmap64 mmap
#define statvfs64 statvfs
#define fstatvfs64 fstatvfs
#endif
#endif
