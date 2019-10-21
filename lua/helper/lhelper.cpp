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
#include <zlib.h>
#include "lhelper.hpp"

#define LUA_HELPER			"HELPER"

/* */
luaObjectHelper *luaObjectHelper::unwrap(lua_State *L, int index, void **udata)
{
    luaObjectHelper **p = (luaObjectHelper **)luaL_checkudata(L, index, LUA_HELPER);
    if (p == NULL);
    else if (udata != NULL) (*udata) = *((void **)(p + 1));
    return *p;
}

#define DECLARE_EVENT(__event)			\
static int __event(lua_State *L) {		\
	void *u; luaObjectHelper *p = luaObjectHelper::unwrap(L, 1, &u); /* lua_remove(L, 1); */return p->__event(L, u);	\
}


static int __invokeClosure(lua_State *L)
{
    return (
            (int(*)(lua_State *, luaObjectHelper *, void *))lua_touserdata(L, lua_upvalueindex(3))
    )(L, (luaObjectHelper *)lua_touserdata(L, lua_upvalueindex(1)), lua_touserdata(L, lua_upvalueindex(2)));
}

int luaObjectHelper::link(lua_State *L, luaEventClosure_t closure, void *u)
{
    {
        lua_pushlightuserdata(L, this);
        lua_pushlightuserdata(L, u);
        lua_pushlightuserdata(L, (void *)closure);
    } lua_pushcclosure(L, __invokeClosure, 3);
    return 1;
}

/* */
int luaObjectHelper::Callback::dump(lua_State *L, int f_idx)
{
    Scope __scope(L);

    if (f_idx == 0);
    else if (lua_type(L, f_idx) != LUA_TFUNCTION)
        return LUA_ERRRUN;
    else lua_pushvalue(L, f_idx);

    this->clear();
    return lua_dump(L, writer, this, false);
}

int luaObjectHelper::Callback::load(lua_State *L)
{
    return luaL_loadbufferx(L, this->data(), this->length(), "=(dump)", "bt");
}

/* */
int luaObjectHelper::Callback::call(lua_State *L, bson_iterator *args, int nret)
{
    int r;

    return ((r = this->load(L)) == LUA_OK)
                ? luaObjectHelper::call(L, 0, args, nret): r;
}

int luaObjectHelper::Callback::call(lua_State *L, int args, int nret)
{
    return luaObjectHelper::call(L, this->data(), this->length(), args, nret);
}

int luaObjectHelper::Callback::writer (lua_State *L, const void *b, size_t size, void *B)
{
    ((Callback *)B)->append((const char *)b, size); return 0;
}

/* */
int luaObjectHelper::traceback(lua_State *L, int idx, int level)
{
    if (idx < 0) idx = lua_gettop(L) - (idx + 1);

    lua_getglobal(L, "debug");
    switch (lua_type(L, -1))
    {
        case LUA_TTABLE:
        case LUA_TUSERDATA:
        {
            lua_getfield(L, -1, "traceback");
            if (lua_type(L, -1) != LUA_TFUNCTION)
                lua_pop(L, 2);
            else {
                {
                    lua_pushvalue(L, idx);
                    lua_pushinteger(L, level);
                } lua_call(L, 2, 1);
                break;
            }
        } goto _gD;

        default: lua_pop(L, 1);
_gD:    {
            const char *msg = lua_tostring(L, idx);
            if (msg == NULL)
            {
                if (luaL_callmeta(L, idx, "__tostring") &&
                    (lua_type(L, -1) == LUA_TSTRING))
                    return 1;
                else msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, idx));
            }
            luaL_traceback(L, L, msg, level);  /* append a standard traceback */
        }
    }

    /* fprintf(stderr, "T %s\n", msg); */
    lua_getglobal(L, "_TRACEBACK");
    if (lua_type(L, -1) != LUA_TFUNCTION)
        lua_pop(L, 1);
    else {
        int base = lua_gettop(L);

        lua_pushvalue(L, -2);
        if (lua_pcall(L, 1, 0, base) == LUA_OK)
            return 0;
    }
    return 1;  /* return the traceback */
}

/* */
int luaObjectHelper::args(lua_State *L, const char *buffer)
{
    if (buffer == NULL) return 0;
    {
        bson_iterator b_it; bson_iterator_from_buffer(&b_it, buffer);
        return args(L, &b_it);
    }
}

int luaObjectHelper::args(lua_State *L, bson_iterator *args)
{
    if (args == NULL) return 0;
    else if (bson_iterator_type(args) != BSON_ARRAY) return luaL_pushBSON(L, args);
    {
        int start = lua_gettop(L);

        bson_iterator a_it;
        for (bson_iterator_subiterator(args, &a_it); bson_iterator_next(&a_it) != BSON_EOO;)
            luaL_pushBSON(L, &a_it);
        return lua_gettop(L) - start;
    }
}

int luaObjectHelper::resume(lua_State *L, int nargs)
{
    switch (nargs = lua_resume(L, NULL, nargs))
    {
        case LUA_OK:
        case LUA_YIELD: break;
        default: luaObjectHelper::traceback(L, -1, 1);
    }
    return nargs;
}

/* */
static int __traceback(lua_State *L) { return luaObjectHelper::traceback(L, 1, 2); }

static int aux_pcall(lua_State *L, int narg, int nres, int base)
{
    int r;

    lua_pushcfunction(L, __traceback);  /* push message handler */
    lua_insert(L, base);  /* put it under function and args */
    r = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);
    return r;
}

int luaObjectHelper::call(lua_State *L, int f, int narg, int nres)
{
    const int base = lua_gettop(L) - narg;  /* function index */

    if (f != 0)
    {
        if (lua_type(L, f) != LUA_TFUNCTION)
            return LUA_ERRRUN;
        else {
            lua_pushvalue(L, f);
            lua_insert(L, base);  /* function copy */
        }
    }
    return aux_pcall(L, narg, nres, base);
}

int luaObjectHelper::call(lua_State *L, const char *buffer, size_t size, int narg, int nres)
{
    const int base = (lua_gettop(L) - narg) + 1;  /* function index */

    if (buffer && (luaL_loadbufferx(L, buffer, size, "=(call)", "bt") != LUA_OK))
        return luaL_error(L, "%d: Unable to buffer", __LINE__);
    else {
        if (lua_type(L, -1) != LUA_TFUNCTION)
            return LUA_ERRRUN;

        lua_insert(L, base); /* function move */
    } return aux_pcall(L, narg, nres, base);
}

/* */
int luaObjectHelper::call(lua_State *L, int ref, bson_iterator *args, int nres)
{
    return call(L, ref, [&](lua_State *L) { return luaObjectHelper::args(L, args); }, nres);
}

