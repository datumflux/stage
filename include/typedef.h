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
#ifndef __TYPEDEFS_H
#  define __TYPEDEFS_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief 공통적인 부분을 구현
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <sys/types.h>

/*! \addtogroup core_typedef
 *  @{
 */

#ifndef LOCAL
#  define LOCAL         			static
#endif	/* LOCAL */

#ifndef EXTRN
#  ifdef __cplusplus
#    define EXTRN					extern "C"
#  else
#    define EXTRN					extern
#  endif
#endif	/* EXTRN */

#define __BEGIN
#define __END

#include <stdint.h>
#ifndef __cplusplus
typedef unsigned char 				uchar;
#endif

#if !defined(__bool__) && !defined(__cplusplus)
#  define __bool__
typedef enum { false = 0, true } bool;
#endif

#include <stdio.h>
#ifdef USE_CONFIG
#  include "config.h"
#endif

/* EXPORT_STARTUP: 프로그램 구동전 수행할 함수를 등록한다. */
#define EXPORT_STARTUP(__startup__)								\
	LOCAL void __startup__(void) __attribute__ ((constructor));	\
	LOCAL void __startup__(void)

/* EXPORT_EXIT: 프로그램 종료시 수행할 함수를 등록한다. */
#define EXPORT_EXIT(__exit__)									\
	LOCAL void __exit__(void) __attribute__ ((destructor));		\
	LOCAL void __exit__(void)

#include <unistd.h>

#  define PAGE_SIZE				getpagesize()
#  define PAGE_ALIGN( __n)		(((__n) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
#  define PAGE_OFFSET( __n)		((__n) & ~(PAGE_SIZE - 1))

#if defined(__MACH__)
typedef int64_t					__off64_t;
#endif

#  define bswap__x(x)				(x)
#  if __BYTE_ORDER == __LITTLE_ENDIAN
/* LITTLE-ENDIAN: x86 */
#    define bswap__16				bswap__x
#    define bswap__32				bswap__x
#    define bswap__64				bswap__x

#    define bswap__f32				bswap__x
#    define bswap__f64				bswap__x
#else
#    include <byteswap.h>

/* BIG-ENDIAN */
#    define bswap__16				bswap_16
#    define bswap__32				bswap_32
#    define bswap__64				bswap_64

LOCAL inline float bswap__f32( const float f) {
	int32_t i = bswap__32( *(int32_t *)&f); return *((float *)&i); }
LOCAL inline double bswap__f64( const double d) {
	int64_t i = bswap__64( *(int64_t *)&d); return *((double *)&i); }
#  endif

/* @} */
#endif

