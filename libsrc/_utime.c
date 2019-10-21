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
#include "utime.h"
#include <unistd.h>


/*! \defgroup core_utime msec기반의 시간을 처리하기 위한 time_t 대응 함수
 *  @{
 */

utime_t utime( struct timeval timeNow)
{
	return (utime_t)(timeNow.tv_sec * 1000LL) +
						(utime_t)(timeNow.tv_usec / 1000LL);
	/* return (timeNow.tv_sec * 1000000LL) + timeNow.tv_usec; */
}

#include <stdlib.h>

utime_t utimeNow( struct timeval *timeNow)
{
	struct timeval __x;

	(timeNow ?: (timeNow = &__x)); {
		if (gettimeofday(timeNow, NULL) < 0) abort();
	} return utime(*timeNow);
}



utime_t utimeDiff( struct timeval timeStart, struct timeval timeEnd)
{
	timeEnd.tv_sec -= timeStart.tv_sec;
	if ((timeEnd.tv_usec -= timeStart.tv_usec) >= 0)
		;
	else {
		timeEnd.tv_usec += 1000000; {
			;
		} timeEnd.tv_sec--;
	} return utime( timeEnd);
	/* (utime_t)(timeEnd.tv_sec * 1000LL) + 
						(utime_t)(timeEnd.tv_usec / 1000LL); */
}


utime_t utimeTick( struct timeval timeStart)
{
	struct timeval timeNow; {
		if (gettimeofday( &timeNow, NULL) < 0) abort();
	} return utimeDiff( timeStart, timeNow);
}


#include <time.h>


int utimeSleep( int timeSleep)
{
	if (timeSleep <= 0) return 0;
	{
		struct timespec ts;

        ts.tv_sec = timeSleep / 1000LL;
        ts.tv_nsec = (timeSleep % 1000LL) * 1000000LL;
		return nanosleep(&ts, NULL);
	}
}


struct timespec *utimeSpec(struct timeval *timeNow, int msec, struct timespec *ts)
{
	struct timeval __timeNow;

	if (timeNow == NULL) gettimeofday(timeNow = &__timeNow, NULL);
	{
        utime_t timeMills = utime(*timeNow) + msec;

		ts->tv_sec = timeMills / 1000LL;
		ts->tv_nsec = (timeMills % 1000LL) * 1000000LL;
	}
	return ts;
}

#if 0
int main( int argc, char *argv[])
{
	struct timeval timeStart, timeEnd;


	gettimeofday( &timeStart, NULL); {
		do {
			usleep( 10);
			gettimeofday( &timeEnd, NULL);
			fprintf( stderr, "%lld, %lld\n", 
					utime( timeEnd) - utime( timeStart), 
					utimeDiff( timeStart, timeEnd));
		} while (1);
	} return 0;
}
#endif
/* @} */