int luaObjectHelper::call(lua_State *L, int ref, std::function<int(lua_State *L)> args, int nres)
{
    if (ref != 0) lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    {
        const int nargs = (args ? args(L): 0);
        return aux_pcall(L, nargs, nres, lua_gettop(L) - nargs);
    }
}

int luaObjectHelper::require(lua_State *L,
        const char *lpszSourceFile, const std::function<int(lua_State *, const char *)> &loadSource)
{
    int top = lua_gettop(L);
    int r = LUA_OK;

#if defined(LUA_LOADED_TABLE)
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
#else
    luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 1);
#endif
    lua_getfield(L, top + 1, lpszSourceFile);
    if (lua_toboolean(L, -1))
    	;
    else {
        lua_pop(L, 1);
        if (((r = loadSource(L, lpszSourceFile)) == LUA_OK) &&
                ((r = luaObjectHelper::call(L, 0, 0, 1)) == LUA_OK))
        {
            {
                lua_pushvalue(L, -1);
            } lua_setfield(L, top + 1, lpszSourceFile);
        }
        /* else KEEP ERROR */
    } lua_remove(L, top + 1); /* remove LUA_LOADED_TABLE */
    return r;
}

DECLARE_EVENT(__index)
DECLARE_EVENT(__newindex)
DECLARE_EVENT(__call)
DECLARE_EVENT(__tostring)
DECLARE_EVENT(__len)
DECLARE_EVENT(__pairs)
DECLARE_EVENT(__ipairs)
DECLARE_EVENT(__gc)

/* */
DECLARE_EVENT(__eq)
DECLARE_EVENT(__lt)
DECLARE_EVENT(__le)

/* */
DECLARE_EVENT(__unm)
DECLARE_EVENT(__add)
DECLARE_EVENT(__sub)
DECLARE_EVENT(__mul)
DECLARE_EVENT(__div)
DECLARE_EVENT(__mod)
DECLARE_EVENT(__pow)
DECLARE_EVENT(__concat)

static const luaL_Reg helper_meta[] = {
        { "__index"    , __index     },
        { "__newindex" , __newindex  },

        { "__call"     , __call      },
        { "__tostring" , __tostring  },

        { "__len"      , __len       },
        { "__pairs"    , __pairs     },
        { "__ipairs"   , __ipairs    },

        { "__gc"       , __gc        },
        /* */
        { "__eq"       , __eq        },
        { "__lt"       , __lt        },
        { "__le"       , __le        },
        /* */
        { "__unm"      , __unm       },
        { "__add"      , __add       },
        { "__sub"      , __sub       },
        { "__mul"      , __mul       },
        { "__div"      , __div       },
        { "__mod"      , __mod       },
        { "__pow"      , __pow       },
        { "__concat"   , __concat    },

        { NULL         , NULL        }
};


#if defined(LUAJIT_VERSION)
void luaL_pushhelper(lua_State *L, luaObjectHelper *p, void *u)
{
    {
        luaObjectHelper **v =
                (luaObjectHelper **)lua_newuserdata(L, sizeof(luaObjectHelper *) + sizeof(void *)); {
            ((*v) = p)->Ref();
            *((void **)(v + 1)) = u;
        } luaL_getmetatable(L, LUA_HELPER);
    } lua_setmetatable(L, -2);
}


int luaopen_helper(lua_State *L)
{
    lua_newtable(L); {
        int top = lua_gettop(L);

        luaL_newmetatable(L, LUA_HELPER);
        {
            int mt = lua_gettop(L);

            {
                lua_pushstring(L, "__metatable");   /* k */
                lua_pushvalue(L, top);  /* v */
            } lua_settable(L, mt);
            for (luaL_Reg *r = (luaL_Reg *) helper_meta; r->name != NULL; ++r)
            {
                lua_pushstring(L, r->name); {
                    lua_pushcclosure(L, r->func, 0);
                } lua_settable(L, mt);
            }
        } lua_setmetatable(L, top);
    } lua_pop(L, 1);
    return 0;
}
#else
void luaL_pushhelper(lua_State *L, luaObjectHelper *p, void *u)
{
    luaObjectHelper **v =
            (luaObjectHelper **)lua_newuserdata(L, sizeof(luaObjectHelper *) + sizeof(void *)); {
        ((*v) = p)->Ref();
        *((void **)(v + 1)) = u;
    } luaL_setmetatable(L, LUA_HELPER);
}


int luaopen_helper(lua_State *L)
{
    lua_createtable(L, 0, 0); {
        luaL_newmetatable(L, LUA_HELPER);
        luaL_setfuncs(L, helper_meta, 0);
    } lua_pop(L, 1);
    return 0;
}
#endif

/* */
lua_State *luaSandboxHelper::newthread(lua_State *O)
{
    lua_State *L;

    this->attach(O); {
        {
            this->attach(L = lua_newthread(O));
        } lua_pop(L, 1);
    } lua_pop(O, 2);
    return L;
}

#if defined(LUA_RIDX_GLOBALS)
luaSandboxHelper *luaSandboxHelper::apply(lua_State *L) {
    this->reset(); {
		{
			luaL_pushhelper(L, this, NULL);
		} lua_setglobal(L, "_SANDBOX");

        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);    /* _G */
        } sandboxs.insert(SandboxSet::value_type(0, SANDBOX(L, ++this->i_)));
        luaL_pushhelper(L, this, NULL);
    } lua_rawseti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
    return this;
}
#else
static int luaSandbox__index(lua_State *L) {
    return ((luaSandboxHelper *)lua_touserdata(L, lua_upvalueindex(1)))->__index(L, NULL);
}

static int luaSandbox__newindex(lua_State *L) {
    return ((luaSandboxHelper *)lua_touserdata(L, lua_upvalueindex(1)))->__newindex(L, NULL);
}


luaSandboxHelper *luaSandboxHelper::apply(lua_State *L) {
    this->reset(); {
		{
			luaL_pushhelper(L, this, NULL);
		} lua_setglobal(L, "_SANDBOX");

		{
            lua_pushvalue(L, LUA_GLOBALSINDEX);
        } sandboxs.insert(SandboxSet::value_type(0, SANDBOX(L, ++this->i_)));

        lua_pushthread(L);
        lua_newtable(L);
        {
            auto luaL_pushmethod = [this](lua_State *L, const char *k, lua_CFunction fn) {
                {
                    lua_pushlightuserdata(L, this);
                    lua_pushcclosure(L, fn, 1);
                } lua_setfield(L, -2, k);
            };

            lua_pushvalue(L, -1);
            lua_setmetatable(L, -2);
            luaL_pushmethod(L, "__index", luaSandbox__index);
            luaL_pushmethod(L, "__newindex", luaSandbox__newindex);
        } lua_setfenv(L, -2);
    } return this;
}
#endif

bool luaSandboxHelper::reset()
{
    {
        sandboxs.clear();
    } return true;
}

