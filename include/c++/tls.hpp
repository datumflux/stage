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
#ifndef __TLS_HPP
#  define __TLS_HPP
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <pthread.h>
#include <exception>

template <typename T>
class ThreadLocalStorage
{
public:
    ThreadLocalStorage() {
        if (pthread_key_create(&_key, (void (*)(void *))__destroy) != 0)
            std::__throw_runtime_error("pthread_key_create");
    }

    virtual ~ThreadLocalStorage() { pthread_key_delete(_key); }

	virtual bool operator !() { return (pthread_getspecific(_key) == NULL); }
	virtual T *operator -> () { return this->get(); }
	virtual T *operator *  () { return this->get(); }

	virtual T *get()
    {
        T *v = static_cast<T *>(pthread_getspecific(_key));
        if (v == NULL) pthread_setspecific(_key, v = new T());
        return v;
    }

private:
    static void __destroy(T *v) { delete v; }
    pthread_key_t _key;
};

#endif
