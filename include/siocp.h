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
#ifndef __SIOCP_H
#  define __SIOCP_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief 세마포어 기반으로 쓰레드의 로드 벨런싱을 구현
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#ifdef USE_CONFIG
#  include "config.h"
#endif

#include "typedef.h"
#include "utime.h"

/*! \addtogroup core_siocp
 *  @{
 */

typedef struct sio_port *siocp;

/* sio_init: 프로그램에서 사용될 공유 포트 채널 및 리소스을 생성한다.
 *   입력:
 *     int   max_scp	사용할 채널수
 *     int   max_sobj	전체 scp에서 사용될 총 오브젝트 수
 *   출력:
 *     int				초기화된 총 채널수
 *
 *   ** 주) 할당된 정보를 프로그램이 종료될때 자동 회수 된다.
 */
EXTRN int  sio_init( int max_scp, int max_sobj);

/* sio_exit: 자원을 반환한다.
 */
EXTRN void sio_exit( void);


/* scp_attach: max_obj의 분산/동기화 영역을 갖는 공유 Port를 개방한다.
 *   입력:
 *     const char *ch		사용할 공유 Port의 채널 번호
 *     int max_object		분산/동기화 가능한 객체의 수
 *                          ( 0 일 경우 할당된 포트 반환)
 *   출력
 *     siocp		연결 Port (-1 할당 오류, 0 채널 사용중/초기화 안됨)
 *
 *   ** 주) 만약, sio_init()를 통해, 사용할 리소스를 할당하지 않으면,
 *          시스템이 제공하는 정보를 사용하여, 초기화를 한다.
 */
EXTRN siocp scp_attach( const char *ch,
		size_t max_object, int (*lessThen)(const void *l, const void *r, void *udata), void *udata);

/* scp_detach: 할당된 공유포트를 닫는다.
 *   ** 주) 만약, scp_wait()로 대기 중인경우 오류를 반환한다.
 */
EXTRN int scp_detach( siocp);



/******************************************************************************
 *    SIO 객체(Object) 제어에 관련된 API: 해당 API는 sio_open()이후에 사용    *
 ******************************************************************************/

typedef struct sio__object {
	void   *o;			/* 수행 명령 */

	int     priority;	/* 객체 우선순위 */
	utime_t tick;		/* 객체 등록 tick 시간 */
} *sobject;

/* sobj_each: n의 객체 정보를 얻는다.  */
EXTRN sobject sobj_each( int n);

LOCAL inline int sobj_init( int n, void *ptr)
{
	register sobject sobj;

	if ((sobj = sobj_each( n)) == NULL)
		return -1;
	else {
		sobj->o = ptr;
	} return n;
}
/* sobj_alloc: 오브젝트를 할당 받는다.
 *
 *   출력
 *     sobject		할당된 객체 포인터
 *
 *   반환
 *     >= 0 			객체 포인터의 고유 번호
 *       -1				할당 가능한 객체 없음
 *
 *   설명:
 *     siocp를 사용하고자 할때, 중요시 되는 것은 리소스 간의 충돌 
 *     해결에 있다. 이를 위해, 별도로 객체를 관리해야 하는데 이는 구현상의 
 *     많은 오버헤드를 가져와 문제를 발생 시키는 원인이 되기도 한다.
 *
 *     이 함수는 바로, 이러한 객체의 관리를 대행하여 처리하므로써, 사용중인 
 *     객체와 사용하지 않는 객체를 분리하는 역활을 담당한다.
 */
EXTRN int sobj_alloc( sobject *);

/* sobj_left: 할당 가능한 객체수를 얻는다. */
EXTRN int sobj_left();

#define sobj_max()						sobj_alloc(0)

EXTRN int sobj_regno( sobject);		/* 객체의 고유 번호 반환 */

#define sobj_refer(id)					sobj_each(sid)

/* sobj_free: scp_signal()로 전달되지 않는 오브젝트를 제거한다.  */
EXTRN int sobj_free( sobject);

