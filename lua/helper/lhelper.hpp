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
#ifndef __LUA_HELPER_H
#  define __LUA_HELPER_H

/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "lua.hpp"
#include <atomic>
#include <string>
#include <functional>

/* LuaJIT */
#ifndef LUA_OK
extern "C" {
#include "luajit.h"
#include "compat-5.3.h"
};
#endif

/*! \addtogroup lua_helper
 *  @{
 */
struct luaObjectHelper;

typedef int(*luaEventClosure_t)(lua_State *, luaObjectHelper *h, void *u);

/* */
#include "bson.h"
#include <unordered_map>

int luaL_pushBSON(lua_State *L, bson_iterator *); /* bson -> lua */

/* idx == 0: 그룹 생성 안함 */
inline int luaL_pushBSON(lua_State *L, int idx, const char *buffer)
{
    bson_iterator bson_it; bson_iterator_from_buffer(&bson_it, buffer);
    if (idx == 0)
        return luaL_pushBSON(L, &bson_it);
    else {
        if (idx < 0) idx = (lua_gettop(L) + idx) + 1;
        while (bson_iterator_next(&bson_it) != BSON_EOO)
        {
        	const char *k = bson_iterator_key(&bson_it);

	        luaL_pushBSON(L, &bson_it);
        	switch (*(k + 0))
	        {
	        	case '#':
		        {
		        	char *p = NULL;

		        	long i = strtol(k + 1, &p, 10);
		        	if ((i == LONG_MIN) || (i == LONG_MAX));
		        	else if (*(p + 0) == 0)
			        {
		        		lua_rawseti(L, idx, i);
		        		break;
			        }
		        }

	        	default: lua_pushstring(L, k);
		        {
		        	lua_insert(L, -2);
		        } lua_rawset(L, idx);
	        }
        }
    } return 1;
}

inline int luaL_pushBSON(lua_State *L, int idx, bson *b) {
    return (b->finished ? luaL_pushBSON(L, idx, bson_data(b)): 0);
}

/* luaL_toBSON: idx에 있는 값을 bson으로 변환, allowCount != NULL 이 아니면 LUA_USERDATA도 같이 변환 */
int luaL_toBSON(lua_State *L, int idx, const char *k, bson *b, int *allowClone); /* lua -> bson */

/* luaL_collectBSON: luaL_toBSON() 함수로 변환 중, allowClone 설정을 통해 LUA_USERDATA가 전달되는 경우,
 *                   전달된 객체의 사용을 마치고 회수를 진행한다. (allowCount > 0) */
int luaL_collectBSON(lua_State *L, bson_iterator *a_it, int *allowClone);

inline int luaL_freeBSON(lua_State *L, const char *buffer, int allowClone)
{
	bson_iterator bson_it; bson_iterator_from_buffer(&bson_it, buffer);
	int r = luaL_collectBSON(L, &bson_it, &allowClone);
	bson_free((void *)buffer);
	return r;
}

#include <jsoncpp/json/json.h>

int luaL_pushJSON(lua_State *L, Json::Value value);
int luaL_toJSON(lua_State *L, int idx, const char *__k, Json::Value *b);

/*
 * function(arg1, arg2, ...)   ==> function (arg1, arg2, ...)
 *
 * idx < 0      해당 위치의 값만 bson으로 변경
 * idx > 0      이후의 모든 값을 bson으로 변경
 */
int luaL_toargs(lua_State *L, int idx, const char *f, bson *b_in, int *allowClone);
inline const char *luaL_toargs(lua_State *L, int idx, const char *f, int *allowClone)
{
    bson b_in;

    bson_init(&b_in);
    if (luaL_toargs(L, idx, f, &b_in, allowClone) < 0)
        bson_destroy(&b_in);
    else {
        bson_finish(&b_in);
        return bson_data(&b_in);
    } return NULL;
}

/* */
#include "lock.h"
#include "c++/object.hpp"
#include <map>

