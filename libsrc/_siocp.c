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
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>

#include "siocp.h"
#include "pqueue.h"
#include "list.h"
#include "lock.h"


/*! \defgroup core_siocp 쓰레드 관리자
 *  @{
 *
 *      Windows에서 제공되는 IOCP와 유사한 기능을 수행하는 라이브러리로,
 *      여러 쓰레드의 프로세싱을 분배하거나, 자원을 제어하는 역활을 수행한다.
 *
 */
struct sio_object {
	struct sio__object i;
	int    norder;				/* -1	사용 안됨  */
	struct list_head l_obj;
};


struct sio_port {
	sem_t *sem; 							    /* 접근 SIGNAL 번호 */
	struct {
		void *udata;
		int (*lessThen)(const void *l, const void *r, void *udata);
		struct {
			pq_pState wait;					/* 대기중인 object 리소스 */
			struct list_head l_leave;		/* 반환 대기 중인 리소스 */
		} c; LOCK_T lock;
	}; struct list_head l_scp;
};



LOCAL struct sio_context {
	struct {
		struct {
			int max_object;
			struct list_head l_object;		/* sio_object 관리 테이블 */
		}; struct list_head l_port; 		/* sio_port 관리 테이블 */
	}; LOCK_T lock;
} *SIO = NULL;


#define DEF_PRIORITY					99

/*! \brief siocp를 초기화 한다.
 *
 *  \param max_scp		최대 채널 수
 *  \param max_obj		관리할 최대 객체 수
 *  \retval < 0		오류
 *  \retval = 0		성공
 */
int sio_init( int max_scp, int max_obj)
{
#define SIO_SIZEOF( __n, __o)												 \
		sizeof(struct sio_context) + (sizeof(struct sio_port) * ((size_t)(__n))) + (((size_t)(__o)) * sizeof(struct sio_object))
	if ((SIO != NULL) ||
			!(SIO = calloc( 1, SIO_SIZEOF( max_scp, max_obj))))
		;
	else {
		SIO->max_object = max_obj;
		INIT_LIST_HEAD( &SIO->l_object); {
			struct sio_object *o = (struct sio_object *)(SIO + 1);

			do {
				o->norder = -1; {
					list_add_tail( &o->l_obj, &SIO->l_object);
				} ++o;
			} while ((--max_obj) > 0);

			INIT_LIST_HEAD( &SIO->l_port); {
				struct sio_port *p = (struct sio_port *)o; 
				
				do {
					list_add_tail( &p->l_scp, &SIO->l_port); {
						;
					} ++p;
				} while ((--max_scp) > 0);
			} INIT_LOCK( &SIO->lock);
		} return 0;
	} return -1;
}

/*! \breif siocp 사용을 마친다.
 *
 */
void sio_exit( void)
{
	if (SIO == NULL)
		return;
	else {
		/* 사용을 마친 세마포어를 회수 한다. */
		DESTROY_LOCK( &SIO->lock);
		free( SIO);
	} SIO = NULL;
}


/*! \brief 고유번호에 대응되는 객체(sobject)를 얻는다.
 *
 *  \param nobj		객체 고유번호
 *  \retval	== NULL	초기화가 안되었거나, nobj가 범위이상
 *  \retval != NULL	객체 주소
 */
sobject sobj_each( int nobj)
{
	return ((SIO == NULL) || (nobj >= SIO->max_object)) 
		? NULL: (sobject)(((struct sio_object *)(SIO + 1)) + nobj);
}


/*! \brief 객체(sobject)에 대한 고유번호를 얻는다.
 *
 *  \param o	객체 주소
 *  \retval -1		초기화 안됨
 *  \retval >= 0	객체 고유번호
 */
int sobj_regno( sobject o) 
{
	return (SIO == NULL) ? -1: 
		((struct sio_object *)o) - (struct sio_object *)(SIO + 1);
}

