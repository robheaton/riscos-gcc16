#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
  int i;
  printf ("Hello, World from RISC OS 64!\n");
  printf ("sizeof (long) = %zu, sizeof (void *) = %zu\n", sizeof (long), sizeof (void *));
  for (i = 0; i < argc; i++) printf ("argv[%d] = \"%s\"\n", i, argv[i]);
  printf ("%d %ld %lld %u %x %c %s %5.2f %e\n", -42, 1234567890123L, 123456789012345LL, 4000000000u, 255, 'z', "str", 3.14159, 12345.678);
  {
    char *p = malloc (1000);
    strcpy (p, "malloc works");
    printf ("%s at a 16-byte aligned address: %s\n", p, ((unsigned long) p & 15) == 0 ? "yes" : "no");
    free (p);
  }
  return 0;
}