struct luaObjectRef: public ObjectRef {
    luaObjectRef(const char *sign)
        : ObjectRef(sign) { }
    virtual ~luaObjectRef() { }
};

struct luaObjectHelper: public luaObjectRef {
    const uintptr_t BEGIN;

    struct Scope {
        Scope(lua_State *L): L(L), top_(lua_gettop(L)) { }
        virtual ~Scope() { lua_settop(L, top_); }

        const int operator *() { return top_; }

        lua_State *L;
    private:
        int top_;
    };

	struct GC {
		GC(lua_State *L): L(L) { }
		virtual ~GC() { lua_settop(L, 0); lua_gc(L, LUA_GCCOLLECT, 0); }

		lua_State *L;
	};


	/* BSON 변환시 데이터 전환 처리를 진행
     *    luaL_popBSON() <- luaL_toBSON() -> luaL_pushBSON()
     *               -1           0             1
     */
    virtual int transfer(lua_State *L, int op /* -1 <- 0 -> 1 */, void *u) { return 0; }

    /* metatable callback
     *
     *   1   2          n
     *   +---+--- ~~ ---+
     *   | T | argument |
     *   +---+--- ~~ ---+
     *
     *   lua_type(L, 1) == LUA_TUSERDATA (this)
     */
    virtual int __index(lua_State *L, void *u) { return 0; }
    virtual int __newindex(lua_State *L, void *u) { return 0; }
    virtual int __call(lua_State *L, void *u) { return 0; }
    virtual int __len(lua_State *L, void *u) { return 0; }
    virtual int __gc(lua_State *L, void *u) { this->Unref(); return 0; }

    /* */
    virtual int __tostring(lua_State *L, void *u) { return 0; }

    virtual int __pairs(lua_State *L, void *u) { return 0; }
    virtual int __ipairs(lua_State *L, void *u) { return 0; }

    /* */
    virtual int __eq(lua_State *L, void *u) { return 0; }   /* == */
	virtual int __lt(lua_State *L, void *u) { return 0; }   /* > */
	virtual int __le(lua_State *L, void *u) { return 0; }   /* >= */

	/* */
	virtual int __unm(lua_State *L, void *u) { return 0; } /* -udata */
	virtual int __add(lua_State *L, void *u) { return 0; }  /* a + b */
	virtual int __sub(lua_State *L, void *u) { return 0; }  /* a - b */
	virtual int __mul(lua_State *L, void *u) { return 0; }  /* a * b */
	virtual int __div(lua_State *L, void *u) { return 0; }  /* a / b */
	virtual int __mod(lua_State *L, void *u) { return 0; }  /* a % b */
	virtual int __pow(lua_State *L, void *u) { return 0; }  /* a ^ b */
	virtual int __concat(lua_State *L, void *u) { return 0; }   /* a .. b */

    /* */
    int link(lua_State *L, luaEventClosure_t closure, void *u);

    /* */
    static luaObjectHelper *unwrap(lua_State *L, int index, void **udata);

    /* */
    static int traceback(lua_State *L, int idx, int level = 2);

    /* */
    struct Callback: public std::string {
        Callback(lua_State *L = NULL, int f_idx = 0)
            : std::string() {
            if (L && (dump(L, f_idx) != LUA_OK)) throw __LINE__;
        }

        virtual int dump(lua_State *L, int f_idx);
        virtual int load(lua_State *L);

        /* */
        virtual int call(lua_State *L, bson_iterator *args, int nret = LUA_MULTRET);
        virtual int call(lua_State *L, int args, int nret = LUA_MULTRET);

    private:
        static int writer (lua_State *L, const void *b, size_t size, void *B);
    };

    static int args(lua_State *L, const char *buffer);
    static int args(lua_State *L, bson_iterator *args);

    static int resume(lua_State *L, int nargs);

