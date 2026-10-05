/* Host model of UnixLib's pthread_once: old (one global mutex held across init_routine) vs new (per-control state machine).
   Built against glibc pthreads; checks exactly-once, waiting for completion, independence of different controls,
   and that the std::async pattern (join inside a once routine while the other thread needs ANOTHER once) no longer deadlocks. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>

typedef int once_t;

/* ---- OLD (UnixLib 5.0 as shipped) ---- */
static pthread_mutex_t old_mutex = PTHREAD_MUTEX_INITIALIZER;
static int old_once(once_t *c, void (*fn)(void))
{
    pthread_mutex_lock(&old_mutex);
    if (*c == 0) { fn(); *c = 1; }
    pthread_mutex_unlock(&old_mutex);
    return 0;
}

/* ---- NEW ---- */
static pthread_mutex_t new_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t new_cond = PTHREAD_COND_INITIALIZER;
#define ONCE_DONE 1
#define ONCE_RUNNING 2
static void new_cleanup(void *arg)
{
    pthread_mutex_lock(&new_mutex);
    *(once_t *)arg = 0;
    pthread_cond_broadcast(&new_cond);
    pthread_mutex_unlock(&new_mutex);
}
static int new_once(once_t *c, void (*fn)(void))
{
    if (c == NULL || fn == NULL) return EINVAL;
    pthread_mutex_lock(&new_mutex);
    while (*c == ONCE_RUNNING) pthread_cond_wait(&new_cond, &new_mutex);
    if (*c == 0) {
        *c = ONCE_RUNNING;
        pthread_mutex_unlock(&new_mutex);
        pthread_cleanup_push(new_cleanup, c);
        fn();
        pthread_cleanup_pop(0);
        pthread_mutex_lock(&new_mutex);
        *c = ONCE_DONE;
        pthread_cond_broadcast(&new_cond);
    }
    pthread_mutex_unlock(&new_mutex);
    return 0;
}

typedef int (*once_fn)(once_t *, void (*)(void));
static once_fn ONCE;

/* ---- tests ---- */
static atomic_int runs[16];
static once_t ctl[16];
static void init0(void) { atomic_fetch_add(&runs[0], 1); usleep(2000); }
static void *hammer(void *p) { (void)p; for (int i = 0; i < 200; i++) ONCE(&ctl[0], init0); return NULL; }

/* async pattern: "main" runs once(A) whose routine joins the worker; the worker runs once(B) before finishing */
static once_t A, B;
static atomic_int b_done;
static void initB(void) { atomic_store(&b_done, 1); }
static void *worker(void *p) { (void)p; usleep(20000); ONCE(&B, initB); return NULL; }
static pthread_t worker_thr;
static void initA_join(void) { pthread_join(worker_thr, NULL); }

/* a waiter must not return before the running init completes */
static once_t W; static atomic_int w_started, w_done;
static void initW(void) { atomic_store(&w_started, 1); usleep(50000); atomic_store(&w_done, 1); }
static atomic_int w_violation;
static void *waiter(void *p) { (void)p; while (!atomic_load(&w_started)) usleep(100); ONCE(&W, initW); if (!atomic_load(&w_done)) atomic_store(&w_violation, 1); return NULL; }
static void *w_runner(void *p) { (void)p; ONCE(&W, initW); return NULL; }

static volatile int timed_out;
static void on_alarm(int s) { (void)s; timed_out = 1; _exit(3); }

int main(int argc, char **argv)
{
    signal(SIGALRM, on_alarm);
    int use_new = argc > 1 && !strcmp(argv[1], "new");
    ONCE = use_new ? new_once : old_once;
    int fails = 0;
    /* 1. exactly once under contention */
    pthread_t t[8];
    for (int i = 0; i < 8; i++) pthread_create(&t[i], NULL, hammer, NULL);
    for (int i = 0; i < 8; i++) pthread_join(t[i], NULL);
    if (atomic_load(&runs[0]) != 1) { printf("FAIL exactly-once: ran %d times\n", runs[0]); fails++; }
    /* 2. waiters block until the running init completes */
    pthread_t w1, w2;
    pthread_create(&w1, NULL, w_runner, NULL); pthread_create(&w2, NULL, waiter, NULL);
    pthread_join(w1, NULL); pthread_join(w2, NULL);
    if (atomic_load(&w_violation)) { printf("FAIL waiter returned before init finished\n"); fails++; }
    /* 3. std::async pattern: must not deadlock (watchdog: 3 s) */
    alarm(3);
    pthread_create(&worker_thr, NULL, worker, NULL);
    ONCE(&A, initA_join);
    alarm(0);
    if (!atomic_load(&b_done)) { printf("FAIL async pattern: B not done\n"); fails++; }
    printf("%s: %s\n", use_new ? "new" : "old", fails ? "FAILED" : "all checks passed (async pattern completed)");
    (void)timed_out;
    return fails;
}
