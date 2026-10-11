/* a program in strict C99 that defines functions the library has as extensions: no clash with the headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
static int getline (char *s, int n) { (void) s; return n; }
static char *stpcpy (char *d, const char *s) { return strcpy (d, s); }
static int setenv (const char *a, const char *b, int c) { (void) a; (void) b; return c; }
static int fileno (FILE *f) { (void) f; return 1; }
static int ffs (int x) { return x; }
static int timezone2;
static int err (int x) { return x; }
static int fnmatch (const char *a, const char *b) { return a == b; }
static int dysize (int y) { return y; }
static int gettimeofday (void) { return 0; }
int main (void) { char b[8]; (void) timezone2; return getline (b, 1) + stpcpy (b, "") - b + setenv ("a", "b", 0) + fileno (stdout) + ffs (1) + err (0) + fnmatch ("a", "b") + dysize (1) + gettimeofday (); }
