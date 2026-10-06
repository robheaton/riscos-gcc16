/* $NetBSD: semaphore.h,v 1.2 2003/01/21 12:27:11 kleink Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#define __need_pthread_t
#include <pthread.h>

/* POSIX 1003.1b semaphores */

typedef unsigned int semid_t;

struct pthread_queue_t;

typedef	struct
{
  unsigned int	usem_magic;
  
  semid_t		usem_semid;
  
  /* Protects data below. */
  pthread_rwlock_t	usem_interlock;
  
  struct pthread_queue_t *usem_waiters;
  unsigned int	usem_count;
} sem_t;

#define	SEM_FAILED	((sem_t *)0)
#define	SEM_VALUE_MAX	(~0U)

#include <sys/cdefs.h>

__BEGIN_DECLS
extern int sem_close (sem_t *) __THROW;
extern int sem_destroy (sem_t *) __THROW;
extern int sem_getvalue (sem_t * __restrict, int * __restrict) __THROW;
extern int sem_init (sem_t *, int, unsigned int) __THROW;
extern int sem_post (sem_t *) __THROW;
extern int sem_trywait (sem_t *) __THROW;
extern int sem_unlink (const char *) __THROW;

/* This function is a cancellation point.  */
extern int sem_wait (sem_t *__sem);
extern int sem_timedwait (sem_t *__restrict __sem, const struct timespec *__restrict __abstime);

extern sem_t *sem_open (const char *, int, ...) __THROW;
__END_DECLS

#endif /* !_SEMAPHORE_H_ */
