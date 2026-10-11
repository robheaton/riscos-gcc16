/* sys/stat.h - struct stat and fstat () for the descriptors of open ().  RISC OS has no owners, links or device numbers: they are 0 (st_nlink 1).  A file is S_IFREG, mode 0444 or 0666 by its attributes, a
   standard stream is S_IFCHR.  The times are the modification time from the file's date stamp (0 for a file with no stamp, and for a stream that was not made by open ()). */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000
#define S_IFIFO  0010000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISUID 0004000
#define S_ISGID 0002000
#define S_ISVTX 0001000
#define S_IRWXU 0000700
#define S_IRUSR 0000400
#define S_IWUSR 0000200
#define S_IXUSR 0000100
#define S_IRWXG 0000070
#define S_IRGRP 0000040
#define S_IWGRP 0000020
#define S_IXGRP 0000010
#define S_IRWXO 0000007
#define S_IROTH 0000004
#define S_IWOTH 0000002
#define S_IXOTH 0000001
struct stat
{
  dev_t st_dev; ino_t st_ino; mode_t st_mode; nlink_t st_nlink; uid_t st_uid; gid_t st_gid; dev_t st_rdev;
  off_t st_size;
  time_t st_atime, st_mtime, st_ctime;
  blksize_t st_blksize; blkcnt_t st_blocks;
};
struct stat64
{
  dev_t st_dev; ino_t st_ino; mode_t st_mode; nlink_t st_nlink; uid_t st_uid; gid_t st_gid; dev_t st_rdev;
  off64_t st_size;
  time_t st_atime, st_mtime, st_ctime;
  blksize_t st_blksize; blkcnt_t st_blocks;
};
extern int fstat (int fd, struct stat *st);
extern int fstat64 (int fd, struct stat64 *st);
#ifdef __cplusplus
}
#endif
#endif