int luaSandboxHelper::purge(lua_State *L)
{
	int r = 0;

	for (SandboxSet::iterator r_it = sandboxs.begin(); r_it != sandboxs.end(); )
		if (r_it->first > 0)
			switch (lua_status((lua_State *)r_it->first))
			{
				case LUA_YIELD: goto _gS;
				case 0:
				{
					lua_State *C = (lua_State *)r_it->first;
					lua_Debug ar;

					if (lua_getstack(C, 0, &ar) > 0) goto _gS;  /* does it have frames? */
					else if (lua_gettop(C) > 0) goto _gS;
				}
				default: ++r;
				{
					luaL_unref(L, LUA_REGISTRYINDEX, r_it->second.r);
				} r_it = sandboxs.erase(r_it);
			}
		else
_gS:        ++r_it;
	return r;
}


bool luaSandboxHelper::close(lua_State *L)
{
    SandboxSet::iterator r_it = sandboxs.find((uintptr_t)L);
    if (r_it == sandboxs.end())
        return false;
    else {
	    {
		    luaL_unref(L, LUA_REGISTRYINDEX, r_it->second.r);
		    sandboxs.erase(r_it);
	    } this->purge(L);
    } return true;
}

void luaSandboxHelper::attach(lua_State *L, lua_State *T)
{
    SandboxSet::iterator r_it = sandboxs.find((uintptr_t)L);
    if (r_it != sandboxs.end())
        ;
    else {
        auto luaL_clone = [](lua_State *L, int ref, int idx) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            for (lua_pushnil(L); lua_next(L, -2) != 0;)
                switch (lua_type(L, -1))
                {
                    default:
                    {
                        {
                            lua_pushvalue(L, -2); /* k v k */
                            lua_insert(L, -2);    /* k k v */
                        } lua_rawset(L, idx);      /* k */
                    } break;

                    case LUA_TUSERDATA:
                    case LUA_TTHREAD: lua_pop(L, 1);    /* thread는 복제하지 않음 */
                }
            lua_pop(L, 1);
        };

        SANDBOX *clone = NULL;
        /* 등록된 정보는 모두 복재된 상태이므로, 마지막에 복재된 SANDBOX를 찾는다. */
        for (SandboxSet::iterator r_it = sandboxs.begin(); r_it != sandboxs.end(); ++r_it)
            if (r_it->first > 0) switch (lua_status((lua_State *)r_it->first))
            {
                case LUA_YIELD: break;
                case 0: if ((uintptr_t)L != r_it->first)
                {
                    lua_State *C = (lua_State *)r_it->first;
                    lua_Debug ar;

                    if (lua_getstack(C, 0, &ar) <= 0);
                    else if ((clone == NULL) ||
                            (clone->i < r_it->second.i)) clone = &r_it->second;
                } break;
                default: break;
            }

        lua_newtable(L);
        if (clone != NULL)
            luaL_clone(L, clone->r, lua_gettop(L));
        r_it = sandboxs.insert(
                SandboxSet::value_type((uintptr_t)L, SANDBOX(L, ++this->i_))
        ).first;
    } lua_rawgeti((T ? T: L), LUA_REGISTRYINDEX, r_it->second.r);
}


/* private_space -> _G[ sandbox[0] ] */
int luaSandboxHelper::__index(lua_State *L, void *u)
{
    this->attach(L);
    {
    	lua_pushvalue(L, 2);
    	lua_gettable(L, -2);
        switch (lua_type(L, -1))
        {
        	case LUA_TNIL: lua_pop(L, 1);
        	case LUA_TNONE:
	        {
		        lua_rawgeti(L, LUA_REGISTRYINDEX, sandboxs[0].r);
		        {
			        lua_pushvalue(L, 2);
		        } lua_rawget(L, -2);
	        } break;
	        default: break;
        }
    }

	if (lua_type(L, -1) == LUA_TTABLE) switch (lua_type(L, 2))
    {
        case LUA_TSTRING:
        {
            RuleSet::iterator r_it = rules.find(lua_tostring(L, 2));
            if (r_it != rules.end())
                luaL_setproxy(L, r_it->second.c_str());
        } break;
        default: break;
    } return 1;
}

int luaSandboxHelper::__newindex(lua_State *L, void *u)
{
    if (lua_type(L, -2) == LUA_TSTRING)
    {
        const char *k = lua_tostring(L, -2);

        switch (*(k + 0))
        {
            case 0: luaL_checktype(L, -1, LUA_TTABLE); /* _SANDBOX[""] = { ... } */
            {
                std::string x__k;
                char       *x__p;

                auto assignRule = [this, &x__k, &x__p](RuleSet::iterator r_it, int flags) {
                    if (r_it != this->rules.end())
                        r_it->second.flags = flags; /* r,w */
                    else {
                        r_it = this->rules.insert(
                                RuleSet::value_type(x__k.c_str(), RULE("thread", flags))
                        ).first;
                    }
                    if (x__p != NULL) r_it->second.assign(x__p + 1);
                };

                auto parseK = [&x__k, &x__p](const char *k) {
                    if ((x__p = (char *)strchr((x__k = k).c_str(), ':')) != NULL)
                        *(x__p + 0) = 0;
                    return x__k.c_str();
                };

                /* */
                lua_rawgeti(L, LUA_REGISTRYINDEX, sandboxs[0].r);
                for (lua_pushnil(L); lua_next(L, -3); )
                    switch (lua_type(L, -2))
                    {
                        /* "name:ref_name" */
                        case LUA_TSTRING: if (__property(L, k = lua_tostring(L, -2), -1)) goto _gO;
                        {
                            RuleSet::iterator r_it = rules.find(parseK(k + (*(k + 0) == '@')));
                            switch (lua_type(L, -1))
                            {
                                default: assignRule(r_it, 0); goto _gI;

                                case LUA_TNIL: if (r_it == rules.end()) break;
                                rules.erase(r_it);
_gI:                            {
                                    lua_pushvalue(L, -2);
                                    lua_insert(L, -2);
                                } lua_rawset(L, -4);
                            }
                        } break;

                        case LUA_TNUMBER: switch (lua_type(L, -1))
                        {
                            case LUA_TNIL: for (RuleSet::iterator r_it = rules.begin(); r_it != rules.end(); )
                            {
                                {
                                    lua_pushstring(L, r_it->first.c_str());
                                    lua_pushnil(L);
                                } lua_rawset(L, -5);
                                r_it = rules.erase(r_it);
                            } __property(L, NULL, 0); break;

                            /* "name:ref_name" */
                            case LUA_TSTRING: if (__property(L, k = lua_tostring(L, -1), 0) == 0)
                            {
                                assignRule(rules.find(parseK(k + (*(k + 0) == '@'))), 1);
                            } break;

                            default: break;
                        }
_gO:                    default: lua_pop(L, 1);
                    }
            } return 0;

            default:
            {
                RuleSet::iterator r_it = rules.find(k);
                if (r_it != rules.end())
                    switch (r_it->second.flags)
                    {
                        case 0: return luaL_error(L, "%d: read only", __LINE__); /* read-only */
                        default: lua_rawgeti(L, LUA_REGISTRYINDEX, sandboxs[0].r); /* 공유 설정된 경우 */
                        {
	                        {
		                        lua_pushvalue(L, 2); /* k */
		                        lua_pushvalue(L, 3); /* v */
	                        } lua_rawset(L, -3);
                        } goto _gS;
                    }
            } break;
        }
    }

    this->attach(L);
	{
        lua_pushvalue(L, 2); /* k */
        lua_pushvalue(L, 3); /* v */
    } lua_settable(L, -3);
_gS:return 1;
}