/* sobj_setlink: r객체에 l객체를 연결하여 scp_signal()처리 하도록 한다. */
EXTRN int sobj_setlink( sobject r, sobject l, bool is_next);

/* scp_catch: siocp에 저장된 pObject를 받는다.
 *
 *   입력
 *     int flags		wait 처리 상태
 *     		SOBJ_WEAK	리소스 자동 반환을 사용하지 않는다.
 *     					이후, scp_release()를 통해 반환 가능하다.
 *
 *   반환
 *     sobject *	처리할 데이터
 *
 * scp_try: scp_signal()로 부터 발생된 데이터가 있는지 검사한다.
 */

/*! \enum */
enum {
    SOBJ_WAIT = 1,           /*!< 리소스 발생을 대기한다. */
	SOBJ_WEAK = (1 << 30)	/*!< 리소스 반환을 직접 한다. */
};

#define SOBJ_WAIT					SOBJ_WAIT
#define SOBJ_WEAK					SOBJ_WEAK

EXTRN int scp_catch( siocp, sobject *, int flags);
EXTRN int scp_try( siocp, long ms_wait);

/*! \brief 객체 발생을 대기한다
 *
 *    scp_try() + scp_catch() 기능을 동시에 수행한다.
 *
 *  \param sio		scp_attach()를 통해 할당받은 포트
 *  \param[in] o	반환할 객체
 *  \param[out] o	발생된 객체
 *  \param flags	SOBJ_WEAK 설정 조합
 *  \retval < 0		errno
 *  \retval = 0		객체 없음
 *  \retval > 0		남은 객체 수
 */
LOCAL inline int scp_wait( siocp sio, sobject *o, int flags)
{
	int rc;

	if ((rc = scp_try( sio, (flags & SOBJ_WAIT) ? -1: 0)) > 0)
		return scp_catch( sio, o, flags);
	else {
		;
	} return rc;
}

/* scp_look: 처리 대기중인 이벤트의 갯수를 알아낸다.  */
EXTRN int scp_look( siocp);

/* scp_iswait: 대기중인 갯수를 얻는다. (first_tick == 첫번쨰 객체의 등록 시간). */
EXTRN int scp_iswait(siocp, utime_t *first_tick);

/* scp_isover: 해당 Port가 nthrow의 비율이상을 처리하고 있는지 검사. */
EXTRN int scp_isover( siocp, float nthrow);

/* scp_signal: scp_wait()로 pObject를 보낸다.
 *
 *   입력:
 *     siocp			scp_attach()를 통해 생성된 공유 포트
 *     sobject 			sobj_alloc()를 통해 할당받은 오브젝트
 *
 *     nwakeup			동시에 활성화 시킬 slot 수
 *
 *   반환:
 *     < 0					errno
 *     = 0					저장 공간 없음
 */
EXTRN int scp_signal( siocp, sobject *, int nwakeup);


/*! \brief 데이터를 전달한다.
 *
 *    sobj_alloc()을 통해, 전달 객체를 받아 개발자가 지정한 정보를
 *    바로 전달하는 inline 함수
 *
 *  \param p	scp_attach()를 통해 할당받은 공유포트
 *  \param buf	전달할 정보
 *  \param priority	우선순위(낮을수록 높다)
 *  \retval -1		오류 발생
 *  \retval 0		성공
 */
LOCAL inline int scp_put( siocp p, void *buf, int priority)
{
	sobject sobj;

	if (sobj_alloc( &sobj) < 0)
		return -1;
	else {
		sobj->o = buf;
		sobj->priority = priority;
		if (scp_signal( p, &sobj, 0) > 0)
			return 0;
		else {
			;
		} sobj_free( sobj);
	} return -1;
}

/* scp_release: 할당되어 사용중인 object를 반환한다.
 *
 *   입력:
 *     siocp		scp_attach()를 통해 생성된 공유 Port
 *     sobject 		scp_wait()를 통해 할당받은 오브젝트
 */
EXTRN int scp_release( siocp, sobject);
/* @} */
#endif