#define sobj_entry( ptr) 		list_entry( ptr, struct sio_object, l_obj)
#define scp_entry( ptr)			list_entry( ptr, struct sio_port, l_scp)

/*! \brief 객체를 할당 받는다.
 *
 *  \param[out] o	객체 주소
 *  \retval o != NULL	객체 고유번호
 *  \retval o == NULL	전체 객체 수
 */
int sobj_alloc( sobject *o)
{
	int nobj;
	if (o == NULL)
		nobj = ((SIO == NULL) ? 0: SIO->max_object);
	else {
		ENTER_LOCK( &SIO->lock);
		/* Object를 저장할 공간이 있는가? */
		if (list_empty( &SIO->l_object))
			nobj = -(errno = ENOSPC);
		else {
			struct sio_object *c = sobj_entry( SIO->l_object.next);

#if 0
			fprintf( stderr, " sobj_alloc: %p\n", c);
			assert(c != sobj_entry(c->l_obj.next));
#endif
			list_del_init( &c->l_obj); {
				/* 오브젝트가 존재하는 위치를 수치로 변환 한다. */
				nobj = c - (struct sio_object *)(SIO + 1);

				c->i.priority = DEF_PRIORITY;
				c->i.tick = 0;
			} *o = (sobject)c;
		} LEAVE_LOCK( &SIO->lock);
	} return nobj;
}


/*! \brief 사용가능한 객체의 수를 얻는다.
 *
 *  \retval >= 0    할당 가능한 객체의 수
 */
int sobj_left()
{
    int nobj = 0;

    ENTER_LOCK(&SIO->lock); {
        struct list_head *each; 
        
        list_for_each( each, &SIO->l_object)
            nobj++;
    } LEAVE_LOCK(&SIO->lock);

    return nobj;
}

	
/*! \brief 할당된 객체(sobject)들을 그룹화 시킨다.
 *
 *  \param r	객체
 *  \param l	연결할 객체
 *  \param is_next	r->next <= l 여부
 *  \retval < 0		errno
 *  \retval = 0		성공
 */
int sobj_setlink( sobject r, sobject l, bool is_next)
{
	/* 할당된 객체가 아니라면.. */
	if ((((struct sio_object *)r)->norder != -1) ||
			(((struct sio_object *)l)->norder != -1))
		return -(errno = EINVAL);
	else {
		struct list_head *t = &((struct sio_object *)r)->l_obj,
						 *u = &((struct sio_object *)l)->l_obj;

		(is_next ? list_move( u, t): list_move_tail( u, t));
	} return 0;
}

/*! \brief siocp에 적재되지 않은 객체를 해제 한다.
 *
 *  \param o	sobj_alloc()에서 할당받은 주소
 *  \retval	< 0		errno
 *  \retval = 0		성공
 */
int sobj_free( sobject o)
{
    if (o == NULL)
        ;
    else {
        struct sio_object *c = (struct sio_object *)o;

        if (c->norder != -1)	/* 이미 siocp에 적재된 객체라면.. */
            return -(errno = EINVAL);
        else {
            ENTER_LOCK( &SIO->lock); { 
                list_move_tail( &c->l_obj, &SIO->l_object);
            } LEAVE_LOCK( &SIO->lock);
        }
	} return 0;
}


/* __SIO_lessThen: Object의 우선순위를 판단한다.  */
LOCAL int __SIO_lessThen( 
		const struct sio_object *l, const struct sio_object *r, void *udata
	)
{
	int delta;

	if ((delta = (l->i.priority - r->i.priority)) != 0);
	else {
		if (udata == NULL);
		else if ((delta = ((struct sio_port *)udata)->
				lessThen(l->i.o, r->i.o, ((struct sio_port *)udata)->udata)) != 0)
			return (delta < 0);

		if ((delta = (l->i.tick - r->i.tick)) == 0) return (l->norder < r->norder);
	} return (delta < 0);
}