int luaSandboxHelper::__len(lua_State *L, void *u)
{
    SandboxSet::iterator r_it = sandboxs.find((uintptr_t)L);

    if (r_it == sandboxs.end())
        lua_pushinteger(L, 0);
    else {
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, r_it->second.r);
        } lua_len(L, -1);
    } return 1;
}

/* */
struct S__ITER {
    int p_step; /* process */

    void *u;
    luaSandboxHelper *helper;

    static int __next(lua_State *L);
    static int __ipairsaux(lua_State *L);
};

int S__ITER::__next(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
    {
        S__ITER *c = (S__ITER *)lua_touserdata(L, lua_upvalueindex(1));

        if (c->p_step > 0)
            ;
        else {
            lua_pushvalue(L, 2); /* k */
            if (lua_next(L, 1))
                return 2;
        }

        if (c->helper->__next(L, ++c->p_step, c->u) > 0)
            return 2;
        lua_pushnil(L);
    }
    return 1;
}

int S__ITER::__ipairsaux (lua_State *L) {
    {
        lua_Integer i = luaL_checkinteger(L, 2) + 1;

        lua_pushinteger(L, i);
        if (lua_geti(L, 1, i) != LUA_TNIL)
            return 2;
        lua_pushnil(L);
    }
    return 1;
}


int luaSandboxHelper::__pairs(lua_State *L, void *u)
{
    this->attach(L);
    if (luaL_getmetafield(L, 2, "__pairs") == LUA_TNIL) {  /* no metamethod? */
        {
            S__ITER *b = (S__ITER *)lua_newuserdata(L, sizeof(S__ITER));

            if (b == NULL)
                return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
            else {
                b->u = u;
                b->p_step = 0;
                b->helper = this;
            }
        } lua_pushcclosure(L, S__ITER::__next, 1); /* will return generator, */
        lua_pushvalue(L, 2);
        lua_pushnil(L);
    }
    else {
        lua_pushvalue(L, 2);
        lua_call(L, 1, 3);  /* get 3 values from metamethod */
    } return 3;
}

int luaSandboxHelper::__ipairs(lua_State *L, void *u)
{
    {
        S__ITER *b = (S__ITER *)lua_newuserdata(L, sizeof(S__ITER));

        if (b == NULL)
            return luaL_error(L, "%d: %s", __LINE__, strerror(errno));
        else {
            b->u = u;
            b->p_step = -1;
            b->helper = this;
        }
    } lua_pushcclosure(L, S__ITER::__ipairsaux, 1); /* iteration function */

    this->attach(L);
    lua_pushinteger(L, 0);  /* initial value */
    return 3;
}

int luaSandboxHelper::__tostring(lua_State *L, void *u)
{
    lua_pushfstring(L, "sandbox: %p", L, this);
    return 1;
}

/* */
/* r_idx: table, k_idx: key */
int luaL_toindex(lua_State *L, const char *i, int r_idx, int k_idx)
{
    switch (lua_type(L, k_idx))
    {
        default: lua_getglobal(L, "stage");
        {
            switch (lua_type(L, -1))
            {
                case LUA_TTABLE:
                case LUA_TUSERDATA: lua_getfield(L, -1, "index");
                {
                    switch (lua_type(L, -1))
                    {
                        case LUA_TTABLE: lua_remove(L, -2);
                        {
                            lua_pushstring(L, i);
                            lua_rawget(L, -2);
                            if (lua_isfunction(L, -1) == false)
                            {
                                lua_pop(L, 1);
                                break;
                            }
                        }

                        case LUA_TFUNCTION: lua_remove(L, -2);
                        {
                            lua_pushvalue(L, k_idx); /* k */

                            lua_pushstring(L, i);
                            (r_idx ? lua_pushvalue(L, r_idx): lua_newtable(L)); /* t */
                            if (luaObjectHelper::call(L, 0, 3) == LUA_OK)
                                switch (lua_type(L, -1))
                                {
                                    case LUA_TNIL: return -1; /* 저장이 필요하지 않음 */

                                    default:
                                    {
                                    	luaL_tolstring(L, -1, NULL);
                                    } lua_remove(L, -2);
                                    case LUA_TNUMBER:
                                    case LUA_TSTRING: return 0;
                                }
                        } return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
                        default: lua_pop(L, 2);
                    }
                } break;
                default: lua_pop(L, 1);
            }
            luaL_tolstring(L, k_idx, NULL);
        } return 0;
        case LUA_TSTRING:
        case LUA_TNUMBER: lua_pushvalue(L, k_idx);
    }
    return 0;
}


static void X__pushrefCallback(lua_State *L, int ref, luaObjectRef *u) { };

static int luaB_next(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
    if (lua_next(L, 1))
        return 2;
    else {
        lua_pushnil(L);
    } return 1;
}

