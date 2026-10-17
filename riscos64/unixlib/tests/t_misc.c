#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/select.h>

static int fails, once_count;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)
static void once_fn (void) { once_count++; }
static void *once_thread (void *a) { (void) a; return 0; }

int main (void)
{
  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  pthread_once_t o = PTHREAD_ONCE_INIT;
  pthread_key_t k;
  pthread_t th;
  posix_spawn_file_actions_t fa;
  posix_spawnattr_t at;
  char buf[16];
  unsigned char rnd[8];
  struct timespec ts = { 0, 1000000 };

  CHECK (pthread_mutex_lock (&m) == 0 && pthread_mutex_trylock (&m) == EBUSY && pthread_mutex_unlock (&m) == 0);
  CHECK (pthread_once (&o, once_fn) == 0 && pthread_once (&o, once_fn) == 0 && once_count == 1);
  CHECK (pthread_key_create (&k, 0) == 0 && pthread_setspecific (k, "x") == 0 && strcmp ((const char *) pthread_getspecific (k), "x") == 0);
  CHECK (pthread_create (&th, 0, once_thread, 0) == 0 && pthread_join (th, 0) == 0);				/* the cooperative threads of pthread.c: t_thread tests them */
  CHECK (dlopen ("libx.so", RTLD_NOW) == 0 && dlerror () != 0);
  CHECK (confstr (_CS_PATH, buf, sizeof buf) == 1);
  CHECK (getentropy (rnd, sizeof rnd) == 0);
  CHECK (pselect (0, 0, 0, 0, &ts, 0) == 0);
  CHECK (posix_spawn_file_actions_init (&fa) == 0 && posix_spawn_file_actions_addclose (&fa, 3) == 0 && posix_spawn_file_actions_destroy (&fa) == 0);
  CHECK (posix_spawnattr_init (&at) == 0 && posix_spawnattr_setflags (&at, 0) == 0 && posix_spawnattr_destroy (&at) == 0);
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
