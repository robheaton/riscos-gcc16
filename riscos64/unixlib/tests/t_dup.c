/* t_dup.c - things the native GCC and binutils needed: dup/dup2 (descriptors that share one file), rename over an existing file, open (O_WRONLY|O_TRUNC) of an existing file without O_CREAT, realpath into a buffer of
   pathconf size, pathconf, fchmod, and variables made with setenv seen by a child program (system). */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s (errno %d)\n", #c, errno); fails++; } } while (0)
static void put (const char *n, int len, char c) { FILE *f = fopen (n, "wb"); int i; for (i = 0; i < len; i++) fputc (c, f); fclose (f); }
static long size (const char *n) { struct stat st; return stat (n, &st) ? -1 : (long) st.st_size; }

int main (void)
{
  char buf[16];
  int fd, d, d2;
  long pm = pathconf ("/", _PC_PATH_MAX);
  char *rbuf, *rp;

  put ("dupf", 10, 'x');
  fd = open ("dupf", O_RDONLY);
  d = dup (fd);
  CHECK (fd >= 3 && d >= 3 && d != fd);
  CHECK (read (fd, buf, 4) == 4);
  CHECK (read (d, buf, 4) == 4 && lseek (d, 0, SEEK_CUR) == 8);	/* one shared position */
  CHECK (close (fd) == 0);
  CHECK (read (d, buf, 4) == 2);					/* the copy still works after the first is closed */
  d2 = dup2 (d, 1001);
  CHECK (d2 == 1001 && lseek (d2, 0, SEEK_SET) == 0 && read (d2, buf, 3) == 3);
  CHECK (close (d) == 0 && close (d2) == 0);
  CHECK (read (d2, buf, 1) == -1 && errno == EBADF);
  remove ("dupf");

  put ("big", 5000, 'B'); put ("small", 100, 's');
  CHECK (rename ("small", "big") == 0 && size ("big") == 100 && size ("small") == -1);	/* replaces */

  put ("old", 5000, 'O');
  fd = open ("old", O_WRONLY | O_TRUNC);				/* no O_CREAT: the file is emptied all the same */
  CHECK (fd >= 3 && write (fd, "new", 3) == 3 && close (fd) == 0);
  CHECK (size ("old") == 3);

  CHECK (pm >= 255);
  rbuf = (char *) malloc ((size_t) pm);
  rp = realpath ("old", rbuf);
  CHECK (rp == rbuf && strlen (rp) < (size_t) pm);
  { int i; unsigned char *g = (unsigned char *) malloc ((size_t) pm + 64); memset (g, 0xA5, (size_t) pm + 64); rp = realpath ("old", (char *) g); CHECK (g[pm] == 0xA5 && g[pm + 63] == 0xA5); free (g); }
  free (rbuf);
  fd = open ("old", O_RDONLY);
  CHECK (fchmod (fd, 0644) == 0);
  close (fd);

  setenv ("T_DUP_VAR", "hello-from-parent", 1);
  CHECK (system ("Echo <T_DUP_VAR>") == 0);
  remove ("big"); remove ("old");
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