/*! \brief 저장가능한 객체수 최대 max를 갖는 공유 포트를 생성
 *
 *  \param ch		slot 번호
 *  \param max		관리 최대 객체수
 *  \return			siocp 접근 slot 주소
 */
struct sio_port *scp_attach( const char *ch,
		size_t max, int (*lessThen)(const void *l, const void *r, void *udata), void *udata)
{
	struct sio_port *p = NULL;

	if (SIO == NULL) errno = EINVAL;
	else {
		ENTER_LOCK( &SIO->lock);
		if ((max > SIO->max_object) || list_empty( &SIO->l_port)) errno = ENOSPC;
		else {
#define SIO_lessThen		(int (*)(const void *, const void *, void *))__SIO_lessThen
            struct sio_port *__p = scp_entry( SIO->l_port.next);

            if ((__p->sem = sem_open(ch, O_CREAT, 0600, 0)) == NULL)
                ;
            else {
                if (!(__p->c.wait = pq_init(
                		(max == 0) ? SIO->max_object: max, SIO_lessThen, lessThen ? __p: NULL)))
                    sem_close(__p->sem);
                else {
                	__p->udata = udata;
					(p = __p)->lessThen = lessThen;
                    INIT_LIST_HEAD( &p->c.l_leave); {
                        INIT_LOCK( &p->lock);
                    } list_del( &p->l_scp);
                }
            }
		} LEAVE_LOCK( &SIO->lock);
	} return p;
}

/* __throwObject,__throwAll: 사용을 마친, 리소스를 반환한다.
 *
 * 처리:
 *   p에 저장된 object 항목을 찾아, SIO영역으로 리소스를 반환한다.
 */
LOCAL int __throwAll( struct sio_port *p)
{
	int nobj = 0; {
        LIST_HEAD(l_leave);

        while (list_empty(&p->c.l_leave) == false)
        {
            struct sio_object *o = sobj_entry(p->c.l_leave.next);

            o->norder = -1;
            list_move_tail(&o->l_obj, &l_leave);
            nobj++;
        }

        if (nobj == 0)
            ;
        else {
            LEAVE_LOCK(&p->lock); {
            
                ENTER_LOCK( &SIO->lock); {
                    list_splice_tail( &l_leave, &SIO->l_object);
                } LEAVE_LOCK( &SIO->lock);

            } ENTER_LOCK(&p->lock);
        }
	} return nobj;
}


/*! \brief 사용을 마친 공유 포트를 제거한다.
 *
 *  \param p	scp_attach()에서 할당받는 공유포트
 *  \retval	< 0		errno
 *  \retval = 0		반환된 객체수
 */

/* sobj_revoke: pqueue에 존재하는 sio_object를 반환한다.
 *   객체가 초기화 되지 않았기 때문에 list_add_tail()을 사용
 */
LOCAL void sobj_revoke( struct sio_object *c, struct list_head *T)
{
	c->norder = -1; list_add_tail( &c->l_obj, T);
}

int scp_detach( struct sio_port *p)
{
	int nobj;

	ENTER_LOCK( &p->lock); {
		if (p->l_scp.next)	// 제거된 sio_port인지 검사한다.
			nobj = -(errno = EINVAL);
		else { 
			nobj = pq_size( p->c.wait);
			__throwAll( p); {	/* 반환 대기 중인 객체 반환 */ 
				LIST_HEAD(T);

				pq_exit( p->c.wait, (void (*)(void *, void *))sobj_revoke, &T); /* 대기 객체를 반환 */
				ENTER_LOCK( &SIO->lock); {
					list_splice_tail( &T, &SIO->l_object);
					list_add_tail( &p->l_scp, &SIO->l_port);
				} LEAVE_LOCK( &SIO->lock);
			} sem_close(p->sem);
		} LEAVE_LOCK( &p->lock);
	} return nobj;	/* 처리하지 못한 SIGNAL 수 */
}


