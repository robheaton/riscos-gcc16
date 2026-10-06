/* POSIX Semaphores.
   Copyright (c) 2004-2008 UnixLib Developers.  */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define PTHREAD_DEBUG_SEMS */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>

#include <pthread.h>
#include <internal/unix.h>

#ifdef PTHREAD_DEBUG_SEMS
#include <internal/os.h>
#endif

/* The state that sem_init allocates for a semaphore: a mutex and a condition
   variable.  The threads of UnixLib are scheduled by the library itself, and a
   thread that waits on a condition variable is not run again until it is
   signalled or, for a timed wait, until its deadline has passed.  The state is
   hung on the usem_waiters member of sem_t, where the NetBSD code that this
   file comes from kept a queue of the waiting threads.  That queue was never
   filled: sem_wait polled with pthread_yield (), which burns the time slice of
   a thread that has nothing to do, and it leaked a list entry per call;
   sem_timedwait was ENOSYS.  */
struct sem_state
{
  pthread_mutex_t lock;
  pthread_cond_t cond;
};

#define SEM_STATE(sem) ((struct sem_state *) (sem)->usem_waiters)

#define	USEM_MAGIC	0x09fa4012
#define	USEM_USER	0	/* assumes kernel does not use NULL */

/* Check validity of semaphore structure, return 0 if valid
   and non-zero if invalid.  */
static __inline int sem_check_validity (const sem_t *sem)
{
  if (sem != NULL && sem->usem_magic == USEM_MAGIC && sem->usem_waiters != NULL)
    return 0;

  return __set_errno (EINVAL);
}

/* Initialise a semaphore structure.  User has already passed a valid
   piece of memory in 'sem'.  */
int sem_init (sem_t *sem, int pshared, unsigned int value)
{
  struct sem_state *state;

  /* Semaphores shared between processes aren't supported on RISC OS */
  if (pshared)
    return __set_errno (ENOSYS);

  if (sem == NULL || value > SEM_VALUE_MAX)
    return __set_errno (EINVAL);

  state = malloc (sizeof (struct sem_state));
  if (state == NULL)
    return __set_errno (ENOMEM);
  pthread_mutex_init (&state->lock, NULL);
  pthread_cond_init (&state->cond, NULL);

  sem->usem_magic = USEM_MAGIC;
  sem->usem_semid = USEM_USER;
  sem->usem_waiters = (struct pthread_queue_t *) state;
  sem->usem_count = value;
  return 0;
}

int
sem_destroy(sem_t *sem)
{
  struct sem_state *state;

  if (sem_check_validity (sem))
    return -1;

  state = SEM_STATE (sem);
  pthread_mutex_lock (&state->lock);
  if (state->cond.waiting != NULL)
    {
      /* Threads are still waiting on it.  */
      pthread_mutex_unlock (&state->lock);
      return __set_errno (EBUSY);
    }
  pthread_mutex_unlock (&state->lock);

  sem->usem_magic = 0;
  sem->usem_waiters = NULL;
  free (state);
  return 0;
}

sem_t *
sem_open(const char *name, int oflag, ...)
{
  /* not supported under RISC OS */
  __set_errno (ENOSYS);
  return SEM_FAILED;
}

int
sem_close(sem_t *sem)
{
  /* not supported under RISC OS */
  return __set_errno (ENOSYS);
}

int
sem_unlink(const char *name)
{
  /* not supported under RISC OS */
  errno = ENOSYS;
  return (-1);
}

/* Decrement the semaphore, waiting while it is zero: until it is posted, or,
   when ABSTIME is not NULL, until the absolute time ABSTIME (on the
   CLOCK_REALTIME clock) has passed.  A semaphore that can be decremented at
   once is decremented whatever ABSTIME is.  */
static int
sem_wait_until (sem_t *sem, const struct timespec *abstime)
{
  struct sem_state *state;
  int err = 0;

  if (sem_check_validity (sem))
    return -1;

  PTHREAD_SAFE_CANCELLATION

  state = SEM_STATE (sem);
  pthread_mutex_lock (&state->lock);

  while (sem->usem_count == 0)
    {
      if (abstime != NULL)
	err = pthread_cond_timedwait (&state->cond, &state->lock, abstime);
      else
	err = pthread_cond_wait (&state->cond, &state->lock);

      /* The mutex is held again in every case.  A deadline that passed
	 just as the semaphore was posted still gets the semaphore.  */
      if (err != 0 && !(err == ETIMEDOUT && sem->usem_count != 0))
	{
	  pthread_mutex_unlock (&state->lock);
	  return __set_errno (err);
	}
    }

  sem->usem_count--;
  pthread_mutex_unlock (&state->lock);
  return 0;
}

int sem_wait (sem_t *sem)
{
  return sem_wait_until (sem, NULL);
}

int
sem_timedwait (sem_t *sem, const struct timespec *abstime)
{
  if (abstime == NULL)
    return __set_errno (EINVAL);

  return sem_wait_until (sem, abstime);
}

int
sem_trywait(sem_t *sem)
{
  struct sem_state *state;
  int retval = 0;

  if (sem_check_validity (sem))
    return -1;

  state = SEM_STATE (sem);
  pthread_mutex_lock (&state->lock);
  if (sem->usem_count == 0)
    {
      errno = EAGAIN;
      retval = -1;
    }
  else
    sem->usem_count--;
  pthread_mutex_unlock (&state->lock);
  return retval;
}

int
sem_post(sem_t *sem)
{
  struct sem_state *state;

  if (sem_check_validity (sem))
    return -1;

  state = SEM_STATE (sem);
  pthread_mutex_lock (&state->lock);
  if (sem->usem_count == SEM_VALUE_MAX)
    {
      pthread_mutex_unlock (&state->lock);
      return __set_errno (EOVERFLOW);
    }
  sem->usem_count++;
  /* Make the first waiting thread runnable (it takes the semaphore when it
     runs); a broadcast is not needed, one post allows one thread.  */
  pthread_cond_signal (&state->cond);
  pthread_mutex_unlock (&state->lock);
  return 0;
}

int
sem_getvalue(sem_t * __restrict sem, int * __restrict sval)
{
  struct sem_state *state;

  if (sem_check_validity (sem))
    return -1;

  state = SEM_STATE (sem);
  pthread_mutex_lock (&state->lock);
  *sval = (int) sem->usem_count;
  pthread_mutex_unlock (&state->lock);

  return 0;
}