    static int call(lua_State *L, int f, int narg, int nres = LUA_MULTRET);
    static int call(lua_State *L, const char *buffer, size_t size, int narg, int nres = LUA_MULTRET);

    /* */
    static int call(lua_State *L, int ref, bson_iterator *args, int nres = LUA_MULTRET);
    static int call(lua_State *L, int ref, std::function<int (lua_State *L)> args, int nres = LUA_MULTRET);

    static lua_Integer usage(lua_State *L) {
        return ((uint64_t)lua_gc(L, LUA_GCCOUNT, 0) << 10) + lua_gc(L, LUA_GCCOUNTB, 0);
    }

    static lua_Integer gc(lua_State *L) {
        lua_Integer bytes = 0;
        for (lua_Integer last = usage(L), delta;;bytes += delta)
        {
            lua_gc(L, LUA_GCCOLLECT, 0);
            if ((delta = usage(L) - last) <= 0)
                break;
        }
        return bytes;
    }

    /* */
    static int require(lua_State *L,
                       const char *f, const std::function<int(lua_State *, const char *)> &loadSource);

    luaObjectHelper(const char *sign, bool allowClone = false)
            : luaObjectRef(sign)
            , BEGIN(allowClone ? (uintptr_t)this: 0) { }
    virtual ~luaObjectHelper() { }

    /* */
    struct Bson: public luaObjectRef {
        Bson()
            : luaObjectRef("luaObjectHelper::Bson")
            , clone_(0)
            , bson_(NULL) { INIT_RWLOCK(&rwlock_); }
        virtual ~Bson() {
            if (bson_ != NULL)
                luaL_freeBSON(NULL, bson_, clone_);
            DESTROY_RWLOCK(&rwlock_);
        }

        virtual int  call(lua_State *L, int idx,
                std::function<int (lua_State *L, ::bson **b_o, int *clone)> callback);

        /* */
        virtual int  fetch(lua_State *L, const char *i, bool isref);
        virtual void assign(::bson *b_i, int allowClone);

        virtual bool assign(lua_State *L, int idx);
    protected:
        virtual void Dispose() { delete this; }

    private:
        int      clone_;
        char    *bson_;
        RWLOCK_T rwlock_;
    }; typedef std::unordered_map<std::string, Bson *> BsonSet;

    static Bson *assign(BsonSet &objects, const char *id, RWLOCK_T *rwlock = NULL);
    static Bson *fetch(BsonSet &objects, const char *id, RWLOCK_T *rwlock = NULL);
    static int   erase(BsonSet &objects, const char *id, RWLOCK_T *rwlock = NULL);

    static Bson *advance(BsonSet &objects, int offset, std::string *id = NULL, RWLOCK_T *rwlock = NULL);

    static int update(lua_State *L,
            BsonSet &objects, const char *i, const char *id, int idx, RWLOCK_T *rwlock = NULL);
    static int assign(lua_State *L, BsonSet &objects, const char *id, int idx, RWLOCK_T *rwlock = NULL);

    static void destroy(BsonSet &objects) {
        for (luaObjectHelper::BsonSet::iterator
                o_it = objects.begin(); o_it != objects.end(); ++o_it)
            o_it->second->Unref();
    }
}; void luaL_pushhelper(lua_State *L, luaObjectHelper *p, void *u = NULL);
/* inline void luaL_gc(lua_State *L, luaObjectHelper *p) { luaL_pushhelper(L, p); lua_pop(L, 1); } */

/* k_idx가 문자열 또는 숫자형이 아닌경우, 객체 위치 인덱스로 변환한다.
 *
 * stage.index = function (k_idx[, i[, r_idx]])
 *   ...
 * end
 *
 *   == 0: 완료
 *   > 0: 실패 (오류메시지 저장)
 *   < 0: 저장이 필요하지 않음
 */
int luaL_toindex(lua_State *L, const char *i, int r_idx, int k_idx);