LOCAL int __throwObject( struct sio_port *p, struct sio_object *u)
{
    list_del(&u->l_obj);
    LEAVE_LOCK(&p->lock); {

	    u->norder = -1; 
		ENTER_LOCK( &SIO->lock); { 
			list_add_tail( &u->l_obj, &SIO->l_object);
		} LEAVE_LOCK( &SIO->lock);

	} ENTER_LOCK(&p->lock);
    return 1;
}


/*! \brief scp_wait()로 반환 받은 객체를 시스템에 되돌려 준다.
 *
 *   sobj_free()와의 차이점:
 *      - sobj_free()는 scp_signal() 하기 전에 오류로 인해 반환하고자 
 *        할 경우 사용을 하지만,
 *      - scp_release()의 경우는 정상적으로 처리를 마친 객체에 대한 반환을
 *        수행한다.
 *
 *  \param p	scp_attach()에서 할당받는 공유포트
 *  \param o	sobj_alloc()을 통해 할당받은 객체 주소
 *  \retval < 0		errno
 *  \retval = 0		성공
 */
int scp_release( struct sio_port *p, sobject o)
{
	int r;

	ENTER_LOCK( &p->lock); {
		if (p->l_scp.next)	// 제거된 sio_port인가?
			r = -(errno = EINVAL);
		else {
			struct sio_object *c;

			r = (!(c = (struct sio_object *)o)
					? __throwAll( p)	// 전체 제거
					: ((c->norder == -1) ? -(errno = EINVAL): __throwObject(p, c)));
		} LEAVE_LOCK( &p->lock);
	} return r;
}

/*! \brief 공유포트가 nthrow의 비율이상을 처리하고 있는지 검사한다.
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \param nthrow	허용 대기 비율
 *  \retval < 0		errno
 *  \retval	false	nthrow 비율 미만
 *  \retval true	nthrow 비율 이상
 */
int scp_isover( struct sio_port *p, float nthrow)
{
	return (p->l_scp.next) ? -(errno = EINVAL):
		((pq_max( p->c.wait) * nthrow) <= pq_size( p->c.wait));
}

/*! \brief scp_signal()을 통해 발생된 객체를 받는다.
 *
 *  \param p		scp_attach()를 통해 할당받은 공유포트
 *  \param[in] o	반환할 객체 정보
 *  \param[out]	o	반환 받을 객체 주소
 *  \param flags	SOBJ_WEAK - 반환 객체 직접 관리
 *  \retval < 0		errno
 *  \retval >= 0	남은 객체의 총 수 (반환 객체 포함)
 */
int scp_catch( struct sio_port *p, sobject *o, int flags)
{
    int nwait = -1;

	ENTER_LOCK( &p->lock); 
	if (p->l_scp.next)
		nwait = -(errno = EPIPE);
	else { 
		struct sio_object *c;

		pq_adjust(p->c.wait);
		if ((*o))
		{
            /* 사용을 마친 자원을 반환하도록 처리한다.
             *   만약, 반환이 된 자원이라면 norder == -1로 처리된다.
             */
			if (((c = (struct sio_object *)(*o))->norder != -1) &&
					!list_empty( &c->l_obj)) /* 자동 제거 대상이 아니라면.. */
				__throwObject(p, c);
		}

        nwait = pq_size( p->c.wait);
		if ((c = (struct sio_object *)((*o) = pq_pop( p->c.wait))) == NULL)
            ;
        else {
			/* scp_signal()에서 객체가 bulk로 이동해 왔기 때문에,
			 *   list_move_tail()을 사용할 경우 메모리 오류 발생
			 */
			if (!(flags & SOBJ_WEAK))
                list_add_tail( &c->l_obj, &p->c.l_leave);
			else INIT_LIST_HEAD( &c->l_obj); /* scp_release() 제거 */
		}
	} LEAVE_LOCK( &p->lock);
	return nwait;
}