static int ipairsaux (lua_State *L) {
    lua_Integer i = luaL_checkinteger(L, 2) + 1;
    lua_pushinteger(L, i);
    return (lua_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
}

int luaL_setproxy(lua_State *L, const char *i, luaProxyCallback apply, luaObjectRef *u)
{
    static struct PROXY_luaObject: public luaObjectHelper
    {
        struct Callback: public luaObjectRef {
            std::string   i;
            int           ref;
            lua_State    *L;
            luaObjectRef *udata;

	        luaProxyCallback apply;
            Callback(const char *i, luaProxyCallback apply, luaObjectRef *u)
                    : luaObjectRef("PROXY_luaObject::Callback")
                    , i(i)
                    , ref(-1)
                    , L(NULL)
                    , udata(u)
                    , apply(apply ? apply: X__pushrefCallback) { }
            virtual ~Callback() {
	            this->apply(L, ref, udata);
                if (this->L != NULL)
                    luaL_unref(L, LUA_REGISTRYINDEX, ref);
            }

        protected:
            virtual void Dispose() { delete this; }
        };

	    PROXY_luaObject(): luaObjectHelper("PROXY_luaObject") { }
        virtual ~PROXY_luaObject() { }

        virtual int __index(lua_State *L, void *u) {
            Callback *c = (Callback *)u;

            lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref); /* k t */
            if (luaL_toindex(L, c->i.c_str(), 3, 2) == 0)
            {
                lua_gettable(L, -2);
                if (lua_type(L, -1) == LUA_TTABLE)
                {
                    c->Ref();
                    return luaL_setproxy(L, c->i.c_str(), [](lua_State *L, int ref, luaObjectRef *u) {
                        Callback *c = (Callback *)u;

                        if (ref >= 0)
                            c->Unref();
                        else {
                            /* root 테이블을 전달하여, 전체 값이 변경된것 처럼 처리한다. */
                            lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref); {
                                c->apply(L, -1, c->udata);
                            } lua_pop(L, 1);
                        }
                    }, c);
                }
            }
            return 1;
        }

        virtual int __newindex(lua_State *L, void *u) {
            Callback *c = (Callback *)u;

            lua_rawgeti(L, LUA_REGISTRYINDEX, c->ref); /* k v t */
            if (luaL_toindex(L, c->i.c_str(), 4, 2) == 0) /* k v t k */
            {
	            {
		            lua_pushvalue(L, 3); /* k v t k v */
	            } lua_settable(L, -3); /* k v t */
                c->apply(L, -1, c->udata);
            }
            return 1;
        }

        virtual int __tostring(lua_State *L, void *u) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ((Callback *)u)->ref); {
                luaL_tolstring(L, -1, NULL);
            } return 1;
        }

        virtual int __len(lua_State *L, void *u) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ((Callback *)u)->ref); {
                lua_len(L, -1);
            } return 1;
        }

        virtual int __pairs(lua_State *L, void *u) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ((Callback *)u)->ref);
            if (luaL_getmetafield(L, 2, "__pairs") == LUA_TNIL) {  /* no metamethod? */
                lua_pushcfunction(L, luaB_next);  /* will return generator, */
                lua_pushvalue(L, 2);
                lua_pushnil(L);
            }
            else {
                lua_pushvalue(L, 2);
                lua_call(L, 1, 3);  /* get 3 values from metamethod */
            } return 3;
        }

        virtual int __ipairs(lua_State *L, void *u) {
            lua_pushcfunction(L, ipairsaux); { /* iteration function */
                lua_rawgeti(L, LUA_REGISTRYINDEX, ((Callback *)u)->ref);
                lua_pushinteger(L, 0);  /* initial value */
            } return 3;
        }

        virtual int  __gc(lua_State *L, void *u)
        {
            if (u != NULL) ((Callback *)u)->Unref();
            return this->luaObjectHelper::__gc(L, NULL);
        }

        virtual void Dispose() { }
    } PROXY;

    if (lua_type(L, -1) != LUA_TTABLE) return 0;
    {
	    PROXY_luaObject::Callback *r = new PROXY_luaObject::Callback(i, apply, u);
	    {
		    r->Ref();
		    r->ref = luaL_ref(L, LUA_REGISTRYINDEX);
		    {
		    	lua_getglobal(L, "_LUA");
		    	r->L = (lua_type(L, -1) == LUA_TNIL) ? L: lua_tothread(L, -1);
	    	} lua_pop(L, 1);
	    } luaL_pushhelper(L, &PROXY, r);
    } return 1;
}

/* */
int luaL_collectBSON(lua_State *L, bson_iterator *a_it, int *allowClone)
{
    switch (bson_iterator_type(a_it))
    {
        case BSON_BINDATA: switch (bson_iterator_bin_type(a_it))
        {
            case BSON_BIN_USER - BSON_OBJECT: /* luaObjectHelper */
            {
                uintptr_t *x__b = (uintptr_t *)bson_iterator_bin_data(a_it);

                if (((luaObjectHelper *)x__b[0])->transfer(L, -1, (void *)x__b[1]) >= 0)
                    --(*allowClone);
            } break;

            default: break;
        } break;

        case BSON_ARRAY:
        case BSON_OBJECT: if ((*allowClone) > 0)
        {
            bson_iterator b_it;

            for (bson_iterator_subiterator(a_it, &b_it); bson_iterator_next(&b_it) != BSON_EOO; )
                if (luaL_collectBSON(L, &b_it, allowClone) <= 0);
                else if ((*allowClone) <= 0) break;
        } break;
        default: return 0;
    }

    return 1;
}


auto bson_iterator_gunzip = [](bson_iterator *a_it,
                               std::function<void(const char *o, size_t l)> callback)
{
    z_stream strm = {0};

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = (unsigned char *)bson_iterator_bin_data(a_it);
    strm.avail_in = bson_iterator_bin_len(a_it);
#define windowBits          15
#define ENABLE_ZLIB_GZIP    32
    if (inflateInit2(&strm, windowBits | ENABLE_ZLIB_GZIP) < 0)
        _gC:        ;
    else {
        std::basic_string<unsigned char> o__buffer;
#define ZIP_CHUNK           0x4000
        unsigned char x__out[ZIP_CHUNK];

        do {
            strm.avail_out = ZIP_CHUNK;
            strm.next_out = x__out;
            if (inflate (&strm, Z_NO_FLUSH) < 0)
                goto _gC;
            o__buffer.append(x__out, ZIP_CHUNK - strm.avail_out);
        } while (strm.avail_out == 0);
        callback((const char *)o__buffer.data(), (size_t)o__buffer.length());
    }
    inflateEnd(&strm);
};

