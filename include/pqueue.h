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
#ifndef __PQUEUE_H
#  define __PQUEUE_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief Priority Queue 알고리즘을 구현
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"
#include <malloc.h>

/*! \addtogroup core_pqueue
 *  @{
 */

/* priority queue 알고리즘을 사용한, 부분 영역 정렬 루틴
 */
typedef struct pq__State {
	void **heap;
	struct {
		int nobject;			/* 저장된 항목수 */
		void *udata;
		int (*lessThen)( const void *, const void *, void *udata); 
	}; int size, nalloc;	/* 최대 저장 가능한 객체 수 */
} *pq_pState;


#define pq_size( __pq)					((__pq)->nobject)
#define pq_max( __pq)					((__pq)->size)

#define pq_left( __pq)					(pq_max( __pq) - pq_size( __pq))

/* pq_sizeof: max영역을 갖는 버퍼의 크기를 얻는다. */
EXTRN size_t pq_sizeof( int max);


/* pq_map: __p영역에 공간을 할당하고, 초기화를 한다.
 * 
 *   __p: max만큼의 영역을 할당후 다음 위치
 *
 *   bool lessThen( const void *a, const void *b) {
 *     return (a <= b); // a가 b 앞으로 정렬된다.
 *   }
 */
EXTRN pq_pState pq_map( char **__p, int max, 
        int (*lessThen)( const void *, const void *, void *), void *udata);

LOCAL inline pq_pState pq_init( int max, 
        int (*lessThen)(const void *, const void *, void *), void *udata)
{
	char *p;

	if (!(p = (char *)calloc( 1, pq_sizeof(max))))
		return NULL;
	else {
		;
	} return pq_map( &p, max, lessThen, udata);
}

/* pq_remap: 데이터 저장 공간을 확장한다.
 *   __p: max 만큼의 영역을 할당한 후의 버퍼 위치
 *   pq:  새롭게 할당된 pq_pState
 */
EXTRN int pq_remap( char **__p, int max, pq_pState *);

LOCAL inline pq_pState pq_resize( pq_pState pq, int max)
{
	if (pq->nalloc >= max)
		pq->size = max;
	else {
		char *p;

		if (!(p = (char *)realloc( (char *)pq, pq_sizeof(max))))
			return NULL;
		else {
			;
		} pq_remap( &p, max, &pq);
	} return pq;
}

/* pq_clean: 저장된 객체를 제거하고, 초기화를 진행한다. 
 *
 *    void (*f)( void *object, void *arg);
 */
EXTRN void   pq_clean( pq_pState, void (*__f)( void *object, void *arg), void *arg);

/* pq_unmap: 저장된 객체를 제거하고, pQueue를 소멸 시킨다. */
EXTRN void   pq_unmap( pq_pState, void (*__f)( void *object, void *arg), void *arg);

LOCAL inline void pq_exit( pq_pState pq, void (*__f)( void *object, void *arg), void *arg)
{
	pq_unmap( pq, __f, arg);
	free( (void *)pq);
}


/* pq_push: 객체를 추가한다.
 *   반환) pQueue의 객체를 모두 사용하였을 경우, lessThen에 의해 상위에
 *         위치한 obj를 반환한다. (제일 우선되는 obj 반환)
 */
EXTRN void  *pq_push( pq_pState, void *obj);

/* pq_pop: 객체를 제거한다. */
EXTRN void  *pq_pop( pq_pState);

/* pq_top: 우선순위가 가장 높은 객체를 반환한다. */
EXTRN void  *pq_top( pq_pState);

/* pq_resort: 객체를 재 정렬 시킨다. */
EXTRN int    pq_resort( pq_pState, int n);

/* pq_adjust: TOP 객체의 우선 순위값이 변경되었을 경우 TOP위치를 변경
 *    : 해당 위치에 올 다음 객체를 선택한다.
 */
#define pq_adjust( __p)						pq_resort( __p, 1)

/* pq_each: pq__State에 등록된 정보를 검색한다.
 *    : n 항목의 위치는 1 ~ n 의 범위를 가진다.
 */
EXTRN void *pq_each( pq_pState, int n);

/* pq_update: 해당 위치의 obj를 변경 한다. */
EXTRN void *pq_update( pq_pState, int n, void *obj);

/* pq_remove: 등록된 n번째 항목을 제거한다. */
EXTRN void *pq_remove( pq_pState, int n);

/* @} */
#endif