/*! \brief scp_signal()에서 발생되는 이벤트를 기다린다.
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \param iswait	객체 발생 대기 여부
 *  \retval	-1		오류 발생
 *  \retval false	객체 없음
 *  \retval true	객체 발생
 */
int scp_try( struct sio_port *p, long msec)
{
    if (msec < 0)
        return (sem_wait(p->sem) + 1);
    else {
        struct timespec timeOut;

        {
            struct timeval timeNow; gettimeofday(&timeNow, NULL);
            uint64_t timeMills =
                    ((uint64_t)(timeNow.tv_sec * 1000LL) + (uint64_t)(timeNow.tv_usec / 1000LL)) + msec;

            timeOut.tv_sec = timeMills / 1000LL;
            timeOut.tv_nsec = (timeMills % 1000LL) * 1000000LL;
        }

_gW:    if (sem_timedwait(p->sem, &timeOut) < 0)
			switch (errno)
			{
				case EAGAIN   : goto _gW; /* IPC_NOWAIT를 사용한 경우 */
                case ETIMEDOUT: return 0;
				default       : return -1;
			}
	} return 1;
}

/*! \brief 이벤트 대기중인 객체의 수를 얻는다.
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \retval < 0		errno
 *  \retval >= 0	대기 객체 수
 */
int scp_look( struct sio_port *p)
{
	if (p->l_scp.next) 
        errno = EINVAL;
    else {
        int value = 0;

        if (sem_getvalue(p->sem, &value) == 0)
            return value;
    } return -1;
}

/*! \brief 대기중 정보를 얻는다.
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \param[out] first_tick	처음 객체의 tick 값
 *  \retval < 0		errno
 *  \retval >= 0	대기 객체 수
 */
int scp_iswait(struct sio_port *p, utime_t *first_tick)
{
    int nwait = -1;

	ENTER_LOCK(&p->lock);
	if (p->l_scp.next)
        nwait = -(errno = EINVAL);
    else {
        nwait = pq_size(p->c.wait);
        if (first_tick != NULL)
            (*first_tick) = (nwait == 0)
                ? 0
                : ((struct sio_object *)pq_top(p->c.wait))->i.tick;
    } LEAVE_LOCK(&p->lock);
    return nwait;
}

/*! \brief scp_try()대기 중인 쓰레드에 이벤트를 발생시킨다.
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \param[in] o	전달할 객체의 시작주소
 *  \param[out] o	다음 전달 객체의 시작 주소
 *  \param nwakeup	동시에 활성화 쓰레드 수 (0 == 전체)
 *  \retval < 0		errno
 *  \retval = 0		저장 공간 없음
 *  \retval > 0		전달된 객체의 수
 */
int scp_signal( struct sio_port *p, sobject *o, int nwakeup)
{
	int nsignal;

	ENTER_LOCK( &p->lock);
	if (p->l_scp.next) nsignal = -(errno = EINVAL);
	else if ((nsignal = pq_left( p->c.wait)) == 0) errno = ENOSPC;
	else if (o != NULL) {
		struct sio_object *c = (struct sio_object *)(*o),
						  *b = c;

		if (nwakeup <= 0) nwakeup = nsignal;
		else if (nsignal < nwakeup) nwakeup = nsignal;

		nsignal = 0;
		do {
			if ((--nwakeup) < 0)
				break;
			else {
				/* 우선순위 결정을 위한 값을 설정한다. */
				c->norder = pq_size( p->c.wait);
				if (c->i.tick == 0) c->i.tick = utimeNow(NULL);

                if (pq_push( p->c.wait, c) || (sem_post(p->sem) < 0))
                {
                    nsignal = -(errno);
                    break;
                }
            } nsignal++;
		} while ((c = sobj_entry( c->l_obj.next)) != b);

		if (c == b)
		    (*o) = NULL;
        else {
			struct sio_object *l = sobj_entry( b->l_obj.prev);

			l->l_obj.next = &c->l_obj;
			c->l_obj.prev = &l->l_obj;
			(*o) = (sobject)c;
		}
	} LEAVE_LOCK( &p->lock); return nsignal;
}



