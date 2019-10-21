#include "lstage.hpp"
#include "c++/string_format.hpp"

lua__G      lua__G::_G;
const char *lua__G::TAG = "sandbox";


static std::string luaI_tostring(lua_State *L,  int idx, void *u)
{
    const char *k = luaL_tolstring(L, idx, NULL);

    lua_pop(L, 1);
    return u ? std::format("#%lu.%s", (intptr_t)u, k): k;
};

int lua__G::luaB_next(lua_State *L)
{
    void *u = NULL;
    lua__G *_this = (lua__G *)luaObjectHelper::unwrap(L, 1, &u);
    std::string k = (lua_type(L, 2) == LUA_TNIL) ? "": luaI_tostring(L, 2, u);

    luaObjectHelper::Bson *o = luaObjectHelper::advance(u
            ? _this->sandbox: _this->objects, (k.length() > 0), &k, &_this->rwlock);
    if (o == NULL)
        lua_pushnil(L);
    else {
        {
            lua_pushfstring(L, u ? strchr(k.c_str(), '.') + 1: k.c_str());
            o->fetch(L, TAG, true);
        } o->Unref();
        return 2;
    } return 1;
}

/* */
lua__G::lua__G()
        : luaObjectHelper("lua__G") { INIT_RWLOCK(&rwlock); }

lua__G::~lua__G()
{
    {
        luaObjectHelper::destroy(this->sandbox);
    } luaObjectHelper::destroy(this->objects);
    DESTROY_RWLOCK(&rwlock);
}

int lua__G::__index(lua_State *L, void *u)
{
    if (luaL_toindex(L, TAG, 0, 2) == 0) switch (lua_type(L, -1))
    {
        default:
        {
            luaObjectHelper::Bson *o = luaObjectHelper::fetch(
                    u ? this->sandbox: this->objects, luaI_tostring(L, -1, u).c_str(), &this->rwlock);
            if (o == NULL)
                return 0;
            else {
                o->fetch(L, TAG, false);
            } o->Unref();
        } break;

        case LUA_TNUMBER:
        {
            if (u != NULL)
                return luaL_error(L, "%d: The access index is incorrectly specified.", __LINE__);
        } luaL_pushhelper(L, this, (void *)((intptr_t)lua_tointeger(L, -1)));
    } return 1;
}

int lua__G::__newindex(lua_State *L, void *u)
{
    luaObjectHelper::BsonSet *o_entry = u ? &this->sandbox: &this->objects;

    if (luaL_toindex(L, TAG, 0, 2) == 0) switch (lua_type(L, -1))
    {
        default:
        {
            std::string k = luaI_tostring(L, -1, u);

            return (lua_type(L, 3) == LUA_TNIL)
                   ? luaObjectHelper::erase(*o_entry, k.c_str(), &this->rwlock)
                   : luaObjectHelper::assign(L, *o_entry, k.c_str(), 3, &this->rwlock);
        }

        case LUA_TNUMBER: switch (lua_type(L, 3))
        {
            case LUA_TNIL:
            {
                int u__i = lua_tointeger(L, -1);

                if (u__i <= 0)
                    return luaL_error(L, "%d: The access index is incorrectly specified.", __LINE__);
                else {
                    std::string u__k = std::format("#%lu.", u__i);

                    ENTER_RWLOCK(&this->rwlock);
                    for (luaObjectHelper::BsonSet::iterator i_it = o_entry->begin(); i_it != o_entry->end(); )
                        if (i_it->first.find(u__k))
                            ++i_it;
                        else {
                            i_it->second->Unref();
                            i_it = o_entry->erase(i_it);
                        }
                } LEAVE_RWLOCK(&this->rwlock);
            } return 0;
            default: break;
        } break;
    } return luaL_error(L, "%d: Object with limited variable settings.", __LINE__);
}

int lua__G::__call(lua_State *L, void *u)
{
    int r = luaL_toindex(L, TAG, 0, 2);
    if (r < 0) return 0;
    else if (r == 0)
    {
        lua_replace(L, 2);
        return luaObjectHelper::update(L, u ? this->sandbox: this->objects,
                                       TAG, luaI_tostring(L, 2, u).c_str(), 3, &this->rwlock);
    }
    return r;
}

int lua__G::__pairs(lua_State *L, void *u)
{
    {
        ;
    } lua_pushcclosure(L, luaB_next, 0); /* will return generator, */
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    return 3;
}

int lua__G::__tostring(lua_State *L, void *u)
{
    lua_pushfstring(L, "sandbox: %p [%d]", this, (uintptr_t)u);
    return 1;
}

void lua__G::Dispose() { }

/* */
int lua__G::Proxy::fetch(lua_State *L, const char *k, luaObjectHelper::Bson *o)
{
    std::string i = TAG;
    {
        guard::rdlock __rdlock(&sandbox_->rwlock);

        luaSandboxHelper::RuleSet::iterator r_it = sandbox_->rules.find(k);
        if (r_it != sandbox_->rules.end())
            i = r_it->second.c_str();
    }
    return o->fetch(L, i.c_str(), true);
}

/* */
lua__G::Proxy::Proxy(lua__G *sandbox)
        : luaSandboxHelper("lua__G::State")
        , sandbox_(sandbox) { }

lua__G::Proxy::~Proxy() { }

