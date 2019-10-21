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
#include <malloc.h>
#include <errno.h>

#include "pqueue.h"


/*! \defgroup core_pqueue priority queue 알고리즘을 사용한 객체 정렬
 *  @{
 */

size_t pq_sizeof( int max)
{
	return ((((size_t)max) + 1) * sizeof(void *)) + sizeof(struct pq__State);
}


pq_pState pq_map( char **__p, int max, 
        int (*lessThen)( const void *, const void *, void *), void *udata)
{
	pq_pState nq;

	if (!(nq = (pq_pState)(*__p)))
		;
	else {
		nq->udata = udata;
		nq->lessThen = lessThen; {
			nq->nobject = 0;
			nq->nalloc = (nq->size = max); 
			nq->heap = (void **)(nq + 1);
		} (*__p) += pq_sizeof(max);
	} return nq;
}


int pq_remap( char **__p, int max, pq_pState *pq)
{
	int n = 0;

	if ((*pq)->nalloc >= max)
		(*pq)->size = max;
	else {
		pq_pState nq;

		if (!(nq = (pq_pState)(*__p)))
			return -1;
		else {
			n = max - (*pq)->nalloc; {
				(*pq) = nq;
				nq->nalloc = (nq->size = max);
			} nq->heap = (void **)(nq + 1);
		} (*__p) += pq_sizeof(max);
	} return n;
}


void pq_unmap( pq_pState pq, void (*__f)( void *, void *), void *arg)
{
	if ((pq->nobject > 0) && __f)
		do {
			(*__f)( pq->heap[pq->nobject], arg);
		} while ((--pq->nobject) > 0);
}


/* __upObject: offset으로 부터, ~ 0 방향으로 1/2 정렬을 한다. */
LOCAL int __upObject( int offset, pq_pState pq)
{
	void *node = pq->heap[offset]; {
		register int shift = (offset >> 1);

		while ((shift > 0) &&
				pq->lessThen( node, pq->heap[shift], pq->udata))
		{
			pq->heap[offset] = pq->heap[shift]; {
				offset = shift;
			} shift >>= 1;
		} pq->heap[offset] = node;
	} return offset;
}


/* __downObject: offset ~ pq->nobject 방향으로 1/2 정렬을 한다. */
LOCAL int __downObject( int offset, pq_pState pq)
{
	void *node = pq->heap[offset]; {
		register int shift, 
		             next;

		do {
			next = (shift = offset << 1) + 1;
			if ((next <= pq->nobject) && 
					pq->lessThen( pq->heap[next], pq->heap[shift], pq->udata))
				shift = next;

			if ((shift > pq->nobject) || 
					(pq->lessThen( pq->heap[shift], node, pq->udata) == 0))
				break;
			else {
				pq->heap[offset] = pq->heap[shift];
			} offset = shift;
		} while (true); pq->heap[offset] = node;
	} return offset;
}


/* __adjustObject: n의 값을 중심으로 재 정렬을 한다. */
LOCAL int __adjustObject( int n, pq_pState pq)
{
	int shift;

	if (!(shift = (n >> 1)))
		;
	else {
		if (pq->lessThen( pq->heap[n], pq->heap[shift], pq->udata))
			return __upObject( n, pq);
	} return __downObject( n, pq);
}


void *pq_push( pq_pState pq, void *obj)
{
	if (pq->nobject < pq->size) /* 저장 공간이 있을 경우 */
		/* -- 마지막에 저장하고 pq->nobject ~ 0 까지 정렬 */
		pq->heap[++pq->nobject] = obj;
	else {
	    errno = EOVERFLOW;
		if ((pq->nobject <= 0) ||
				pq->lessThen( obj, pq->heap[1], pq->udata)) /* top 위치와 비교 */
			; /* top 위치의 값보다 작은 경우 reject시킨다. */
		else {
			void *top = pq->heap[1];

			/* top 위치의 값보다 큰 경우: 1 ~ pq->nobject까지 수정 */
			pq->heap[1] = obj; {
				__downObject( 1, pq);
			} obj = top; /* top항목을 pop시킨다. */
		} return obj;
	} __upObject( pq->nobject, pq); return NULL;
}


void *pq_top( pq_pState pq) { return pq->nobject ? pq->heap[1]: NULL; }


void *pq_pop( pq_pState pq)
{
	void *obj;

	if (pq->nobject <= 0)
		return NULL;
	else {
		obj = pq->heap[1]; { /* top항목을 pop한다. */
			/* 마지막 위치 값을 top으로 이동 */
			pq->heap[1] = pq->heap[pq->nobject];
			pq->heap[pq->nobject--] = NULL;
		} __downObject( 1, pq); /* top 부터 마지막 까지 정렬 */
	} return obj;
}


void *pq_each( pq_pState pq, int n)
{
	if ((n <= 0) || (n > pq->nobject))
		errno = ERANGE;
	else {
		return pq->heap[n];
	} return NULL;
}



int pq_resort( pq_pState pq, int n)
{
	if ((n <= 0) || (n > pq->nobject))
		errno = ERANGE;
	else {
		return __adjustObject( n, pq);
	} return -1;
}


void *pq_remove( pq_pState pq, int n)
{
	if ((n <= 0) || (n > pq->nobject))
		errno = ERANGE;
	else {
		void *obj = pq->heap[n];

		if (n >= pq->nobject)
			pq->heap[pq->nobject--] = NULL;
		else {
			pq->heap[n] = pq->heap[pq->nobject]; {
				pq->heap[pq->nobject--] = NULL;
			} if (pq->nobject > 0) __adjustObject( n, pq);
		} return obj;
	} return NULL;
}


void *pq_update( pq_pState pq, int n, void *obj)
{
	if ((n <= 0) || (n > pq->nobject))
		errno = ERANGE;
	else {
		void *now = pq->heap[n];

		pq->heap[n] = obj; {
			__adjustObject( n, pq);
		} return now;
	} return NULL;
}


LOCAL void __T( void *__p0, void *__p1) { ; }

void pq_clean( pq_pState pq, void (*__f)( void *, void *), void *arg)
{
	if (__f == NULL) __f = __T;
	if (pq->nobject)
		do {
			(*__f)( pq->heap[pq->nobject], arg);
			pq->heap[pq->nobject] = NULL;
		} while ((--pq->nobject) > 0);
}
/* @} */
