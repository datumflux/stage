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
#ifndef __UTIME_H
#  define __UTIME_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief msec기반의 시간을 처리하기 위한 time_t 대응 함수
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"
#include <sys/time.h>

/*! \addtogroup core_utime
 *  @{
 */
typedef uint64_t utime_t;


/* utime: timeNow를 1/1000 sec 로 변환 */
EXTRN utime_t utime( struct timeval timeNow);

/* utimeNow: 현재 시간에 대한 1/1000 sec를 반환한다. */
EXTRN utime_t utimeNow( struct timeval *);

/* utimeDiff: timeEnd시간과 timeStart시간의 차이를 반환 (1/1000 sec) */
EXTRN utime_t utimeDiff( struct timeval timeStart, struct timeval timeEnd);

/* utimeTick: 현재 시간과 timeStart를 비교하여 차이를 반환 (1/1000 sec) */
EXTRN utime_t utimeTick( struct timeval timeStart);

/* utimeSleep: timeSleep 만큼 대기 한다. (1/1000 sec) */
EXTRN int      utimeSleep( int timeSleep);

/* utimeSpec: timeNow + msec = ts */
EXTRN struct timespec *utimeSpec(struct timeval *timeNow, int msec, struct timespec *ts);

/* @} */
#endif