/* */
int luaL_pushBSON(lua_State *L, bson_iterator *a_it)
{
    switch (bson_iterator_type(a_it))
    {
        case BSON_SYMBOL:
        case BSON_STRING:
        {
            size_t length = bson_iterator_string_len(a_it) - 1; /* remove '\0' */
            lua_pushlstring(L, bson_iterator_string(a_it), length);
        } break;

        case BSON_NULL: lua_pushnil(L); break;
        case BSON_BOOL: lua_pushboolean(L, bson_iterator_bool(a_it)); break;

        case BSON_INT: lua_pushinteger(L, bson_iterator_int(a_it)); break;
        case BSON_LONG: lua_pushinteger(L, bson_iterator_long(a_it)); break;

        case BSON_DOUBLE: lua_pushnumber(L, bson_iterator_double(a_it)); break;
        case BSON_DATE: lua_pushinteger(L, bson_iterator_time_t(a_it)); break;

        case BSON_BINDATA: switch (bson_iterator_bin_type(a_it))
        {
            case BSON_BIN_USER - BSON_OBJECT: /* luaObjectHelper */
            {
                uintptr_t *x__b = (uintptr_t *)bson_iterator_bin_data(a_it);

                ((luaObjectHelper *)x__b[0])->transfer(L, 1, (void *)x__b[1]);
            } break;

            case BSON_BIN_USER - BSON_STRING: /* compress: string */
            {
                bson_iterator_gunzip(a_it, [&](const char *o, size_t l) {
                    lua_pushlstring(L, o, l);
                });
            } break;

            case BSON_BIN_USER - BSON_CODE: /* compress: function */
            {
                bson_iterator_gunzip(a_it, [&](const char *o, size_t l) {
                    luaL_loadbufferx(L, o, l, bson_iterator_key(a_it), "bt");
                });
            } break;

            /* */
            case BSON_BIN_FUNC:
            {
                luaL_loadbufferx(L, bson_iterator_bin_data(a_it),
                        bson_iterator_bin_len(a_it), bson_iterator_key(a_it), "bt");
            } break;
            default: lua_pushlstring(L, bson_iterator_bin_data(a_it), bson_iterator_bin_len(a_it));
        } break;

        case BSON_ARRAY:
        case BSON_OBJECT: lua_newtable(L);
        {
            bool isArray = (bson_iterator_type(a_it) == BSON_ARRAY);
            bson_iterator b_it;

            for (bson_iterator_subiterator(a_it, &b_it); bson_iterator_next(&b_it) != BSON_EOO; )
                if (luaL_pushBSON(L, &b_it) > 0)
                {
                    char *k = (char *)bson_iterator_key(&b_it);

                    if (isArray || *(k + 0) == '#')
                    {
                        long i = strtol(k + (*(k + 0) == '#'), &k, 10);
                        if ((i == LONG_MIN) || (i == LONG_MAX));
                        else if (*(k + 0) == 0)
                        {
                            lua_rawseti(L, -2, i);
                            continue;
                        }
                    }
                    {
                        lua_pushstring(L, bson_iterator_key(&b_it));
                        lua_insert(L, -2);
                    } lua_rawset(L, -3);
                }
        } break;
        default: return 0;
    }

    return 1;
}

/* */
int luaL_toJSON(lua_State *L, int idx, const char *__k, Json::Value *b)
{
    std::string k__p = __k ? __k: "";
    const char *k = k__p.c_str();
    const char *p = NULL;

#define MARK_POS            2
    if (k__p.length() >= MARK_POS) /* "name@T" 조건인 경우만 처리하도록 제한 */
    {
        char *v = (char *)(k + (k__p.length() - MARK_POS));
        if (*(v + 0) == '@')
        {
            *(v + 0) = 0;
            p = v + 1;
        }
    }
#undef MARK_POS
#define TO_K                (*(k + 0) ? (*b)[k]: (*b))
    switch (lua_type(L, idx))
    {
        case LUA_TNONE: return 0;
        case LUA_TNIL: TO_K = Json::Value(); break;

        case LUA_TBOOLEAN: TO_K = Json::Value(lua_toboolean(L, idx)); break;
        case LUA_TNUMBER:
        {
            if (p != NULL)
                switch (*(p + 0))
                {
                    case 'l':
                    case 'i': TO_K = Json::Value((Json::Int64)lua_tointeger(L, idx)); break;
                    case 'b': TO_K = Json::Value(lua_tointeger(L, idx) > 0); break;

                    case 'f':
                    case 'd': TO_K = Json::Value(lua_tonumber(L, idx)); break;
                }
            else if (lua_isinteger(L, idx))
                TO_K = Json::Value((Json::Int64)lua_tointeger(L, idx));
            else TO_K = Json::Value(lua_tonumber(L, idx));
        } break;

        case LUA_TTABLE:
        {
            const int a_base = (idx < 0) ? (lua_gettop(L) + (idx + 1)) : idx;

            Json::Value entry;
            for (lua_pushnil(L); lua_next(L, a_base) != 0; lua_pop(L, 1))
            {
                Json::Value v;

                if (luaL_toJSON(L, -1, NULL, &v) > 0)
                    switch (lua_type(L, -2))
                    {
                        case LUA_TNUMBER: entry.append(v); break;
                        default: entry[lua_tostring(L, -2)] = v; break;
                    }
            }
            lua_settop(L, a_base);
            TO_K = entry;
        } break;

        default:
        {
            TO_K = Json::Value(luaL_tolstring(L, idx, NULL));
        } lua_pop(L, 1);
    }
    return 1;
}

int luaL_pushJSON(lua_State *L, Json::Value value)
{
    if (value.isNull()) lua_pushnil(L);
    else if (value.isString()) lua_pushstring(L, value.asCString());
    else if (value.isBool()) lua_pushboolean(L, value.asBool());
    else if (value.isNumeric()) lua_pushnumber(L, value.asDouble());
    else {
        lua_newtable(L);
        if (value.isObject())
            for (const auto &k : value.getMemberNames())
            {
                lua_pushstring(L, k.c_str());
                if (luaL_pushJSON(L, value[k]) > 0)
                    lua_rawset(L, -3);
                else lua_pop(L, 1);
            }
        else {
            int x__i = 0;

            for (int v__i = 0; v__i < (int)value.size(); ++v__i)
            {
                if (luaL_pushJSON(L, value[v__i]) > 0)
                    lua_rawseti(L, -2, ++x__i);
            }
        }
    }
    return 1;
}


/* */
#include "c++/string_format.hpp"

static int writer (lua_State *L, const void *b, size_t size, void *B)
{
    luaL_addlstring((luaL_Buffer *)B, (const char *)b, size);
    return 0;
}

