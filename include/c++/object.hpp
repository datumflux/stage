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
#ifndef __OBJECT_HPP
#  define __OBJECT_HPP
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <atomic>

#if defined(RELEASE)
#  define OBJECT__TRACE()
#else
#  include <unistd.h>
#  define OBJECT__TRACE()   fprintf(stderr, " * %s.%d: %s - %p\n", __FUNCTION__, getpid(), SIGN.c_str(), this)
#endif

struct ObjectRef {
	ObjectRef(const char *sign)
		: SIGN(sign)
		, ref_(0) { OBJECT__TRACE(); }
	virtual ~ObjectRef() { OBJECT__TRACE(); }

	virtual ObjectRef *Ref() { ++ref_; return this; }
	virtual void       Unref() { if ((--ref_) <= 0) this->Dispose(); }

	virtual int32_t    Count() const { return ref_; }

	const std::string SIGN;
protected:
	virtual void Dispose() = 0;

private:
	std::atomic<std::int32_t> ref_;
};
#undef OBJECT__TRACE

#endif