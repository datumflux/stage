#ifndef __LUA_STAGE_H
#  define __LUA_STAGE_H

#include "lhelper.hpp"

/*!
 * \brief 샌드박스에 저장되는 객체를 운영하기 위한 클래스
 */
struct lua__G: public luaObjectHelper {
    static const char *TAG;

    lua__G();
    virtual ~lua__G();

    virtual int __index(lua_State *L, void *u);
    virtual int __newindex(lua_State *L, void *u);
    virtual int __call(lua_State *L, void *u);

    virtual int __pairs(lua_State *L, void *u);
    virtual int __tostring(lua_State *L, void *u);

    virtual void Dispose();

    /*!
     * \brief lua_State에 연결되는 샌드박스를 운영하는 클래스
     *
     * \details
     *    apply(L)->Ref(); 호출을 통해 lua_State에 샌드박스 설치
     */
    struct Proxy: public luaSandboxHelper {
        int fetch(lua_State *L, const char *k, luaObjectHelper::Bson *o);

        /* */
        Proxy(lua__G *sandbox);
        virtual ~Proxy();

        virtual int __index(lua_State *L, void *u);
        virtual int __newindex(lua_State *L, void *u);

        /* luaSandboxHelper::__property() */
        virtual int __property(lua_State *L, const char *k, int v);
        virtual int __next(lua_State *L, int step, void *u);

        virtual void Dispose();
    private:
        lua__G *sandbox_;
    };

    static lua__G _G;
protected:
    static int luaB_next(lua_State *L);

    luaSandboxHelper::RuleSet rules;
    BsonSet objects, sandbox; /* _G, _P */
    RWLOCK_T rwlock;
};

/*!
 * \brief lua_State를 관리한다.
 */
struct lua__Stage: public lua__G::Proxy
{
    lua__Stage(lua_State *L, lua__G *sandbox = &lua__G::_G);
    virtual ~lua__Stage();

    virtual lua__Stage *apply();
    /**/
    lua_State *L;
    struct GC: public luaSandboxHelper::GC {
        GC(lua__Stage *context, lua_State *L);
        virtual ~GC();
    };

    virtual lua_State *operator *() const { return this->L; }
private:
    bool dispose_;
};
#endif