#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main (int argc, char **argv)
{
  int i, code = 0, readin = 0;
  printf ("args:");
  for (i = 1; i < argc; i++) printf (" [%s]", argv[i]);
  printf ("\n");
  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-x") && i + 1 < argc) code = atoi (argv[++i]);
      else if (!strcmp (argv[i], "-e") && i + 1 < argc) fprintf (stderr, "err:%s\n", argv[++i]);
      else if (!strcmp (argv[i], "-i")) readin = 1;
    }
  if (readin) { char line[256]; while (fgets (line, sizeof line, stdin)) printf ("stdin:%s", line); }
  if (getenv ("TPROG_VAR")) printf ("TPROG_VAR=%s\n", getenv ("TPROG_VAR"));
  return code;
}
