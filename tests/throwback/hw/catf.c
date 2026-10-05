/* catf.c -- print files that may not exist, without an error: the Obey file of a test pack stops at the first command that fails (*Type of a missing file), this never does.
   catf FILE...       prints each file (or "(FILE: not there)"), then a line with its size;  catf -s FILE...  prints only the size lines.  Always ends with status 0.  */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main (int argc, char **argv)
{
  int sizes_only = 0, first = 1;
  if (argc > 1 && !strcmp (argv[1], "-s")) { sizes_only = 1; first = 2; }
  for (int i = first; i < argc; i++)
    {
      struct stat st;
      if (stat (argv[i], &st) != 0)
	{
	  printf ("(%s: not there)\n", argv[i]);
	  continue;
	}
      if (!sizes_only)
	{
	  FILE *f = fopen (argv[i], "r");
	  if (f)
	    {
	      char buf[1024];
	      size_t n;
	      printf ("----- %s:\n", argv[i]);
	      while ((n = fread (buf, 1, sizeof buf, f)) > 0)
		fwrite (buf, 1, n, stdout);
	      fclose (f);
	      printf ("----- end of %s\n", argv[i]);
	    }
	  else printf ("(%s: cannot be opened)\n", argv[i]);
	}
      printf ("%s: %ld bytes\n", argv[i], (long) st.st_size);
    }
  return 0;
}