#if 0
#include <pthread.h>

struct tmpBuffer {
	int  numLine, numAlloc;
	char pBuffer[256];
};


void *__testThread( void *pParms)
{
	struct sio_port *p = (struct sio_port *)pParms;
	sobject o = NULL;
	struct tmpBuffer *C;


	fprintf( stdout, " THREAD: %lu\n", pthread_self());
	while (scp_wait( p, &o, SOBJ_WAIT | SOBJ_WEAK) > 0)
		if ((C = o->o) == NULL)
			break;
		else {
#if 1
			fprintf( stdout, "%05d/%03d,%lu: %s", 
					C->numLine, C->numAlloc, pthread_self(), C->pBuffer);
#else
			fputs( C->pBuffer, stdout);
#endif
			scp_release( p, o);
			//pthread_yield();
		}

	fprintf( stderr, "CLOSE - THREAD\n");
	pthread_exit(NULL);
}

#include <string.h>

int main( int argc, char *argv[])
{
	struct sio_port * pShare;
	struct tmpBuffer *__T;
	int maxSeries = 10;

	if (sio_init( 10, 15) < 0)
		return fprintf( stderr, " FAILD\n");

	atexit( sio_exit);
	if (((pShare = scp_attach( 0, 15)) == NULL) ||
			(pShare == (struct sio_port *)-1) ||
			((__T = malloc( 15 * sizeof(struct tmpBuffer))) == NULL))
		return fprintf( stderr, " >> %s\n", strerror(errno));
	else {
		pthread_t thread1, thread2, thread3;
		void     *pStatus;
		int       nobj, numLine = 0;;
		sobject o, s;
		struct tmpBuffer *C = &__T[(nobj = sobj_alloc( &o))];

		s = o;
		pthread_create( &thread1, NULL, __testThread, pShare); usleep(500);
		pthread_create( &thread2, NULL, __testThread, pShare); usleep(500);
		pthread_create( &thread3, NULL, __testThread, pShare); usleep(500);
		fprintf( stderr, " -Alloc: %d\n", nobj);
		while (fgets( C->pBuffer, sizeof(C->pBuffer), stdin) != NULL)
		{
			C->numLine = ++numLine,
				C->numAlloc = nobj;
			o->o = C;

			fprintf( stderr, "*");
			sobj_setlink( s, o, false);
			while ((nobj = sobj_alloc( &o)) < 0)
			{
				while (s)
				{
					fprintf( stderr, " [%d]\n", scp_signal( pShare, &s, 0));
					usleep(1);
//					sleep(0);
				}
			} //fprintf( stderr, " -Alloc: %d (%d)\n", nobj, scp_look( pShare));
			
			if (s == NULL)
			{
				s = o;
				//fprintf( stderr, " - Start\n");
			}

			C = &__T[nobj];
		}

		while (s)
		{
			//fprintf( stderr, " -Signal\n");
			scp_signal( pShare, &s, 0);
			usleep(1);
//			sleep(0);
		}

		sleep(10);
		scp_put( pShare, NULL, 0);
		fprintf( stderr, "Exit Signal\n");
		scp_put( pShare, NULL, 0);
		fprintf( stderr, "Exit Signal\n");
		scp_put( pShare, NULL, 0);
		fprintf( stderr, "Exit Signal\n");
		pthread_join( thread1, &pStatus);
		pthread_join( thread2, &pStatus);
		pthread_join( thread3, &pStatus);
		fprintf( stderr, "END\n");
	}

	scp_detach( pShare);
	return 0;
}
#endif
/* @} */