int luaL_toBSON(lua_State *L, int idx, const char *__k, bson *b, int *allowClone)
{
    std::string k__p = __k ? __k: "";
    const char *k = k__p.c_str();
    const char *p = NULL;

#define MARK_POS            2
    if (k__p.length() >= MARK_POS) /* "name@T" 조건인 경우만 처리하도록 제한 */
    {
        char *v = (char *)(k + (k__p.length() - MARK_POS));
        if (*(v + 0) == '@')
        {
            *(v + 0) = 0;
            p = v + 1;
        }
    }
#undef MARK_POS

    switch (lua_type(L, idx))
    {
        case LUA_TNONE: return 0;
        case LUA_TNIL: bson_append_null(b, k); break;

        case LUA_TBOOLEAN: bson_append_bool(b, k, lua_toboolean(L, idx)); break;
        case LUA_TNUMBER:
        {
            if (p != NULL)
                switch (*(p + 0))
                {
                    case 'l': bson_append_long(b, k, lua_tointeger(L, idx)); break;
                    case 'i': bson_append_int(b, k, (int)lua_tointeger(L, idx)); break;
                    case 'b': bson_append_bool(b, k, (lua_tointeger(L, idx) > 0)); break;
                    case 'D': bson_append_time_t(b, k, lua_tointeger(L, idx)); break;

                    case 'f':
                    case 'd': bson_append_double(b, k, lua_tonumber(L, idx)); break;
                }
            else if (lua_isinteger(L, idx))
                bson_append_long(b, k, lua_tointeger(L, idx));
            else bson_append_double(b, k, lua_tonumber(L, idx));
        } break;

        case LUA_TFUNCTION:
        {
            luaL_Buffer buffer;

            luaL_buffinit(L, &buffer); {
                lua_pushvalue(L, idx);
                switch (lua_dump(L, writer, &buffer, false))
                {
                    case LUA_OK: break;
                    default: lua_pop(L, 1);
                    {

                    } return -1;
                }
            } lua_pop(L, 1);
            {
                z_stream strm;

                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
#define windowBits              15
#define GZIP_ENCODING           16
                if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                 windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) < 0)
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501
#  if defined(COMPAT53_API)
_gC:                bson_append_binary(b, k, BSON_BIN_FUNC, buffer.ptr, buffer.nelems);
#  else
_gC:                bson_append_binary(b, k, BSON_BIN_FUNC, buffer.buffer, buffer.p - buffer.buffer);
#  endif
#else
_gC:                bson_append_binary(b, k, BSON_BIN_FUNC, buffer.b, buffer.n);
#endif
                else {
                    std::basic_string<unsigned char> o__buffer;

#define ZIP_CHUNK               0x4000
                    unsigned char x__out[ZIP_CHUNK];

#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM == 501
#  if defined(COMPAT53_API)
                    strm.next_in = (unsigned char *)buffer.ptr;
                    strm.avail_in = buffer.nelems;
#  else
                    strm.next_in = (unsigned char *)buffer.buffer;
                    strm.avail_in = buffer.p - buffer.buffer;
#  endif
#else
                    strm.next_in = (unsigned char *)buffer.b;
                    strm.avail_in = buffer.n;
#endif
                    do {
                        strm.avail_out = ZIP_CHUNK;
                        strm.next_out = x__out;
                        if (deflate(&strm, Z_FINISH) < 0)
                            goto _gC;
                        else {
                            o__buffer.append(x__out, ZIP_CHUNK - strm.avail_out);
                        }
                    } while (strm.avail_out == 0);
                    bson_append_binary(b, k, BSON_BIN_USER - BSON_CODE,
                                       (const char *)o__buffer.data(), o__buffer.length());
                }
                deflateEnd(&strm);
            }
        } break;

        case LUA_TTABLE:
        {
            /* ids@Ai = array int */
            char a_x = 0;

            if (p != NULL)
                switch (*(p + 0))
                {
                    case 'A': a_x = *(p + 1); bson_append_start_array(b, k); break;
                    case 'O': a_x = *(p + 1); goto _gO;
                    default:  a_x = *(p + 0);
_gO:				{
                        bson_append_start_object(b, k);
                    } break;
                }
            else if (*(k + 0) != 0) bson_append_start_object(b, k);

            lua_pushnil(L);
            {
                std::string a_p;
                const int a_base = (idx < 0) ? (lua_gettop(L) + idx) : idx;

                if (a_x != 0) a_p = std::format("@%c", a_x);
                for (; lua_next(L, a_base); lua_pop(L, 1))
                {
                    std::string k;

                    switch (a_x)
                    {
                        case 'A': if (lua_type(L, -2) == LUA_TNUMBER)
                        {
                            k = std::format("%d", lua_tointeger(L, -2)).c_str();
_gA:                        {
                                if (luaL_toBSON(L, -1, k.c_str(), b, allowClone) < 0)
                                    return -1;
                            }
                        } break;
                        default: switch (lua_type(L, -2))
                        {
                            default:
                            {
                                luaObjectHelper::Scope x__scope(L);
                                k = luaL_tolstring(L, -2, NULL);
                            } break;

                            case LUA_TSTRING: k = lua_tostring(L, -2); break;
                            case LUA_TNUMBER: k = std::format("#%d", lua_tointeger(L, -2)).c_str();
                        } goto _gA;
                    }
                }
            } if ((p != NULL) || *(k + 0)) bson_append_finish_object(b);
        } break;

        case LUA_TUSERDATA: if (allowClone)
        {
            void *x__u = NULL;

            luaObjectHelper *x__p = luaObjectHelper::unwrap(L, idx, &x__u);
            if ((x__p == NULL) || (((uintptr_t)x__p) != x__p->BEGIN));
            else if (x__p->transfer(L, 0, x__u) >= 0)
            {
                uintptr_t x__b[2] = {
                        (uintptr_t)x__p, (uintptr_t)x__u
                }; (*allowClone)++;
                bson_append_binary(b, k, BSON_BIN_USER - BSON_OBJECT, (char *)x__b, sizeof(x__b));
                break;
            }
        }

        default /* case LUA_TSTRING */:
        {
            size_t length;
            const char *s = luaL_tolstring(L, idx, &length);

            if (p != NULL)
                switch (*(p + 0))
                {
                    case 'S': bson_append_symbol_n(b, k, s, (int)length); break;
                    case 'C': bson_append_code_n(b, k, s, (int)length); break;
                    case 'B': bson_append_binary(b, k, BSON_BIN_BINARY, s, (int)length); break;
                    default: bson_append_string_n(b, k, s, (int)length); break;
                }
            else bson_append_string_n(b, k, s, (int)length);
        } lua_pop(L, 1); /* luaL_tolstring() */
    }
    return 1;
}


int luaL_toargs(lua_State *L, int idx, const char *f, bson *b_in, int *allowClone)
{
    if (idx < 0)
        return luaL_toBSON(L, idx, f, b_in, allowClone);
    else {
        int nargs = (lua_gettop(L) - idx) + 1;

	    if (nargs == 1)
		    luaL_toBSON(L, idx, f, b_in, allowClone);
	    else {
		    --idx;
		    bson_append_start_array(b_in, f);
		    for (int x__i = 1; x__i <= nargs; ++x__i)
                luaL_toBSON(L, idx + x__i, std::format("%d", x__i).c_str(), b_in, allowClone);
		    bson_append_finish_array(b_in);
	    } return nargs;
    }
}

/* */
int luaObjectHelper::Bson::call(lua_State *L, int idx,
        std::function<int (lua_State *L, ::bson **b_o, int *clone)> callback)
{
    int r = lua_gettop(L);
    guard::rwlock __rwlock(&this->rwlock_);

    if (idx == 0);
    else {
        if (idx < 0)
            idx = r - (idx + 1);
        lua_pushvalue(L, idx);
    }

    (this->bson_ ? (void)luaL_pushBSON(L, 0, this->bson_): lua_pushnil(L));
    if ((r = luaObjectHelper::call(L, 0, 1)) == LUA_OK)
    {
        ::bson x__o; bson_init(&x__o);
        ::bson *b_o = &x__o;
        int allowClone = 0;

        r = callback(L, &b_o, &allowClone);
        if (b_o == NULL)
            bson_destroy(&x__o);
        else {
            bson_finish(b_o);
            if (this->bson_ != NULL)
                luaL_freeBSON(L, this->bson_, this->clone_);

            this->clone_ = allowClone;
            this->bson_ = (char *)bson_data(b_o);
        }
    } return r;
}

