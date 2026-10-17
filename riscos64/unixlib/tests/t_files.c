/* t_files - UnixLib 64: Unix file names on RISC OS: creating files and directories, listing a directory, the current directory, stat, rename, unlink, rmdir.  Prints what happens; exit code 1 so
   that the runner shows the output.  Built with  aarch64-riscos-gcc -munixlib t_files.c */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unixlib/local.h>

static int fails;
#define CHECK(c, what) do { int ok_ = (c); printf ("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) { fails++; printf ("   errno %d (%s)\n", errno, strerror (errno)); } } while (0)

static void list (const char *dir)
{
  DIR *d = opendir (dir);
  struct dirent *e;
  printf ("ls %s:", dir);
  if (!d) { printf (" opendir failed, errno %d\n", errno); fails++; return; }
  while ((e = readdir (d)) != NULL) printf (" %s%s", e->d_name, e->d_type == DT_DIR ? "/" : "");
  printf ("\n");
  closedir (d);
}

int main (void)
{
  struct stat st;
  char cwd[256];
  char buf[64];
  int fd;
  struct statvfs sv;
  FILE *f;

  CHECK (mkdir ("ultest", 0777) == 0 || errno == EEXIST, "mkdir ultest");
  CHECK (stat ("ultest", &st) == 0 && S_ISDIR (st.st_mode), "stat ultest is a directory");
  fd = open ("ultest/hello.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  CHECK (fd >= 0, "open ultest/hello.txt for writing (the name has a dot: hello.txt is hello/txt)");
  if (fd >= 0) { write (fd, "hello\n", 6); close (fd); }
  fd = open ("ultest/main.c", O_WRONLY | O_CREAT | O_TRUNC, 0666);
  CHECK (fd >= 0, "open ultest/main.c (suffix swapping: ultest.c.main)");
  if (fd >= 0) { write (fd, "int x;\n", 7); close (fd); }
  CHECK (stat ("ultest/main.c", &st) == 0 && S_ISREG (st.st_mode) && st.st_size == 7, "stat ultest/main.c: a file of 7 bytes");
  CHECK (stat ("ultest/hello.txt", &st) == 0 && st.st_size == 6, "stat ultest/hello.txt: 6 bytes");
  fd = open ("ultest/hello.txt", O_RDONLY);
  CHECK (fd >= 0 && read (fd, buf, sizeof buf) == 6 && memcmp (buf, "hello\n", 6) == 0, "read it back");
  if (fd >= 0) close (fd);
  list ("ultest");
  CHECK (rename ("ultest/hello.txt", "ultest/world.txt") == 0, "rename hello.txt to world.txt");
  CHECK (access ("ultest/hello.txt", F_OK) != 0 && access ("ultest/world.txt", F_OK) == 0, "the old name is gone, the new one is there");
  f = fopen ("ultest/data.bin", "w+");
  CHECK (f != NULL, "fopen w+");
  if (f) { fputs ("abc", f); rewind (f); CHECK (fgetc (f) == 'a', "fgetc after rewind"); fclose (f); }
  CHECK (getcwd (cwd, sizeof cwd) != NULL, "getcwd");
  printf ("cwd = %s\n", cwd);
  CHECK (chdir ("ultest") == 0, "chdir ultest");
  CHECK (getcwd (cwd, sizeof cwd) != NULL, "getcwd inside");
  printf ("cwd = %s\n", cwd);
  CHECK (stat ("world.txt", &st) == 0 && st.st_size == 6, "stat world.txt relative to the new current directory");
  CHECK (chdir ("..") == 0, "chdir ..");
  CHECK (statvfs (".", &sv) == 0, "statvfs .");
  CHECK (unlink ("ultest/world.txt") == 0 && unlink ("ultest/data.bin") == 0 && unlink ("ultest/main.c") == 0, "unlink the files");
  /* the suffix directory ultest.c (it holds main, the RISC OS name of ultest/main.c): "ultest/c" would be taken for a RISC OS name by the guess of UnixLib, so the name is given without translation */
  { int ctl = __get_riscosify_control (); int r;
    __set_riscosify_control (__RISCOSIFY_NO_PROCESS);
    r = rmdir ("ultest.c");
    __set_riscosify_control (ctl);
    CHECK (r == 0, "rmdir ultest.c (the suffix directory), name given as RISC OS"); }
  CHECK (rmdir ("ultest") == 0, "rmdir ultest");
  CHECK (stat ("ultest", &st) != 0 && errno == ENOENT, "ultest is gone");
  CHECK (opendir ("nosuchdir") == NULL && errno == ENOENT, "opendir of a missing directory: ENOENT");
  printf ("SUMMARY %d failed\n", fails);
  return 1;
}
