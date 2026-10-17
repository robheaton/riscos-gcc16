/* t_riscosify - UnixLib 64: the translation of file names, in both directions, with fixed flags (so that the file system does not matter): prints one line for each name.  The lines are compared with
   the output of the same table with the sources of UnixLib on the host (expected-riscosify.txt).  Built with  aarch64-riscos-gcc t_riscosify.c -Wl,-u,__ul64_init -lunixlib64 */
#include <stdio.h>
#include <string.h>
#include <unixlib/local.h>

static const char *const names[] = {
  "foo", "foo/bar", "foo/bar/baz", "foo.c", "src/main.c", "src/main.h", "a/b/c.o", "lib/libfoo.a", "readme.txt", "x.y.z", "/tmp/scratch", "/dev/null", "file,ff8", "file.txt,fff", ".", "..", "../x", "./x",
  "dir/../x", "a b", "A-B_c", "~/file.c", "adfs::4.$.foo", "<Obey$Dir>.x", "ADFS::HardDisc4.$.dir.file", "$.dir.file", "gcc:include/stdio.h", "@.x", "%.x", "a//b", "a/", "/", "c.foo", "h.bar.baz", "ultest/c", "ultest/c/main", "c", "src/c", "src/h/x", "ultest", NULL
};

int main (void)
{
  int i, flags[3] = { __RISCOSIFY_DONT_CHECK_DIR | __RISCOSIFY_NO_SUFFIX * 0, __RISCOSIFY_DONT_CHECK_DIR | __RISCOSIFY_FILETYPE_EXT, __RISCOSIFY_DONT_CHECK_DIR | __RISCOSIFY_NO_SUFFIX | __RISCOSIFY_STRICT_UNIX_SPECS };
  __sfixinit ("c:cc:f:h:i:ii:l:o:p:s:y");
  for (i = 0; names[i]; i++)
    {
      char out[256], back[256];
      int k;
      printf ("%-28s", names[i]);
      for (k = 0; k < 3; k++)
	{
	  int ft = -1;
	  char *e = __riscosify (names[i], 0, flags[k], out, sizeof out, &ft);
	  if (!e) printf ("  [%d] NULL", k);
	  else printf ("  [%d] %s", k, out);
	  if (e && ft != -1) printf (" (type %x)", ft);
	}
      {
	char *e = __riscosify (names[i], 0, flags[0], out, sizeof out, NULL);
	if (e && __unixify (out, 0, back, sizeof back, __RISCOSIFY_FILETYPE_NOTSPECIFIED)) printf ("  back: %s", back);
      }
      printf ("\n");
    }
  return 1;
}
