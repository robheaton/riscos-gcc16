/* gets.c - gets (): C90's, removed from C11 (the header declares it for older standards only): reads a line from stdin into S without a length limit, drops the newline, NULL at the end of the input. */
#include <stdio.h>

char *gets (char *s)
{
  char *p = s;
  int c;
  while ((c = getchar ()) != '\n' && c != EOF) *p++ = (char) c;
  if (c == EOF && p == s) return NULL;
  *p = 0;
  return s;
}