int luaObjectHelper::Bson::fetch(lua_State *L, const char *i, bool isref)
{
    int r;
    {
        guard::rdlock __rdlock(&this->rwlock_);
        r = luaL_pushBSON(L, 0, this->bson_);
    }

    if ((i == NULL) || (r < 0));
    else if ((isref == false) || (lua_type(L, -1) != LUA_TTABLE));
    else {
        this->Ref();
        return luaL_setproxy(L, i, [](lua_State *L, int ref, luaObjectRef *u) {
	        luaObjectHelper::Bson *b = (luaObjectHelper::Bson *)u;

            if (ref >= 0)
                b->Unref();
            else {
                bson b_i; bson_init(&b_i);
                int allowClone = 0;

                if (luaL_toBSON(L, ref, "v", &b_i, &allowClone) < 0)
                	bson_destroy(&b_i);
                else b->assign(&b_i, allowClone);
            }
        }, this);
    }
    return r;
}

void luaObjectHelper::Bson::assign(::bson *b_i, int allowClone)
{
    bson_finish(b_i);
    {
        guard::rwlock __rwlock(&this->rwlock_);

        if (this->bson_ != NULL)
            luaL_freeBSON(NULL, this->bson_, this->clone_);

        this->clone_ = allowClone;
        this->bson_ = (char *)bson_data(b_i);
    }
}

bool luaObjectHelper::Bson::assign(lua_State *L, int idx)
{
    bson b_i; bson_init(&b_i);
    int allowClone = 0;

    if (luaL_toBSON(L, idx, "v", &b_i, &allowClone) < 0)
        bson_destroy(&b_i);
    else {
        this->assign(&b_i, allowClone);
        return true;
    }
    return false;
}

/* */
int luaObjectHelper::update(lua_State *L,
        BsonSet &objects, const char *i, const char *id, int idx, RWLOCK_T *rwlock)
{
    luaObjectHelper::Bson *o = NULL;

    switch (lua_type(L, idx))
    {
        default:
        {
            (rwlock ? ENTER_RWLOCK(rwlock): 0); {
                luaObjectHelper::BsonSet::iterator it = objects.find(id);
                if (it != objects.end())
                    o = it->second;
                else {
                    objects.insert(
                            luaObjectHelper::BsonSet::value_type(id, o = new luaObjectHelper::Bson())
                    );

                    o->Ref();
                } o->Ref();
            } (rwlock ? LEAVE_RWLOCK(rwlock): 0);

            switch (lua_type(L, idx))
            {
                default: o->assign(L, idx); break;

                case LUA_TFUNCTION: switch (o->call(L, idx, [&o](lua_State *L, bson **b_o, int *allowClone) {
                    switch (lua_type(L, -1))
                    {
                        case LUA_TNIL:
                        {
                            (*b_o) = NULL;
                        } return LUA_ERRERR + 1;

                        default: if (luaL_toBSON(L, -1, "v", *b_o, allowClone) < 0)
                        case LUA_TNONE: (*b_o) = NULL;
                    }
                    return LUA_OK;
                }))
                {
                    case LUA_ERRERR + 1: goto _gD;
                    default: break;
                } break;

                case LUA_TNONE: o->fetch(L, i, true);
            }
        } break;

_gD:    o->Unref();
        case LUA_TNIL: (rwlock ? ENTER_RWLOCK(rwlock): 0);
        {
            luaObjectHelper::BsonSet::iterator it = objects.find(id);
            if (it == objects.end())
                ;
            else {
                it->second->Unref();
                objects.erase(it);
            } (rwlock ? LEAVE_RWLOCK(rwlock): 0);
        } return 0;
    } o->Unref();
    return 1;
}

luaObjectHelper::Bson *luaObjectHelper::fetch(BsonSet &objects, const char *id, RWLOCK_T *rwlock)
{
    luaObjectHelper::Bson *o = NULL;

    (rwlock ? ENTER_RDLOCK(rwlock): 0); {
        luaObjectHelper::BsonSet::iterator it = objects.find(id);
        if (it != objects.end())
            (o = it->second)->Ref();
    } (rwlock ? LEAVE_RDLOCK(rwlock): 0);
    return o;
}

luaObjectHelper::Bson *luaObjectHelper::advance(BsonSet &objects, int offset, std::string *id, RWLOCK_T *rwlock)
{
    luaObjectHelper::Bson *o = NULL;

    (rwlock ? ENTER_RDLOCK(rwlock): 0); {
        luaObjectHelper::BsonSet::iterator it =
                (id && id->length()) ? objects.find(id->c_str()): objects.begin();
        if (it == objects.end())
            ;
        else {
            std::advance(it, offset);
            if (it != objects.end())
            {
                if (id != NULL)
                    id->assign(it->first.c_str());
                (o = it->second)->Ref();
            }
        }
    } (rwlock ? LEAVE_RDLOCK(rwlock): 0);
    return o;
}

int luaObjectHelper::erase(BsonSet &objects, const char *id, RWLOCK_T *rwlock)
{
    int r = 0;

    (rwlock ? ENTER_RWLOCK(rwlock): 0); {
        luaObjectHelper::BsonSet::iterator it = objects.find(id);
        if (it == objects.end())
            ;
        else {
            {
                it->second->Unref();
                objects.erase(it);
            } ++r;
        } (rwlock ? LEAVE_RWLOCK(rwlock): 0);
    } return r;
}

luaObjectHelper::Bson *luaObjectHelper::assign(BsonSet &objects, const char *id, RWLOCK_T *rwlock)
{
    luaObjectHelper::Bson *o;

    (rwlock ? ENTER_RWLOCK(rwlock): 0); {
        luaObjectHelper::BsonSet::iterator it = objects.find(id);
        if (it != objects.end())
            o = it->second;
        else {
            objects.insert(
                    luaObjectHelper::BsonSet::value_type(id, o = new luaObjectHelper::Bson())
            );

            o->Ref();
        } o->Ref();
    } (rwlock ? LEAVE_RWLOCK(rwlock): 0);
    return o;
}

int luaObjectHelper::assign(lua_State *L, BsonSet &objects, const char *id, int idx, RWLOCK_T *rwlock)
{
    bson b_i; bson_init(&b_i);
    int allowClone = 0;

    if (luaL_toBSON(L, idx, "v", &b_i, &allowClone) < 0)
        ;
    else {
        luaObjectHelper::Bson *o = luaObjectHelper::assign(objects, id, rwlock);

        if (o != NULL)
        {
            {
                o->assign(&b_i, allowClone);
            } o->Unref();
            return 0;
        }
    } bson_destroy(&b_i);
    return -1;
}