/* luaL_setproxy: idx(table) 접근시 컬럼값 변경이 발생될때 값을 저장할 수 있도록 처리
 *    ref >= 0      테이블 소멸
 *        <  0      테이블 갱신
 */
typedef void (*luaProxyCallback)(lua_State *L, int ref, luaObjectRef *u);

int luaL_setproxy(lua_State *L, const char *i, luaProxyCallback apply = NULL, luaObjectRef *u = NULL);

/* */
#include <deque>

/* */
/* lua_State의 _G를 lua_newthread() 단위로 분리한다. */
struct luaSandboxHelper: public luaObjectHelper {

	static luaSandboxHelper *unwrap(lua_State *L)
	{
		luaSandboxHelper *G;

		{
			lua_getglobal(L, "_SANDBOX");
			G = (luaSandboxHelper *)luaObjectHelper::unwrap(L, -1, NULL);
		} lua_pop(L, 1);
		return G;
	}

	struct Scope {
        Scope(luaSandboxHelper *_this, lua_State *L, int top = -1)
                : G(_this)
                , top_((top == -1) ? lua_gettop(L): top) {
        	if (_this == NULL)
        	    G = luaSandboxHelper::unwrap(L);
        	this->L = G->newthread(L_ = L);
        }
        virtual ~Scope() { G->close(L); ((top_ >= 0) ? lua_settop(L_, top_): (void)0); }

        lua_State *L;
	    luaSandboxHelper *G;
	private:
        int top_;
        lua_State *L_;
    };

	struct GC: public luaObjectHelper::GC {
		GC(luaSandboxHelper *_this, lua_State *L)
			: luaObjectHelper::GC(L)
			, G(_this) {
			{
				if (_this == NULL)
					G = luaSandboxHelper::unwrap(this->L);
				G->attach(this->L);
			} lua_settop(this->L, 0);
		}
		virtual ~GC() { G->purge(this->L); }

		luaSandboxHelper *G;
	};

	/* */
    luaSandboxHelper(const char *sign = "luaSandboxHelper"): luaObjectHelper(sign), i_(0) { }
    virtual ~luaSandboxHelper() { }

    virtual int __index(lua_State *L, void *u);
    virtual int __newindex(lua_State *L, void *u);

    virtual int __tostring(lua_State *L, void *u);

    virtual int __len(lua_State *L, void *u);
    virtual int __pairs(lua_State *L, void *u);
    virtual int __ipairs(lua_State *L, void *u);

    /* _SANDBOX[""] = { ... } */
    virtual int __property(lua_State *L, const char *k, int v) { return 0;}

    /* pairs() or ipairs() */
    virtual int __next(lua_State *L, int step, void *u) { return 0; }

    /* virtual void Dispose() { delete this; } */

    lua_State *newthread(lua_State *O);
    bool       close(lua_State *L);

	int        purge(lua_State *L);

    /* luaL_newstate()에 연결한다. */
    luaSandboxHelper *apply(lua_State *L);

    /* */
    struct SANDBOX {
        int      r;
        uint64_t i;

        SANDBOX(lua_State *L = NULL, uint64_t i = 0)
            : r(L ? luaL_ref(L, LUA_REGISTRYINDEX): -1), i(i) { }
    }; typedef std::unordered_map<uintptr_t, SANDBOX> SandboxSet;

    struct RULE: public std::string {
        int flags;
        RULE(const char *i = "", int flags = -1)
            : std::string(i)
            , flags(flags) { }
    }; typedef std::unordered_map<std::string, RULE> RuleSet;

    RuleSet    rules;
    SandboxSet sandboxs;

    void attach(lua_State *L, lua_State *T = NULL);   /* sandbox를 연결 */
protected:
    virtual bool reset(); /* rules 정보를 제외하고 모든 정보 초기화 */

private:
    uint64_t i_;
};

/* */
LUA_API int  luaopen_helper(lua_State *L);
/* @} */
#endif
