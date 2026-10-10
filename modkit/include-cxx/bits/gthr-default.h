// bits/gthr-default.h for modules: there are no threads.  libstdc++'s own (the one for POSIX threads) wants <pthread.h>; this one is the single thread version of GCC (gthr-single.h): __gthread_active_p ()
// is 0 and the mutex functions do nothing (a module has one thread of control; an interrupt handler is not a thread that a mutex could stop).  <mutex>, <thread>, <future> and <condition_variable> do not compile (libstdc++ wants more of the thread layer than this).  <memory> (shared_ptr), <string>, <iterator>, <random> ... need this header only for their reference counts and locks.
#ifndef _MODKIT_GTHR_DEFAULT_H
#define _MODKIT_GTHR_DEFAULT_H 1
#include <bits/gthr-single.h>
#endif
