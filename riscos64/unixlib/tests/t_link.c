/* t_link.c - link () is a copy: the file is there under the new name with the same contents, the old name stays, EEXIST / ENOENT / EPERM. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d, errno %d)\n", #c, __LINE__, errno); } else printf ("ok   %s\n", #c); } while (0)

int main (void)
{
  FILE *f;
  char buf[32] = "";
  struct stat st;
  remove ("lk_a"); remove ("lk_b"); remove ("lk_dir");
  f = fopen ("lk_a", "w"); fputs ("original text", f); fclose (f);
  CHECK (link ("lk_a", "lk_b") == 0);
  f = fopen ("lk_b", "r"); CHECK (f && fgets (buf, sizeof buf, f) && !strcmp (buf, "original text")); if (f) fclose (f);
  CHECK (stat ("lk_a", &st) == 0 && st.st_size == 13);
  errno = 0; CHECK (link ("lk_a", "lk_b") == -1 && errno == EEXIST);
  errno = 0; CHECK (link ("lk_none", "lk_c") == -1 && errno == ENOENT);
  CHECK (mkdir ("lk_dir", 0777) == 0);
  errno = 0; CHECK (link ("lk_dir", "lk_d2") == -1 && errno == EPERM);
  CHECK (unlink ("lk_a") == 0 && unlink ("lk_b") == 0 && rmdir ("lk_dir") == 0);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