int lua__G::Proxy::__next(lua_State *L, int step, void *u)
{
    switch (lua_type(L, -1))
    {
        case LUA_TNIL:
        case LUA_TSTRING:
        {
            std::string k;

            if (step > 1) k.assign(lua_tostring(L, -1));
            {
                luaObjectHelper::Bson *o =
                        luaObjectHelper::advance(sandbox_->objects, (step > 1), &k, &sandbox_->rwlock);
                if (o == NULL)
                    break;
                else {
                    lua_pushstring(L, k.c_str());
                    this->fetch(L, k.c_str(), o);
                }
                o->Unref();
            }
        } return 2;
        default: break;
    }
    return luaSandboxHelper::__next(L, step, u);
}

int lua__G::Proxy::__index(lua_State *L, void *u)
{
    if (luaL_toindex(L, TAG, 0, 2) == 0) switch (lua_type(L, -1))
    {
        case LUA_TSTRING:
        {
            const char *k = lua_tostring(L, -1);

            if (strcmp(k, "__") == 0)
                luaL_pushhelper(L, sandbox_, NULL);
            else {
                luaObjectHelper::Bson *o =
                        luaObjectHelper::fetch(sandbox_->objects, k, &sandbox_->rwlock);
                if (o == NULL)
                    goto _gR;
                else {
                    this->fetch(L, k, o);
                }
                o->Unref();
            }
        } return 1;
_gR:    default: lua_replace(L, 2);
    }
    return luaSandboxHelper::__index(L, u);
}

/* luaSandboxHelper::__property() */
int lua__G::Proxy::__property(lua_State *L, const char *k, int v)
{
    if ((k == NULL) || (*(k + 0) != '*')) return 0; /* clean -- ignore */
    {
        std::string x__k = k + 1;
        char       *x__p = (char *)strchr(x__k.c_str(), ':');

        auto assignRule = [&x__k, &x__p, this](RuleSet::iterator r_it,  int flags) {
            if (r_it != sandbox_->rules.end())
                r_it->second.flags = flags;
            else {
                r_it = sandbox_->rules.insert(
                        luaSandboxHelper::RuleSet::value_type(x__k.c_str(), RULE(TAG, flags))
                ).first;
            }

            if (x__p != NULL) r_it->second.assign(x__p + 1);
        };

        if (x__p != NULL) *(x__p + 0) = 0;
        {
            guard::rwlock __rwlock(&sandbox_->rwlock);
            RuleSet::iterator r_it = sandbox_->rules.find(x__k.c_str());

            if (v != 0) switch (lua_type(L, v))
            {
                case LUA_TNIL: if (r_it != sandbox_->rules.end())
                {
                    sandbox_->rules.erase(r_it);
                    luaObjectHelper::erase(sandbox_->objects, x__k.c_str());
                } break;

                default: assignRule(r_it, 0); /* read-only */
                {
                    luaObjectHelper::assign(L, sandbox_->objects, x__k.c_str(), v);
                } break;
            }
            else assignRule(r_it, 1);
        }
    } return 1;
}

int lua__G::Proxy::__newindex(lua_State *L, void *u)
{
    if (luaL_toindex(L, TAG, 0, 2) == 0) switch (lua_type(L, -1))
    {
        case LUA_TSTRING:
        {
            const char *k = lua_tostring(L, -1);

            if (*(k + 0) != 0) switch (lua_type(L, 3))
            {
                case LUA_TNIL:
                {
                    if (luaObjectHelper::erase(sandbox_->objects, k, &sandbox_->rwlock))
                        return 1;
                } break;

                default:
                {
                    guard::rwlock __rwlock(&sandbox_->rwlock);

                    luaObjectHelper::Bson *o = luaObjectHelper::fetch(sandbox_->objects, k);
                    if (o == NULL)
                    {
                        luaSandboxHelper::RuleSet::iterator r_it = sandbox_->rules.find(k);
                        if (r_it != sandbox_->rules.end())
                            switch (r_it->second.flags)
                            {
                                case 0: return luaL_error(L, "%d: read only", __LINE__); /* read-only */
                                default: return luaObjectHelper::assign(L, sandbox_->objects, k, 3);
                            }
                        break;
                    }
                    else if (o->assign(L, 3)) o->Unref();
                    else {
                        o->Unref();
                        return luaL_error(L, "%d: Includes non-serialable data [%d]", __LINE__, lua_type(L, 3));
                    }
                } return 1;
            }
        }
        default: lua_replace(L, 2);
    }
    return luaSandboxHelper::__newindex(L, u);
}

void lua__G::Proxy::Dispose() { }

/* */
lua__Stage::GC::GC(lua__Stage *context, lua_State *L)
        : luaSandboxHelper::GC(context, L) {
}

lua__Stage::GC::~GC() { }


/* */
lua__Stage::lua__Stage(lua_State *L, lua__G *sandbox)
        : lua__G::Proxy(sandbox)
        , L(L ? L: luaL_newstate())
        , dispose_(L == NULL)
{
    if (dispose_)
    {
#if defined(LUAJIT_MODE_ENGINE)
        luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON);
#endif
        luaL_openlibs(L);
        /* {
            lua_pushcfunction(L, __luaTraceback);
        } lua_setglobal(L, "_TRACEBACK"); */
    } luaopen_helper(L);
}

lua__Stage::~lua__Stage()
{
    if (dispose_)
        lua_close(this->L);
}

lua__Stage *lua__Stage::apply()
{
    lua__G::Proxy::apply(L)->Ref(); /* apply sandbox */
    return this;
}