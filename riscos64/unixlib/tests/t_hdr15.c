/* t_hdr15.c - the declarations that tool chain 0.15 added: <search.h> (lfind, lsearch, insque, remque), <sys/wait.h> (wait4) and <netinet/tcp.h> (TCP_NODELAY, TCP_MAXSEG), and that the functions work.
   "t_hdr15 child" is the child that the program starts (it exits with 7).  The SDK of RISC OS 64. */
#include <netinet/tcp.h>
#include <search.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d)\n", #c, __LINE__); } else printf ("ok   %s\n", #c); } while (0)
static int cmp (const void *a, const void *b) { return *(const int *) a - *(const int *) b; }
struct node { struct node *next, *prev; int v; };

int main (int argc, char **argv)
{
  int a[5] = { 5, 3, 9, 1, 7 }, key = 9, none = 4, st = 0;
  int tab[8], *p;
  size_t n = 5, m = 0;
  struct node x = { 0, 0, 1 }, y = { 0, 0, 2 }, z = { 0, 0, 3 };
  pid_t pid;
  struct rusage ru;
  char *av[3];
  if (argc > 1 && !strcmp (argv[1], "child")) return 7;
  CHECK (TCP_NODELAY == 1 && TCP_MAXSEG == 2);
  p = lfind (&key, a, &n, sizeof a[0], cmp);
  CHECK (p == &a[2]);
  CHECK (lfind (&none, a, &n, sizeof a[0], cmp) == NULL);
  CHECK (lsearch (&key, tab, &m, sizeof tab[0], cmp) == &tab[0] && m == 1);
  CHECK (lsearch (&none, tab, &m, sizeof tab[0], cmp) == &tab[1] && m == 2);
  CHECK (lsearch (&key, tab, &m, sizeof tab[0], cmp) == &tab[0] && m == 2);
  insque (&x, NULL);
  insque (&y, &x);
  insque (&z, &y);
  CHECK (x.next == &y && y.prev == &x && y.next == &z && z.prev == &y);
  remque (&y);
  CHECK (x.next == &z && z.prev == &x);
  av[0] = argv[0]; av[1] = (char *) "child"; av[2] = NULL;
  CHECK (posix_spawn (&pid, argv[0], NULL, NULL, av, environ) == 0);
  CHECK (wait4 (pid, &st, 0, &ru) == pid && WIFEXITED (st) && WEXITSTATUS (st) == 7);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
