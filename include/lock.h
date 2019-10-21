/*
Copyright (c) 2018-2019 DATUMFLUX CORP.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
 */
#ifndef __LOCK_H
#  define __LOCK_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief Lock에 대한 공동적인 처리
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <pthread.h>

/*! \addtogroup core_lock
 *  @{
 */
#if defined(USE_CONFIG)
#  include "config.h"
#endif

#if defined(USE_SPINLOCK) /* spinlock */
#    ifndef _GNU_SOURCE
#       error "'_GNU_SOURCE' undeclared (first use in CFLAGS)"
#    endif

#    define LOCK_T				pthread_spinlock_t

#    define INIT_LOCK(L)		pthread_spin_init( L, 0)
#    define DESTROY_LOCK		pthread_spin_destroy

#    define ENTER_LOCK			pthread_spin_lock
#    define TRY_LOCK			pthread_spin_trylock
#    define LEAVE_LOCK			pthread_spin_unlock
#  else /* mutex 기반의 safe-lock */
#    define LOCK_T				pthread_mutex_t

#    define INIT_LOCK(L)		\
	do {	\
		static pthread_mutex_t __T =	\
			PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;	\
		(*L) = __T;	\
	} while (0)
#    define DESTROY_LOCK(L)

#    define ENTER_LOCK			pthread_mutex_lock
#    define TRY_LOCK			pthread_mutex_trylock
#    define LEAVE_LOCK			pthread_mutex_unlock
#  endif

#  ifdef __cplusplus
namespace guard {
	class lock {
	public:
		lock( LOCK_T *lck)
			: _lock( lck) { ENTER_LOCK( _lock); }
		virtual ~lock() { LEAVE_LOCK( _lock); }

	private:
		LOCK_T *_lock;
	};
};
#  endif

#  define RWLOCK_T				pthread_rwlock_t

#  define INIT_RWLOCK(L)		pthread_rwlock_init( L, NULL)
#  define INIT_RWLOCK_NP(L)		do {                                    \
    pthread_rwlockattr_t __attr;                                        \
    pthread_rwlockattr_init(&__attr);                                   \
    pthread_rwlockattr_setpshared(&__attr, PTHREAD_PROCESS_PRIVATE);    \
    pthread_rwlockattr_setkind_np(&__attr,                              \
            PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);              \
    pthread_rwlock_init( L, &__attr);                                   \
    pthread_rwlockattr_destroy(&__attr);                                \
} while (0)
#  define DESTROY_RWLOCK(L)		pthread_rwlock_destroy( L)

#  define ENTER_RDLOCK(L)		pthread_rwlock_rdlock(L)
#  define TRY_RDLOCK(L)			pthread_rwlock_tryrdlock(L)
#  define LEAVE_RDLOCK(L)		pthread_rwlock_unlock(L)

#  define ENTER_RWLOCK(L)		pthread_rwlock_wrlock(L)
#  define TRY_RWLOCK(L)			pthread_rwlock_trywrlock(L)
#  define LEAVE_RWLOCK(L)		pthread_rwlock_unlock(L)

#  ifdef __cplusplus
namespace guard {
	class rwlock {
	public:
		rwlock( RWLOCK_T *rw)
			: _rwlock( rw) { ENTER_RWLOCK( _rwlock); }
		virtual ~rwlock() { LEAVE_RWLOCK( _rwlock); }

	private:
		RWLOCK_T *_rwlock;
	};

	class rdlock {
	public:
		rdlock( RWLOCK_T *rw)
			: _rwlock( rw) { ENTER_RDLOCK( _rwlock); }
		~rdlock() { LEAVE_RDLOCK( _rwlock); }

	private:
		RWLOCK_T *_rwlock;
	};
};
#  endif
/* @} */
#endif
