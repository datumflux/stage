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
#include "lhelper.hpp"
#include "odbc.h"

#include "c++/string_format.hpp"
#include <codecvt>
#include <locale>
#include <cmath>
#include "log4cxx.h"

/* */
#define ROWS_SIGN				"odbc::ROWS"
#define RESULTSET_SIGN			"odbc::ResultSet"

DECLARE_LOGGER("lua");

#define _HELPER_TRACE()	        _TRACE("[%u.%p] - %s [%s:%d]", pthread_self(), this, this->SIGN.c_str(), __FUNCTION__, __LINE__)
#define _OBJECT_TRACE()	        _TRACE("[%u.%p] - %s:%d", pthread_self(), this, __FUNCTION__, __LINE__)
/*
	var adapter = odbc.open({});

	adapter.begin();

	var rs = adapter.execute("SELECT * FROM test WHERE id = ? AND seq = ?", { "test", 10 })
	if rs.status == 0 then
		var r = rs.fetch("userid/seq")

		for var k, v in pairs(rs) do
		end

		rs.fetch("seq/$");
		rs.apply(adapter, function (changelog) {
		});
	end

	adapter.commit();
*/

#include <c++/tls.hpp>

#if 0 /* */
#include <iostream>
#include <sstream>

static std::wstring widen(const char *s, size_t l)
{
	std::wostringstream w;

	w.imbue(std::locale("en_US.UTF-8"));
    {
        const std::ctype<wchar_t>& ctfacet = std::use_facet< std::ctype<wchar_t> >(w.getloc());
        for (size_t i = 0 ; i < l; ++i)
            w << ctfacet.widen(s[i]);
    }
	return w.str() ;
}

static std::string narrow(const wchar_t *w, size_t l)
{
	std::ostringstream s;

	s.imbue(std::locale("en_US"));
    {
        const std::ctype<char>& ctfacet = std::use_facet< std::ctype<char> >(s.getloc()) ;
        for (size_t i = 0 ; i < l; ++i)
            s << ctfacet.narrow(w[i], 0);
    }
	return s.str() ;
}
#endif

namespace lua__odbc {
/*
 * ODBC
 */
struct TLS__ODBC {
    struct HDBC: public std::deque<SQLHDBC> {
        HDBC(SQLINTEGER timeOut)
            : std::deque<SQLHDBC>()
            , executeTimeout(timeOut) { }
        SQLINTEGER executeTimeout;
    }; typedef std::unordered_map<std::string, HDBC *> HDBC_Set;

    HDBC_Set entry;
    ~TLS__ODBC() { }
}; ThreadLocalStorage<TLS__ODBC> TLS_ODBC;

static int TLS__FreeODBC(SQLHDBC *dbc)
{
    if ((*dbc) == SQL_NULL_HDBC) return -1;
    else if (SQLIsConnectDead((*dbc)) == SQL_ERROR)
    {
        TLS__ODBC::HDBC_Set::iterator it = TLS_ODBC->entry.find((const char *)SQLConnectString(*dbc));
        if (it != TLS_ODBC->entry.end())
        {
            TLS__ODBC::HDBC *C = it->second;

            SQLCommit(dbc, SQL_NTS);
            C->push_back((*dbc));

            (*dbc) = SQL_NULL_HDBC;
            return 0;
        }
    }

    if (SQL_SUCCEEDED(SQLDisconnectEx((*dbc))) == SQL_FALSE)
        return -1;
    else {
        (*dbc) = SQL_NULL_HDBC;
    }
    return 0;
}

static int TLS__FetchODBC(const char *id, SQLHDBC *dbc, SQL_CUSTOM_SPEC **spec)
{
    if (id == NULL) return TLS__FreeODBC(dbc);
    {
        TLS__ODBC::HDBC_Set::iterator it = TLS_ODBC->entry.find(id);

        (*dbc) = SQL_NULL_HDBC;
        if (it != TLS_ODBC->entry.end())
            for (TLS__ODBC::HDBC *C = it->second; C->empty() == false;)
            {
                SQLHDBC c_dbc = C->front();

                if (SQLIsConnectDead(c_dbc) != SQL_ERROR)
                    SQLDisconnectEx(c_dbc);
                else {
                    (*dbc) = c_dbc;
                    C->pop_front();

                    return C->executeTimeout;
                }
                C->pop_front();
            }

#define CONNECTION_TIMEOUT			3
#define EXECUTE_TIMEOUT				1
        if (!((*dbc) = SQLConnectEx((SQLCHAR *)id, CONNECTION_TIMEOUT, SQL_NTS)))
            ;
        else {
            if (it != TLS_ODBC->entry.end())
                it->second->executeTimeout = EXECUTE_TIMEOUT;
            else {
                it = TLS_ODBC->entry.insert(
                        TLS__ODBC::HDBC_Set::value_type(id, new TLS__ODBC::HDBC(EXECUTE_TIMEOUT))
                ).first;
            }
            return EXECUTE_TIMEOUT;
        }
#undef EXECUTE_TIMEOUT
#undef CONNECTION_TIMEOUT
    }
    return -1;
}

struct Adapter : public luaObjectHelper {
    Adapter()
        : luaObjectHelper("odbc::Adapter")
        , dbc(SQL_NULL_HDBC)
        , spec(&SQL_MySQL) {
        this->executeTimeout = 100;
    }
    virtual ~Adapter();

    virtual int __index(lua_State *L, void *u)
    {
        const std::string method = luaL_checkstring(L, 2);

        if (method == "execute")       return link(L, (luaEventClosure_t)__execute, u);
        else if (method == "apply")    return link(L, (luaEventClosure_t)__apply, u);
        else if (method == "begin")    return link(L, (luaEventClosure_t)__begin, u);
        else if (method == "commit")   return link(L, (luaEventClosure_t)__commit, u);
        else if (method == "rollback") return link(L, (luaEventClosure_t)__rollback, u);
        else if (method == "close")    return link(L, (luaEventClosure_t)__close, u);
        else if (method == "executeTimeout") lua_pushinteger(L, this->executeTimeout);
        return 1;
    }

    virtual int __newindex(lua_State *L, void *u)
    {
        std::string method = luaL_checkstring(L, 2);
        if (method == "executeTimeout")
            this->executeTimeout = (SQLINTEGER)lua_tointeger(L, 3);
        return 1;
    }

    virtual int __tostring(lua_State *L, void *u) {
        lua_pushfstring(L, "odbc: %p [adapter '%s']", this, (char *)SQLConnectString(this->dbc));
        return 1;
    }

    virtual void Dispose() { delete this; }

    SQLHDBC          dbc;
    SQLINTEGER       executeTimeout;
    SQL_CUSTOM_SPEC *spec;

    /* struct {
        int (*f)(const char *id, void **dbc, void *u);
        void *udata;
    } link_; */

    /* int bind(const char *id) { return link_.f(id, (void **)&this->dbc_, link_.udata); } */
    int bind(const char *id, SQL_CUSTOM_SPEC **spec = NULL) { return TLS__FetchODBC(id, &this->dbc, spec); }
protected:

    static int __execute(lua_State *L, Adapter *adp, void *u);
    static int __apply(lua_State *L, Adapter *adp, void *u);

    static int __begin(lua_State *L, Adapter *adp, void *u);
    static int __commit(lua_State *L, Adapter *adp, void *u);
    static int __rollback(lua_State *L, Adapter *adp, void *u);
    static int __close(lua_State *L, Adapter *adp, void *u);

};


class UpdateBLOB : public SQL_BINARY_CALLBACK {
public:
    UpdateBLOB(const char *buffer, size_t length)
            : buffer_(buffer)
            , length_(length) { }

    virtual ~UpdateBLOB() { if (buffer_ != NULL) free((void *)buffer_); }

    virtual SQLINTEGER NEED_DATA(SQL_PARAM *pParam, SQLINTEGER iOffset, SQLPOINTER pExternalBuffer, SQLPOINTER *pValue)
    {
        (*pValue) = (SQLPOINTER)(buffer_ + iOffset);
        return (SQLINTEGER)(length_ - iOffset);
    }

    void SetData(const char *buffer, size_t length)
    {
        if (buffer_ != NULL) free((void *)buffer_);
        buffer_ = buffer;
        length_ = length;
    }
private:
    const char *buffer_;
    size_t      length_;
};


static void SQLParamsFree(SQL_PARAMS &params)
{
    for (SQL_PARAMS::iterator it = params.begin(); it != params.end(); it++)
        switch (it->fCType)
        {
            case SQL_C_BINARY: delete (UpdateBLOB *)it->rgbValue; break;
            default          : if (it->rgbValue != NULL) delete[] (char *)it->rgbValue;
        }
}

/*
 *
 */
struct ResultSet : public luaObjectHelper {
    SQLHSTMT    stmt;
    SQL_PARAMS  params;
    SQLRETURN   rc;

    std::string query;
    Adapter    *adp;

    struct KEY_FIELD {
        SQL_DATA v;
        struct {
            SQL_FIELD pInfo;
            SQLCHAR  *lpszName;
        } I;
    }; typedef std::vector<KEY_FIELD *> SQL_KEY_FIELDS;

    static SQL_DATA toSQLData(lua_State *L, int idx, SQL_DATA v)
    {
        switch (lua_type(L, idx))
        {
            case LUA_TNIL: return NULL;

            case LUA_TBOOLEAN: switch (v->wCType)
            {
                case SQL_C_BIT: *SQL_DATA_PTR<int8_t>(v) = lua_toboolean(L, idx); break;
                default: return NULL;
            } break;
            case LUA_TNUMBER: switch (v->wCType)
            {
                case SQL_C_BIT:
                case SQL_C_STINYINT: *SQL_DATA_PTR<int8_t>(v) = (int8_t)lua_tointeger(L, idx); break;
                case SQL_C_SSHORT: *SQL_DATA_PTR<int16_t>(v) = (int16_t)lua_tointeger(L, idx); break;
                case SQL_C_SLONG: *SQL_DATA_PTR<int32_t>(v) = (int32_t)lua_tointeger(L, idx); break;
                case SQL_C_SBIGINT: *SQL_DATA_PTR<int64_t>(v) = (int64_t)lua_tointeger(L, idx); break;

                case SQL_C_UTINYINT: *SQL_DATA_PTR<uint8_t>(v) = (uint8_t)lua_tointeger(L, idx); break;
                case SQL_C_USHORT: *SQL_DATA_PTR<uint16_t>(v) = (uint16_t)lua_tointeger(L, idx); break;
                case SQL_C_ULONG: *SQL_DATA_PTR<uint32_t>(v) = (uint32_t)lua_tointeger(L, idx); break;
                case SQL_C_UBIGINT: *SQL_DATA_PTR<uint64_t>(v) = (uint64_t)lua_tointeger(L, idx); break;

                case SQL_C_FLOAT: *SQL_DATA_PTR<float>(v) = (float)lua_tonumber(L, idx); break;
                case SQL_C_DOUBLE: *SQL_DATA_PTR<double>(v) = lua_tonumber(L, idx); break;

                case SQL_C_TYPE_DATE: *SQL_DATA_PTR<SQL_DATE_STRUCT>(v) = SQLToDATE(lua_tointeger(L, idx)); break;
                case SQL_C_TYPE_TIME: *SQL_DATA_PTR<SQL_TIME_STRUCT>(v) = SQLToTIME(lua_tointeger(L, idx)); break;
                case SQL_C_TYPE_TIMESTAMP: *SQL_DATA_PTR<SQL_TIMESTAMP_STRUCT>(v) = SQLToTIMESTAMP(lua_tointeger(L, idx)); break;

                default: return NULL;
            } break;
            case LUA_TTABLE: switch (v->wCType)
            {
                case SQL_C_BINARY:
                {
                    SQL_DATA r = NULL;
                    bson b;

                    bson_init(&b);
                    if (luaL_toBSON(L, idx, "", &b, NULL) < 0)
                        ;
                    else {
                        bson_finish(&b);
                        if (v->dwColumnLength > bson_size(&b))
                            (r = v)->StrLen_or_Ind = bson_size(&b);
                        else {
                            r = SQL_DATA_New(v->wCType, bson_size(&b));
                            SQL_DATA_Free(v);
                        }
                        memcpy(SQL_DATA_PTR<char>(r),  bson_data(&b), r->StrLen_or_Ind);
                    } bson_destroy(&b);
                    return r;
                }
                default: return NULL;
            } break;
            case LUA_TSTRING:
            {
                size_t len; const char *s = lua_tolstring(L, idx, &len);

                switch (v->wCType)
                {
                    case SQL_C_BINARY:
                    case SQL_C_CHAR:
                    {
                        SQL_DATA r;

                        if (v->dwColumnLength > (SQLLEN)len)
                            (r = v)->StrLen_or_Ind = len;
                        else {
                            r = SQL_DATA_New(v->wCType, len);
                            SQL_DATA_Free(v);
                        }

                        {
                            char *b = SQL_DATA_PTR<char>(v);

                            if (len > 0)
                                memcpy(b, s, v->StrLen_or_Ind);
                            *(b + v->StrLen_or_Ind) = 0;
                        }
                        return r;
                    }

                    case SQL_C_WCHAR:
                    {
#if 1
                        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
                        std::wstring w = utf8_conv.from_bytes(s, s + len);
#else
						std::wstring w = widen(s, len);
#endif
                        size_t length = w.length() * sizeof(wchar_t);

                        SQL_DATA r;
                        if (v->dwColumnLength > (SQLLEN)length)
                            (r = v)->StrLen_or_Ind = length;
                        else {
                            r = SQL_DATA_New(v->wCType, length);
                            SQL_DATA_Free(v);
                        }

                        {
                            uint8_t *b = SQL_DATA_PTR<uint8_t>(v);

                            if (v->StrLen_or_Ind > 0)
                                memcpy(b, w.c_str(), v->StrLen_or_Ind);
                            *(b + v->StrLen_or_Ind) = 0;
                        }
                        return r;
                    }

                    default:
                    {
                        if (len > 0)
                            switch (v->wCType)
                            {
                                case SQL_C_STINYINT: *SQL_DATA_PTR<int8_t>(v) = (int8_t)strtol(s, NULL, 10); break;
                                case SQL_C_SSHORT: *SQL_DATA_PTR<int16_t>(v) = (int16_t)strtol(s, NULL, 10); break;
                                case SQL_C_SLONG: *SQL_DATA_PTR<int32_t>(v) = strtol(s, NULL, 10); break;
                                case SQL_C_SBIGINT: *SQL_DATA_PTR<int64_t>(v) = strtoll(s, NULL, 10); break;

                                case SQL_C_UTINYINT: *SQL_DATA_PTR<uint8_t>(v) = (uint8_t)strtoul(s, NULL, 10); break;
                                case SQL_C_USHORT: *SQL_DATA_PTR<uint16_t>(v) = (uint16_t)strtoul(s, NULL, 10); break;
                                case SQL_C_ULONG: *SQL_DATA_PTR<uint32_t>(v) = strtoul(s, NULL, 10); break;
                                case SQL_C_UBIGINT: *SQL_DATA_PTR<uint64_t>(v) = strtoull(s, NULL, 10); break;

                                case SQL_C_BIT: *SQL_DATA_PTR<int8_t>(v) = (strchr("TtYy1", *(s + 0)) != NULL); break;

                                case SQL_C_FLOAT: *SQL_DATA_PTR<float>(v) = strtof(s, NULL); break;
                                case SQL_C_DOUBLE: *SQL_DATA_PTR<double>(v) = strtod(s, NULL); break;

                                case SQL_C_TYPE_TIMESTAMP:
                                {
                                    struct tm utc;
                                    long msec = 0;

                                    if (strptime(s, "%Y-%m-%d %T", &utc) != NULL)
                                        ;
                                    else {
                                        char *x = (char *)strptime(s, "%Y-%m-%dT%T", &utc);
                                        if (x == NULL) throw __LINE__;
                                        else if (*x == '.')
                                        {
                                            char *e;

                                            msec = strtol(x + 1, &e, 10);
                                            if ((msec == LONG_MAX) || (msec == LONG_MIN) || (*(e + 0) != 'Z'))
                                                throw __LINE__;
                                        }
                                        else if (*x != 'Z') throw __LINE__;
                                    }

                                    *SQL_DATA_PTR<SQL_TIMESTAMP_STRUCT>(v) = SQLToTIMESTAMP(mktime(&utc));
                                } break;

                                case SQL_C_TYPE_DATE:
                                {
                                    struct tm utc;

                                    if (strptime(s, "%Y-%m-%d", &utc) == NULL) throw __LINE__;
                                    *SQL_DATA_PTR<SQL_DATE_STRUCT>(v) = SQLToDATE(mktime(&utc));
                                } break;

                                case SQL_C_TYPE_TIME:
                                {
                                    struct tm utc;

                                    if (strptime(s, "%T", &utc) == NULL) throw __LINE__;
                                    *SQL_DATA_PTR<SQL_TIME_STRUCT>(v) = SQLToTIME(mktime(&utc));
                                } break;

                                default: return NULL;
                            }
                        else v->StrLen_or_Ind = 0;
                    } break;
                }
            } break;
        }
        return v;
    }

    static int toSQLData(SQL_DATA v, lua_State *L)
    {
        if ((v != NULL) && (v->StrLen_or_Ind > 0))
            switch (v->wCType)
            {
                case SQL_C_BIT: lua_pushboolean(L, SQL_DATA_VALUE<int8_t>(v)); break;

                case SQL_C_STINYINT: lua_pushinteger(L, SQL_DATA_VALUE<int8_t>(v)); break;
                case SQL_C_SSHORT: lua_pushinteger(L, SQL_DATA_VALUE<int16_t>(v)); break;
                case SQL_C_SLONG: lua_pushinteger(L, SQL_DATA_VALUE<int32_t>(v)); break;
                case SQL_C_SBIGINT: lua_pushinteger(L, SQL_DATA_VALUE<int64_t>(v)); break;

                case SQL_C_UTINYINT: lua_pushinteger(L, SQL_DATA_VALUE<uint8_t>(v)); break;
                case SQL_C_USHORT: lua_pushinteger(L, SQL_DATA_VALUE<uint16_t>(v)); break;
                case SQL_C_ULONG: lua_pushinteger(L, SQL_DATA_VALUE<uint32_t>(v)); break;
                case SQL_C_UBIGINT: lua_pushinteger(L, SQL_DATA_VALUE<uint64_t>(v)); break;

                case SQL_C_FLOAT: lua_pushnumber(L, SQL_DATA_VALUE<float>(v)); break;
                case SQL_C_DOUBLE: lua_pushnumber(L, SQL_DATA_VALUE<double>(v)); break;

                case SQL_C_BINARY: if (IsBSON(SQL_DATA_PTR<char>(v), (int)v->StrLen_or_Ind))
                {
                    bson_iterator b_it; bson_iterator_from_buffer(&b_it, SQL_DATA_PTR<char>(v));
					for (lua_newtable(L); bson_iterator_next(&b_it) != BSON_EOO; )
					{
						lua_pushstring(L, bson_iterator_key(&b_it));
						if (luaL_pushBSON(L, &b_it) > 0)
							lua_rawset(L, -3);
						else lua_pop(L, 1);
					}
                    break;
                }
                case SQL_C_CHAR: lua_pushlstring(L, SQL_DATA_PTR<char>(v), std::max(v->StrLen_or_Ind, (SQLLEN)0)); break;
                case SQL_C_WCHAR:
                {
#if 1
                    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
                    std::string s;
                    {
                        wchar_t *w = SQL_DATA_PTR<wchar_t>(v);
                        s = utf8_conv.to_bytes(w, w + (v->StrLen_or_Ind / sizeof(wchar_t)));
                    }
#else
					std::string s = narrow(
							SQL_DATA_PTR<wchar_t>(v), v->StrLen_or_Ind / sizeof(wchar_t));
#endif

                    lua_pushlstring(L, s.c_str(), s.length());
                } break;

                case SQL_C_TYPE_DATE:
                case SQL_C_TYPE_TIME:
                case SQL_C_TYPE_TIMESTAMP: lua_pushinteger(L, SQL_DATA_VALUE<time_t>(v)); break;
                default: return 0;
            }
        else lua_pushnil(L);
        return 1;
    }

    struct ROWS : public luaObjectHelper
    {
        struct WRAP: public luaObjectRef {
            SQLSMALLINT wIndex;
            SQL_ROWS    rows;

            WRAP(ROWS *o, SQLSMALLINT i, SQL_ROWS r)
                    : luaObjectRef(RESULTSET_SIGN "::ROWS::WRAP")
                    , wIndex(i)
                    , rows(r)
                    , o_(o) { o->Ref(); }
            virtual ~WRAP() { o_->Unref(); }

            virtual void Dispose() { delete this; }
        private:
            ROWS *o_;
        };

        virtual void Dispose() { delete this; }
        virtual int  __gc(lua_State *L, void *u)
        {
            if (u != NULL)
                ((WRAP *)u)->Unref();
            return this->luaObjectHelper::__gc(L, NULL);
        }

        virtual int __tostring(lua_State *L, void *u)
        {
            WRAP *w = (WRAP *)u;
            KEY_FIELD *kf = this->fields[w->wIndex];

            if (kf->I.lpszName == NULL)
                lua_pushfstring(L, "odbc: %p [rs.%d]", this, w->wIndex);
            else {
                lua_pushfstring(L, "odbc: %p [row.%d '%s']", this, w->wIndex, kf->I.lpszName);
            }
            return 1;
        }

        /* */
        static int __next(lua_State *L)
        {
            WRAP *w; ROWS *_this = (ROWS *)luaObjectHelper::unwrap(L, -2, (void **)&w);
            KEY_FIELD *kf = _this->fields[w->wIndex];

            SQL_DATA k = SQLStepKey(w->rows, toSQLData(L, -1, kf->v));
            SQL_ROWS v = (k == NULL) ? NULL: SQLStepRows(w->rows, k);

            toSQLData(k, L);
            if (v == NULL) lua_pushnil(L);
            else if (v->wIsType == (SQL_TRUE + 1)) toSQLData(v->u.v, L);
            else luaL_pushhelper(L, _this, (new WRAP(_this, w->wIndex + 1, v))->Ref());

            return 2;
        }

        virtual int __pairs(lua_State *L, void *u)
        {
            lua_pushcclosure(L, __next, 0);
            lua_pushvalue(L, 1);
            lua_pushnil(L);
            return 3;
        }

        virtual int __len(lua_State *L, void *u)
        {
            lua_pushinteger(L, ((WRAP *)u)->rows->u.i[0]->size());
            return 1;
        }

        /* row({ v, v, ... }) */
        virtual int __call(lua_State *L, void *u)
        {
	        WRAP *w = (WRAP *)u;

            auto luaL_pushROW = [this, w](lua_State *L, int k_idx) {
	            KEY_FIELD *kf = this->fields[w->wIndex];

	            if (toSQLData(L, k_idx, kf->v) == NULL)
		            return luaL_error(L, "%d: The data type does not match. [%d]", __LINE__, lua_type(L, k_idx));
	            else {
		            SQL_ROWS rows = SQLStepRows(w->rows, kf->v, NULL);
		            if (rows == NULL) lua_pushnil(L);
		            else if (rows->wIsType == (SQL_TRUE + 1)) toSQLData(rows->u.v, L);
		            else luaL_pushhelper(L, this, (new WRAP(this, w->wIndex + 1, rows))->Ref());
	            } return 1;
            };

            switch (lua_type(L, 2))
            {
                case LUA_TNONE: lua_newtable(L);
                {
	                int x__i = 0;
	                for (SQLRowValueSet::iterator
			                     it = w->rows->u.i[0]->begin(); it != w->rows->u.i[0]->end(); ++it)
	                {
		                toSQLData(it->first, L);
		                lua_rawseti(L, -2, ++x__i);
	                }
                } break;

                case LUA_TTABLE: lua_newtable(L);
                {
                    for (lua_pushnil(L); lua_next(L, 2); )
	                {
		                {
			                luaL_pushROW(L, -1);    /* v */
		                } lua_rawset(L, -4);
	                }
                } break;
                default: luaL_pushROW(L, 2);
            }

	        if (lua_type(L, -2) == LUA_TFUNCTION)
	        {
	        	int f = lua_absindex(L, -2);

		        switch (luaObjectHelper::call(L, f, 1))
		        {
			        case LUA_OK: return lua_gettop(L) - f;
			        default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		        }
	        }
	        return 1;
        }

        /* v = rs[k] */
        virtual int __index(lua_State *L, void *u)
        {
            WRAP *w = (WRAP *)u;
            KEY_FIELD *kf = this->fields[w->wIndex];

            if (toSQLData(L, -1, kf->v) == NULL)
                return luaL_error(L, "%d: The data type does not match. [%d]", __LINE__, lua_type(L, -1));
            else {
                SQLSMALLINT wIsNew = 0;

                SQL_ROWS rows = SQLStepRows(w->rows, kf->v, (kf->I.lpszName ? &wIsNew: NULL));
                if (rows == NULL) lua_pushnil(L);
                else if (rows->wIsType == (SQL_TRUE + 1)) toSQLData(rows->u.v, L);
                else luaL_pushhelper(L, this, (new WRAP(this, w->wIndex + 1, rows))->Ref());
            }
            return 1;
        }

        /* rs[k] = v */
        virtual int __newindex(lua_State *L, void *u)
        {
            WRAP *w = (WRAP *)u;
            KEY_FIELD *kf = this->fields[w->wIndex];

            if (toSQLData(L, -2, kf->v) == NULL)
                return luaL_error(L, "%d: The data type does not match. [%d]", __LINE__, lua_type(L, -2));
            else if (lua_type(L, -1) == LUA_TNIL) /* */
            {
                if (SQLUpdateRow(w->rows, kf->v, NULL) < SQL_SUCCESS)
                    return luaL_error(L, "%d: An error has occurred.", __LINE__);
            }
            else if (kf->I.lpszName != NULL)
            {
                if (lua_istable(L, -1) == false)
                    return luaL_error(L, "%d: Only changes to the column are allowed.", __LINE__);
                else {
                    SQLSMALLINT wIsNew = 0;

                    SQL_ROWS rows = SQLStepRows(w->rows, kf->v, &wIsNew);
                    if ((rows == NULL) || (rows->wIsType == (SQL_TRUE + 1)))
                        return luaL_error(L, "%d: The key type does not match.", __LINE__);
                    else {
                        WRAP __w(this, w->wIndex + 1, rows);

                        lua_pushnil(L);
                        for (; lua_next(L, -2); lua_pop(L, 1))
                        {
                            if (__newindex(L, &__w) == 0)
                                break;
                        }
                    }
                }
            }
            else {
                SQL_FIELD v_f = SQLFieldFromResult(this->result, (SQLCHAR *)SQL_DATA_PTR<char>(kf->v));
                if (v_f == NULL) return luaL_error(L, "%d: The column does not exist.", __LINE__);
                else if (v_f->wSQLType == SQL_NTS) return luaL_error(L, "%d: The data type is invalid.", __LINE__);
                else {
#define MAX_LENGTH              1024
                    SQL_DATA v = SQL_DATA_New(v_f->wCType, std::min(v_f->dwColumnLength, (SQLLEN)MAX_LENGTH));

                    SQL_DATA r = toSQLData(L, -1, v);
                    if (r == NULL)
                        SQL_DATA_Free(v);
                    else {
                        if (SQLUpdateRow(w->rows, kf->v, r) < SQL_SUCCESS)
                            return luaL_error(L, "%d: An error has occurred.", __LINE__);
                        return 1;
                    }
#undef MAX_LENGTH
                    return luaL_error(L, "%d: This type is not supported. [%d]", __LINE__, lua_type(L, -1));
                }
            }
            return 1;
        }

        SQL_KEY_FIELDS fields;
        SQL_RESULT     result;

        virtual int transfer(lua_State *L, int op, void *x__u)
        {
            if (x__u != NULL)
                switch (op)
                {
                    default: ((WRAP *)x__u)->Unref(); break;
                    case 1: case 0: ((WRAP *)x__u)->Ref(); break;
                }

            switch (op)
            {
                case 1: luaL_pushhelper(L, this, x__u); break;
                case 0: this->Ref(); break;
                default: this->Unref();
            } return 0;
        }

        ROWS(): luaObjectHelper(ROWS_SIGN, true), result(NULL) { }
        virtual ~ROWS()
        {
            for (SQL_KEY_FIELDS::iterator k_it = fields.begin(); k_it != fields.end(); k_it++)
            {
                KEY_FIELD *kf = (*k_it);

                SQL_DATA_Free(kf->v);
                delete kf;
            }
            SQLFreeResult(result);
        }
    };

    virtual void Dispose() { delete this; }

    virtual int __tostring(lua_State *L, void *u)
    {
        lua_pushfstring(L, "odbc: %p [rs '%s']", this->adp, this->query.c_str());
        return 1;
    }

    virtual int __call(lua_State *L, void *u) { return 0; }

    /* */
    virtual int __index(lua_State *L, void *u)
    {
	    /*
			while (rs.more == true) do
				var r = rs.fetch({ "userid", $" }, function (row)
				end);

				// result
				rs.erase(row, {"XXXX", 1});
			end
			rs._1
		 */
_gW:    switch (lua_type(L, -1))
	    {
	    	case LUA_TNUMBER:
		    {
		    	const int __i = lua_tointeger(L, -1) - 1;

			    if (__i < 0) lua_pushinteger(L, this->rc);
			    else if (__i > (int)this->params.size())
			    	return luaL_error(L, "%d: The parameter index is not valid.", __LINE__);
			    else if (this->rc != SQL_NO_DATA) return luaL_error(L, "%d: The result is not valid.", __LINE__);
			    else {
				    SQL_PARAM &P = this->params.at(__i);

				    switch (P.fCType)
				    {
					    case SQL_C_WCHAR:
					    {
#if 1
						    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
						    std::string s;
						    {
							    wchar_t *w = (wchar_t *)P.rgbValue;
							    s = utf8_conv.to_bytes(w, w + (P.StrLen_or_Ind / sizeof(wchar_t)));
						    }
#else
						    std::string s = narrow((wchar_t *)P.rgbValue, P.StrLen_or_Ind / sizeof(wchar_t));
#endif
						    lua_pushlstring(L, s.c_str(), s.length());
					    } break;

					    case SQL_C_CHAR:
					    {
						    lua_pushlstring(L, (char *)P.rgbValue, (int)P.StrLen_or_Ind);
					    } break;
					    case SQL_C_BIT: lua_pushboolean(L, (int)*((int8_t *)P.rgbValue)); break;

#define CASE_PARAM_INTEGER(C_TYPE, P_TYPE)  case SQL_C_##C_TYPE: lua_pushinteger(L, (lua_Integer)*((P_TYPE *)P.rgbValue)); break
					    CASE_PARAM_INTEGER(STINYINT, int8_t);
					    CASE_PARAM_INTEGER(SSHORT, int16_t);
					    CASE_PARAM_INTEGER(SLONG, int32_t);
					    CASE_PARAM_INTEGER(SBIGINT, int64_t);

					    CASE_PARAM_INTEGER(UTINYINT, uint8_t);
					    CASE_PARAM_INTEGER(USHORT, uint16_t);
					    CASE_PARAM_INTEGER(ULONG, uint32_t);
					    CASE_PARAM_INTEGER(UBIGINT, int64_t);
#undef CASE_PARAM_INTEGER

#define CASE_PARAM_NUMBER(C_TYPE, P_TYPE)   case SQL_C_##C_TYPE: lua_pushnumber(L, (lua_Number)*((P_TYPE *)P.rgbValue)); break
					    CASE_PARAM_NUMBER(FLOAT, float);
					    CASE_PARAM_NUMBER(DOUBLE, double);
#undef CASE_PARAM_NUMBER
					    case SQL_C_TYPE_TIMESTAMP:
					    {
						    lua_pushinteger(L, (lua_Integer)SQLFromTIMESTAMP((SQL_TIMESTAMP_STRUCT *)P.rgbValue));
					    } break;
				    }
			    }
		    } break;

	    	case LUA_TSTRING:
		    {
			    const std::string method = luaL_checkstring(L, -1);

			    if (method == "fetch") return link(L, (luaEventClosure_t)__fetch, u);
			    else if (method == "columns") return link(L, (luaEventClosure_t)__columns, u);
			    else if (method == "erase") return link(L, (luaEventClosure_t)__erase, u);
			    else if (method == "near") return link(L, (luaEventClosure_t)__near, u);
			    else if (method == "apply") return link(L, (luaEventClosure_t)__apply, u);
			    else if (method == "more") lua_pushboolean(L, SQL_SUCCEEDED(this->rc));
			    else if (*(method.c_str() + 0) == '_')
			    {
				    lua_pushinteger(L, strtol(method.c_str() + 1, NULL, 10));
				    goto _gW;
			    }
		    } break;
        }
	    return 1;
    }

    /* */
    ResultSet(Adapter *adp, SQLHSTMT stmt)
            : luaObjectHelper(RESULTSET_SIGN)
            , stmt(stmt)
            , adp(adp) {
        adp->Ref();
        this->rc = SQL_SUCCESS;
    }

    virtual ~ResultSet() {
        SQLParamsFree(params);
        SQLFreeStmtEx(stmt);
        adp->Unref();
    }

    /* */
    static int apply(lua_State *L, SQLHSTMT stmt, ROWS *R, SQLCHAR *lpszSchemeTable)
    {
        CHANGELOG log;

        SQLResetReport();
        bson_append_start_array(&log.changelog, ""); {
            if (SQLCommitResult(stmt, R->result, lpszSchemeTable, &log) != SQL_SUCCESS)
                return luaL_error(L, "%d: An error occurred during apply.", __LINE__);
        } bson_append_finish_array(&log.changelog);

        {
            SQLLEN lRowCount = 0;
            SQL_DATA pGeneratedKeys = SQLGetGeneratedKeys(R->result, &lRowCount);

            lua_newtable(L);
            if (pGeneratedKeys != NULL)
            {
                int __i = 0;

                do {
                    switch (pGeneratedKeys->wCType)
                    {
#define FETCH_GENERATED_KEYS(v, DT, T)					\
	case SQL_C_S ## DT : lua_pushinteger(L, (lua_Integer)*SQL_DATA_PTR<T>(v)); break;	\
	case SQL_C_U ## DT : lua_pushinteger(L, (lua_Integer)*SQL_DATA_PTR<u ## T>(v)); break;

                        FETCH_GENERATED_KEYS(pGeneratedKeys, TINYINT, int8_t)
                        FETCH_GENERATED_KEYS(pGeneratedKeys, SHORT, int16_t)
                        FETCH_GENERATED_KEYS(pGeneratedKeys, LONG, int32_t)
                        FETCH_GENERATED_KEYS(pGeneratedKeys, BIGINT, int64_t)
#undef FETCH_GENERATED_KEYS
                    }

                    lua_rawseti(L, -2, ++__i);
                    if ((--lRowCount) <= 0)
                        break;
                    else {
                        pGeneratedKeys = SQL_DATA_NEXT(pGeneratedKeys);
                    }
                } while (SQL_TRUE);
            }
        }

        bson_finish(&log.changelog);
        luaL_pushBSON(L, 0, &log.changelog);
        return 0;
    }

protected:
    static bool IsBSON(const char *buffer, int size)
    {
        if (size <= (int)sizeof(int32_t))
            return false;
        else {
            int i;
            bson_little_endian32(&i, buffer);
            return (i == size);
        }
    }

    /* */
    struct CHANGELOG : public SQLCommitCALLBACK {
        /* */
        virtual void ChangeLog(SQLSMALLINT wChangeType, SQLCHAR *szTableName, WhereSet &where, ChangeSet &changes)
        {
            bson_append_start_object(&changelog, std::format("#%d", ++this->apply).c_str());
            {
                bson_append_string(&changelog, "table", (char *)szTableName);
                bson_append_int(&changelog, "action", wChangeType);

                bson_append_start_object(&changelog, "where"); {
                    for (WhereSet::iterator w_it = where.begin(); w_it != where.end(); w_it++)
                        DATA_toBSON(w_it->second, (const char *)w_it->first, &changelog);
                } bson_append_finish_object(&changelog);

                bson_append_start_object(&changelog, "value");
                {
                    for (ChangeSet::iterator i_it = changes.begin(); i_it != changes.end(); i_it++)
                    {
                        bson_append_start_object(&changelog, (const char *)i_it->first);
                        {
                            if (i_it->second.o != NULL) DATA_toBSON(i_it->second.o, "o", &changelog);
                            if (i_it->second.v != NULL)
                            {
                                SQL_DATA v = i_it->second.v;

                                DATA_toBSON(v, "n", &changelog);
                                bson_append_int(&changelog, "t", v->wCType);
                            }
                        }
                        bson_append_finish_object(&changelog);
                    }
                } bson_append_finish_object(&changelog);
            } bson_append_finish_object(&changelog);
        }

        CHANGELOG(): apply(0) { bson_init(&changelog); }
        virtual ~CHANGELOG() { bson_destroy(&changelog); }

        bson       changelog;
        SQLINTEGER apply;
    private:
        static int DATA_toBSON(SQL_DATA v, const char *k, bson *b)
        {
            switch (v->wCType)
            {
                case SQL_C_WCHAR:
                {
#if 1
                    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
                    std::string s;
                    {
                        wchar_t *w = SQL_DATA_PTR<wchar_t>(v);
                        s = utf8_conv.to_bytes(w, w + (v->StrLen_or_Ind / sizeof(wchar_t)));
                    }
#else
					std::string s = narrow(
							SQL_DATA_PTR<wchar_t>(v), v->StrLen_or_Ind / sizeof(wchar_t));
#endif
                    bson_append_string(b, k, s.c_str());
                } break;
                case SQL_C_CHAR: bson_append_string(b, k, SQL_DATA_PTR<char>(v)); break;
#define CASE_ASSIGN_VALUE(ST, CT, OT)	case SQL_C_##ST: {	\
	bson_append_##CT(b, k, (CT)SQL_DATA_VALUE<OT>(v)); \
} break
                case SQL_C_BIT:
                CASE_ASSIGN_VALUE(STINYINT, int, int8_t);
                CASE_ASSIGN_VALUE(SSHORT, int, int16_t);
                CASE_ASSIGN_VALUE(SLONG, int, int32_t);
                CASE_ASSIGN_VALUE(SBIGINT, long, int64_t);

                CASE_ASSIGN_VALUE(UTINYINT, int, uint8_t);
                CASE_ASSIGN_VALUE(USHORT, int, uint16_t);
                CASE_ASSIGN_VALUE(ULONG, long, uint32_t);
                CASE_ASSIGN_VALUE(UBIGINT, long, uint64_t);

                CASE_ASSIGN_VALUE(FLOAT, double, float);
                CASE_ASSIGN_VALUE(DOUBLE, double, double);

                case SQL_C_TYPE_TIME:
                case SQL_C_TYPE_DATE:
                CASE_ASSIGN_VALUE(TYPE_TIMESTAMP, time_t, time_t);
#undef CASE_ASSIGN_VALUE
                case SQL_C_BINARY:
                {
                    if (!IsBSON(SQL_DATA_PTR<char>(v), (int)v->StrLen_or_Ind))
                        return bson_append_binary(b, k, BSON_BIN_BINARY, SQL_DATA_PTR<char>(v), (int)v->StrLen_or_Ind);
                    else {
                        bson s_b;

                        bson_init_finished_data(&s_b, SQL_DATA_PTR<char>(v));
                        return bson_append_bson(b, k, &s_b);
                    }
                } break;

                default: return -1;
            }

            return 0;
        }
    };

    /* */
    static int __columns(lua_State *L, ResultSet *_this, void *udata)
    {
	    SQLResetReport();
	    switch (_this->rc)
	    {
		    default: lua_pushinteger(L, _this->rc); break;
		    case SQL_SUCCESS:
		    {
			    SQL_COLUMNS pColumns = NULL;
			    SQLSMALLINT wColumnCount = SQLResultColumns(_this->stmt, &pColumns, SQL_TRUE);

			    if (wColumnCount == 0)
				    lua_pushnil(L);
			    else {
			    	lua_newtable(L);
				    for (SQLUSMALLINT wColumnIndex = 0; wColumnIndex < wColumnCount; wColumnIndex++)
				    {
					    SQL_COLUMN C = pColumns[wColumnIndex];

					    lua_pushstring(L, (const char *)C->szColumnLabel);
					    lua_newtable(L); {
#define ASSIGN_FIELD(_N, _F, _V)            do {    \
	lua_pushstring(L, _N); lua_push##_F(L, _V); lua_rawset(L, -3);  \
} while (0)
							ASSIGN_FIELD("length", integer, C->dwColumnLength);
							ASSIGN_FIELD("sqltype", integer, C->wSQLType);
							ASSIGN_FIELD("autoincrement", boolean, C->wAutoIncrement);
						    ASSIGN_FIELD("nullable", boolean, C->wNullable);
						    ASSIGN_FIELD("unsigned", boolean, C->wUnsigned);
						    ASSIGN_FIELD("scale", integer, C->wScale);
						    ASSIGN_FIELD("precision", integer, C->dwPrecision);
#undef ASSIGN_FIELD
					    } lua_rawset(L, -3);
				    }
			    } SQLCancelColumns(_this->stmt, pColumns);
		    }

		    if (lua_type(L, -2) == LUA_TFUNCTION)
		    {
			    int f = lua_absindex(L, -2);

			    switch (luaObjectHelper::call(L, f, 1))
			    {
				    case LUA_OK: return lua_gettop(L) - f;
				    default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
			    }
		    }
	    } return 1;
    }

    static int __fetch(lua_State *L, ResultSet *_this, void *udata)
    {
        SQLResetReport();
        switch (_this->rc)
        {
            default: lua_pushinteger(L, _this->rc); break;
            case SQL_SUCCESS:
            {
                SQL_COLUMNS pColumns = NULL;
                SQLSMALLINT wColumnCount = SQLResultColumns(_this->stmt, &pColumns, SQL_TRUE);

                if (wColumnCount == 0)
                    lua_pushnil(L);
                else {
                    std::vector<SQLCHAR *> keys_v;

                    switch (lua_type(L, 1))
                    {
                        case LUA_TTABLE: for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
                        {
                            keys_v.push_back((SQLCHAR *)lua_tostring(L, -1));
                        } break;
                        case LUA_TSTRING: keys_v.push_back((SQLCHAR *)lua_tostring(L, 1));
                    }

                    keys_v.push_back(NULL);
                    {
                        ROWS *R = new ROWS();

                        R->result = SQLAllocResult(pColumns, keys_v.data(), _this->adp->spec);
                        for (SQLSMALLINT wIndex = 0; wIndex < (SQLSMALLINT)keys_v.size(); wIndex++)
                        {
                            KEY_FIELD *kf = new KEY_FIELD();

                            if ((kf->I.pInfo = SQLFieldFromResult(R->result, wIndex, &kf->I.lpszName)) != NULL)
                            {
                                if (kf->I.pInfo->dwColumnLength == 0)
                                {
                                    delete kf;
                                    delete R;

                                    SQLCancelColumns(_this->stmt, pColumns);
                                    return luaL_error(L, "%d: This column can not be used as a key. ['%s']", __LINE__, (char *)keys_v.at(wIndex));
                                }

                                kf->v = SQL_DATA_New(kf->I.pInfo->wCType, kf->I.pInfo->dwColumnLength);
                            }
                            else {
                                delete kf;
                                delete R;

                                SQLCancelColumns(_this->stmt, pColumns);
                                return luaL_error(L, "%d: Column name does not exist. ['%s']", __LINE__, (char *)keys_v.at(wIndex));
                            }
                            R->fields.push_back(kf);
                        }

                        for (SQLROWCOUNT dwRowCount = 0; SQLFetch(_this->stmt) != SQL_NO_DATA_FOUND;)
                            if (SQLReviewColumns(_this->stmt, pColumns) != SQL_SUCCESS)
                                break;
                            else {
                                SQLPumpResult(R->result, ++dwRowCount, pColumns);
                            }

                        luaL_pushhelper(L, R, (new ROWS::WRAP(R, 0, &R->result->ROWS))->Ref());
                    }
                }

                SQLCancelColumns(_this->stmt, pColumns);
            } _this->rc = SQLMoreResults(_this->stmt);

	        if (lua_type(L, -2) == LUA_TFUNCTION)
	        {
		        int f = lua_absindex(L, -2);

		        switch (luaObjectHelper::call(L, f, 1))
		        {
			        case LUA_OK: return lua_gettop(L) - f;
			        default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		        }
	        }
        }
        return 1;
    }

    /* r = .apply(row, [table,] function (changelog)) */
    static int __apply(lua_State *L, ResultSet *_this, void *udata)
    {
        ROWS *R = (ROWS *)luaObjectHelper::unwrap(L, 1, NULL);

        if ((R->SIGN != ROWS_SIGN) || (R->result == NULL))
            return luaL_error(L, "%d: The data was not read.", __LINE__);
        else if (SQLIsDirtyResult(R->result) == SQL_FALSE) return 0;
        else
        {
	        const int f = lua_gettop(L);

	        if (apply(L, _this->stmt, R, (SQLCHAR *) (
			        (lua_type(L, 2) == LUA_TSTRING) ? lua_tostring(L, 2) : NULL
	        )) == 0) switch (lua_type(L, f))
	        {
		        case LUA_TFUNCTION: switch (luaObjectHelper::call(L, f, lua_gettop(L) - f))
		        {
		            case LUA_OK: return lua_gettop(L) - f;
		            default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
		        }
		        default: break;
	        }
	        return 1;
        }
    }

    /* r = .erase(rows, {keys...}) */
    static int __erase(lua_State *L, ResultSet *_this, void *udata)
    {
        ROWS *R = (ROWS *)luaObjectHelper::unwrap(L, 1, &udata);

        if ((R->SIGN != ROWS_SIGN) || (R->result == NULL))
            return luaL_error(L, "%d: The data was not read.", __LINE__);
        else {
            ROWS::WRAP *w = (ROWS::WRAP *)udata;
            KEY_FIELD *kf = R->fields[w->wIndex];

            SQL_ROWS rows = w->rows;
            SQL_DATA k = NULL;

            if (lua_type(L, 2) != LUA_TTABLE)
                k = toSQLData(L, 2, kf->v);
            else {
                SQLSMALLINT wIndex = w->wIndex;

                for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1))
                {
                    if (wIndex >= (SQLSMALLINT)R->fields.size()) goto _gE;
                    else if ((wIndex > 0) && !(rows = SQLStepRows(rows, k)))
_gE:                {
                    	lua_pushinteger(L, 0);
                    	return 1;
                    }
                    else {
                        if (!(k = toSQLData(L, -1, R->fields[wIndex]->v)))
                            return luaL_error(L, "%d: The data type does not match.", __LINE__);
                    }
                    wIndex++;
                }
            }
            lua_pushinteger(L, ((rows != NULL) && (SQLUpdateRow(rows, k, NULL) >= SQL_SUCCESS)) ? 1: 0);
        }
        return 1;
    }

	/* r = .near(rows, {keys...}) */
	static int __near(lua_State *L, ResultSet *_this, void *udata)
	{
		ROWS *R = (ROWS *)luaObjectHelper::unwrap(L, 1, &udata);

		if ((R->SIGN != ROWS_SIGN) || (R->result == NULL))
			return luaL_error(L, "%d: The data was not read.", __LINE__);
		else {
			ROWS::WRAP *w = (ROWS::WRAP *)udata;
			KEY_FIELD *kf = R->fields[w->wIndex];

			SQL_ROWS rows = w->rows;
			SQL_DATA k = NULL;

			if (lua_type(L, 2) != LUA_TTABLE)
				k = toSQLData(L, 2, kf->v);
			else {
				SQLSMALLINT wIndex = w->wIndex;

				for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1))
				{
					if (wIndex >= (SQLSMALLINT)R->fields.size()) goto _gE;
					else if ((wIndex > 0) && !(rows = SQLStepRows(rows, k))) goto _gE;
					else {
						if (!(k = toSQLData(L, -1, R->fields[wIndex]->v)))
							return luaL_error(L, "%d: The data type does not match.", __LINE__);
					}
					wIndex++;
				}
			}

			if (!(rows = SQLStepRows(rows, k, NULL)))
_gE:			lua_pushnil(L);
			else if (rows->wIsType == (SQL_TRUE + 1)) toSQLData(rows->u.v, L);
			else luaL_pushhelper(L, R, (new ROWS::WRAP(R, w->wIndex + 1, rows))->Ref());
		}
		return 1;
	}
};


int Adapter::__execute(lua_State *L, Adapter *adp, void *u)
{
    const int base = lua_gettop(L);

    /*
        r = .execute(query_string, args)

        r = .execute({ query_string, params... }, args);
     */
    SQLResetReport();

    SQLHSTMT stmt = SQLAllocStmtEx(adp->dbc, adp->executeTimeout);
    if (stmt == NULL)
        return luaL_error(L, "%d: Failed to allocate odbc stmt.", __LINE__);
    else {
        std::string query_string;
        SQL_PARAMS params;
        std::stringset formats;

        if (lua_type(L, 1) != LUA_TTABLE)
            query_string = lua_tostring(L, 1);
        else {
            lua_pushnil(L);
            if (lua_next(L, 1) == 0)
            {
                SQLFreeStmtEx(stmt);
                return luaL_error(L, "%d: Query is not set.", __LINE__);
            }

            query_string = lua_tostring(L, -1);
            do {
                lua_pop(L, 1);

                if (lua_next(L, 1) == 0) break;
                switch (lua_type(L, -1))
                {
                    case LUA_TNIL: formats.push_back(""); break;
                    default: formats.push_back(lua_tostring(L, -1));
                }
            } while (true);
        }

        if (lua_type(L, 2) == LUA_TTABLE)
        {
            int r = -1;

            lua_pushnil(L);
            do {
                bool assign = true;

                while (formats.empty() == false)
                {
                    std::string f = formats.front(); /* EX) =10c : 10 bytes - char */

                    formats.pop_front();
                    if (f.length() == 0) break;
                    try {
                        char *e; int length = strtol(f.c_str() + (strchr("=+", *(f.c_str() + 0)) != NULL), &e, 10);;
                        switch (*(e + 0))
                        {
                            case 0x00:
                            case 'W': params.push_back(
                                    SQL_PARAM_WSTR((length > 0) ? new char[(length + 1) * sizeof(wchar_t)]: NULL, length * sizeof(wchar_t))
                            );
_gW:						{
                                SQL_PARAM &P = params.back();

                                P.cbValueMax = (P.rgbValue ? (length + 1) * sizeof(wchar_t): 0);
                            } break;
                            case 'w': params.push_back(
                                    SQL_PARAM_WCHAR((length > 0) ? new char[(length + 1) * sizeof(wchar_t)]: NULL, length * sizeof(wchar_t))
                            ); goto _gW;
                            case 'C': params.push_back(SQL_PARAM_STR((length > 0) ? new char[length + 1]: NULL, length));
_gC:						{
                                SQL_PARAM &P = params.back();

                                P.cbValueMax = (P.rgbValue ? (length + 1): 0);
                            } break;
                            case 'c': params.push_back(SQL_PARAM_CHAR((length > 0) ? new char[length + 1]: NULL, length)); goto _gC;
                            case 'B': if (strchr("=+", *(f.c_str() + 0))) goto _gC;
                            {
                                params.push_back(SQL_PARAM_BINARY(LONGVARBINARY, new UpdateBLOB(NULL, length), length));
                            } break;

                            case 'b': /* bool */
                            case 't': /* tinyint */ params.push_back(SQL_PARAM_SINT8(new char[sizeof(int8_t)])); break;
                            case 's': /* short */ params.push_back(SQL_PARAM_SINT16(new char[sizeof(int16_t)])); break;
                            case 'i': /* int */ params.push_back(SQL_PARAM_SINT32(new char[sizeof(int32_t)])); break;
                            case 'l': /* long */ params.push_back(SQL_PARAM_SINT64(new char[sizeof(int64_t)])); break;

                            case 'T': /* unsigned tinyint */ params.push_back(SQL_PARAM_UINT8(new char[sizeof(uint8_t)])); break;
                            case 'S': /* unsigned short */ params.push_back(SQL_PARAM_UINT16(new char[sizeof(uint16_t)])); break;
                            case 'I': /* unsigned int */ params.push_back(SQL_PARAM_UINT32(new char[sizeof(uint32_t)])); break;
                            case 'L': /* unsigned long */ params.push_back(SQL_PARAM_UINT64(new char[sizeof(uint64_t)])); break;

                            case 'f': /* float */ params.push_back(SQL_PARAM_FLOAT(new char[sizeof(float)])); break;
                            case 'd': /* double */ params.push_back(SQL_PARAM_DOUBLE(new char[sizeof(double)])); break;

                            case 'D': /* datetime */ params.push_back(SQL_PARAM_TYPE_TIMESTAMP(new char[sizeof(SQL_TIMESTAMP_STRUCT)])); break;
                        }
                    } catch (...) {
                        {
                            SQLParamsFree(params);
                            SQLFreeStmtEx(stmt);
                        } return luaL_error(L, "%d: There was a problem with the data type conversion.", __LINE__);
                    }

                    if (*(f.c_str() + 0) == '=')
                        params.back().fParamType = SQL_PARAM_OUTPUT;
                    else {
                        assign = false;
                        if (*(f.c_str() + 0) == '+')
                            params.back().fParamType = SQL_PARAM_INPUT_OUTPUT;
                        break;
                    }
                }

                if (r && ((r = lua_next(L, 2)) != 0))
                {
#define ASSIGN_PARAM(T, V, F)			do { \
	char *__v = new char[sizeof(T)]; params.push_back(F(__v)); *((T *)__v) = (T)V;	\
} while (0)
                    if (assign == true)
                        switch (lua_type(L, -1))
                        {
                            case LUA_TNIL: params.push_back(SQL_PARAM_NULL()); break;
                            case LUA_TSTRING:
                            {
                                size_t length = 0;
                                const char *s = lua_tolstring(L, -1, &length);
                                {
                                    char *v = new char[length + 1];

                                    strcpy(v, s);
                                    /* *(v + length) = 0; */
                                    params.push_back(SQL_PARAM_CHAR(v, (SQLLEN)length));
                                }
                            } break;

                            case LUA_TBOOLEAN: ASSIGN_PARAM(int8_t, lua_toboolean(L, -1), SQL_PARAM_UINT8); break;
                            case LUA_TNUMBER:
                            {
                                if (lua_isinteger(L, -1))
                                    ASSIGN_PARAM(int64_t, lua_tointeger(L, -1), SQL_PARAM_SINT64);
                                else ASSIGN_PARAM(double, lua_tonumber(L, -1), SQL_PARAM_DOUBLE);
                            } break;

                            case LUA_TTABLE:
                            {
                                bson b; bson_init(&b);
                                if (luaL_toBSON(L, -1, "", &b, NULL) < 0)
                                    bson_destroy(&b);
                                else {
                                    bson_finish(&b);
                                    {
                                        SQLLEN length = bson_size(&b);

                                        params.push_back(
                                                SQL_PARAM_BINARY(LONGVARBINARY, new UpdateBLOB(bson_data(&b), length), length)
                                        );
                                    }
                                    break;
                                }
                            }

                            default:
                            {
                                SQLParamsFree(params);
                                SQLFreeStmtEx(stmt);
                            } return luaL_error(L, "%d: This type is not supported. [%d]", __LINE__, lua_type(L, -1));
                        }
                    else {
                        SQL_PARAM &P = params.back();

#define DECLARE_ASSIGN_INTEGER(ST, T)	\
	case ST : switch (lua_type(L, -1))	\
	{	\
		case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;	\
		case LUA_TBOOLEAN: *((T *)P.rgbValue) = (T)lua_toboolean(L, -1); break;	\
		case LUA_TNUMBER: *((T *)P.rgbValue) = (T)lua_tointeger(L, -1); break;	\
		case LUA_TSTRING:	\
		{	\
			long long __v = strtoll(lua_tostring(L, -1), NULL, 10);	\
			if ((__v == LLONG_MIN) || (__v == LLONG_MAX))	\
				goto _gE;	\
			else *((T *)P.rgbValue) = (T)__v;	\
		} break;	\
		default: goto _gE;	\
	} break;

                        switch (P.fCType)
                        {
                            DECLARE_ASSIGN_INTEGER(SQL_C_STINYINT, int8_t)
                            DECLARE_ASSIGN_INTEGER(SQL_C_UTINYINT, uint8_t)

                            DECLARE_ASSIGN_INTEGER(SQL_C_SSHORT, int16_t)
                            DECLARE_ASSIGN_INTEGER(SQL_C_USHORT, uint16_t)

                            DECLARE_ASSIGN_INTEGER(SQL_C_SLONG, int32_t)
                            DECLARE_ASSIGN_INTEGER(SQL_C_ULONG, uint32_t)

                            DECLARE_ASSIGN_INTEGER(SQL_C_SBIGINT, int64_t)
                            DECLARE_ASSIGN_INTEGER(SQL_C_UBIGINT, uint64_t)

                            case SQL_C_WCHAR: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TSTRING:
                                {
                                    size_t length = 0;
                                    const char *s = lua_tolstring(L, -1, &length);
                                    {
#if 1
                                        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
                                        std::wstring w = utf8_conv.from_bytes(s, s + length);
#else
										std::wstring w = widen(s, length);
#endif
                                        if (P.rgbValue == NULL)
                                        {
                                            try {
                                                P.rgbValue = new char[(w.length() + 1) * sizeof(wchar_t)];
                                            } catch (...) { goto _gE; }
                                            memcpy(P.rgbValue, w.c_str(), P.cbColDef = w.length() * sizeof(wchar_t));
                                        }
                                        else if (w.length() <= 0) P.cbColDef = 0;
                                        else memcpy(P.rgbValue, w.c_str(), P.cbColDef = std::min((SQLLEN)(w.length() * sizeof(wchar_t)), P.cbValueMax));
                                        P.StrLen_or_Ind = P.cbColDef; /* *((wchar_t *)(v + P.cbColDef)) = 0; */
                                    }
                                } break;

                                default: goto _gE;
                            } break;

                            case SQL_C_CHAR: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TSTRING:
                                {
                                    size_t length = 0;
                                    const char *s = lua_tolstring(L, -1, &length);
                                    {
                                        if (P.rgbValue == NULL)
                                        {
                                            try {
                                                P.rgbValue = new char[length + 1];
                                            } catch (...) { goto _gE; }
                                            memcpy(P.rgbValue, s, P.cbColDef = length);
                                        }
                                        else if (length == 0) P.cbColDef = 0;
                                        else memcpy(P.rgbValue, s, P.cbColDef = std::min((SQLLEN)length, P.cbValueMax));
                                        P.StrLen_or_Ind = P.cbColDef; /* *(((char *)P.rgbValue) + P.cbColDef) = 0; */
                                    }
                                } break;

                                default: goto _gE;
                            } break;

                            case SQL_C_FLOAT: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TNUMBER: *((float *)P.rgbValue) = (float)lua_tonumber(L, -1); break;
                                case LUA_TSTRING:
                                {
                                    float v = strtof(lua_tostring(L, -1), NULL);
                                    if ((v == HUGE_VALF) || (v == -HUGE_VALF))
                                        goto _gE;
                                    else *((float *)P.rgbValue) = v;
                                } break;
                                default: goto _gE;
                            } break;

                            case SQL_C_DOUBLE: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TNUMBER: *((double *)P.rgbValue) = lua_tonumber(L, -1); break;
                                case LUA_TSTRING:
                                {
                                    double v = strtod(lua_tostring(L, -1), NULL);
                                    if ((v == HUGE_VAL) || (v == -HUGE_VAL))
                                        goto _gE;
                                    else *((double *)P.rgbValue) = v;
                                } break;
                                default: goto _gE;
                            } break;

                            case SQL_C_TYPE_TIMESTAMP: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TNUMBER:
                                {
                                    *((SQL_TIMESTAMP_STRUCT *)P.rgbValue) = SQLToTIMESTAMP(lua_tointeger(L, -1));
                                } break;

                                case LUA_TSTRING: try
                                {
                                    const char *s = lua_tostring(L, -1);
                                    struct tm utc;
                                    long msec = 0;

                                    if (strptime(s, "%Y-%m-%d %T", &utc) != NULL)
                                        ;
                                    else {
                                        char *x = (char *)strptime(s, "%Y-%m-%dT%T", &utc);
                                        if (x == NULL) throw __LINE__;
                                        else if (*x == '.')
                                        {
                                            char *e;

                                            msec = strtol(x + 1, &e, 10);
                                            if ((msec == LONG_MAX) || (msec == LONG_MIN) || (*(e + 0) != 'Z'))
                                                throw __LINE__;
                                        }
                                        else if (*x != 'Z') throw __LINE__;
                                    }

                                    *((SQL_TIMESTAMP_STRUCT *)P.rgbValue) = SQLToTIMESTAMP(mktime(&utc));
                                } catch (... ){ goto _gE; } break;
                                default: goto _gE;
                            } break;

                            case SQL_C_BINARY: switch (lua_type(L, -1))
                            {
                                case LUA_TNIL: P.StrLen_or_Ind = SQL_NULL_DATA; break;
                                case LUA_TSTRING:
                                {
                                    size_t length;
                                    const char *s = lua_tolstring(L, -1, &length);

                                    if (P.StrLen_or_Ind <= SQL_DATA_AT_EXEC)
                                    {
                                        ((UpdateBLOB *)P.rgbValue)->SetData(strdup(s), length);
                                        if (P.StrLen_or_Ind <= SQL_LEN_DATA_AT_EXEC_OFFSET)
                                            P.StrLen_or_Ind = SQL_LEN_DATA_AT_EXEC(((SQLLEN)length));
                                    }
                                    else {
                                        P.cbColDef = std::min((SQLLEN)length, P.cbValueMax);
                                        strcpy((char *)P.rgbValue, s);

                                        P.StrLen_or_Ind = P.cbColDef; /* *(v + P.cbColDef) = 0; */
                                    }
                                } break;

                                case LUA_TTABLE:
                                {
                                    bson b; bson_init(&b);
                                    if (luaL_toBSON(L, -1, "", &b, NULL) < 0)
                                        bson_destroy(&b);
                                    else {
                                        bson_finish(&b);
                                        {
                                            SQLLEN length = bson_size(&b);

                                            if (P.StrLen_or_Ind <= SQL_DATA_AT_EXEC)
                                            {
                                                ((UpdateBLOB *)P.rgbValue)->SetData(bson_data(&b), length);
                                                if (P.StrLen_or_Ind <= SQL_LEN_DATA_AT_EXEC_OFFSET)
                                                    P.StrLen_or_Ind = SQL_LEN_DATA_AT_EXEC(length);
                                            }
                                            else {
                                                P.cbColDef = std::min((SQLLEN)bson_size(&b), P.cbValueMax);
                                                memcpy(P.rgbValue, bson_data(&b), P.cbColDef);
                                                bson_destroy(&b);

                                                P.StrLen_or_Ind = P.cbColDef;
                                            }
                                        }
                                        break;
                                    }
                                }

                                default: goto _gE;
                            } break;

_gE:						default:
                            {
                                SQLParamsFree(params);
                                SQLFreeStmtEx(stmt);
                            } return luaL_error(L, "%d: There was a problem with the data type conversion.", __LINE__);
                        }
                    }
#undef DECLARE_ASSIGN_INTEGER
                    lua_pop(L, 1);
                }
#undef ASSIGN_PARAM
            } while ((formats.empty() == false) || r);
        }

        SQLRETURN rc = SQLExecuteEx(stmt, (SQLCHAR *)query_string.c_str(), params.data());
        if (SQL_SUCCEEDED(rc))
            try {
                ResultSet *r = new ResultSet(adp, stmt);

                r->query = query_string.c_str();
                r->params.assign(params.begin(), params.end()); {
                    luaL_pushhelper(L, r);
                } params.clear();

                stmt = SQL_NULL_HSTMT;
            } catch (...) { }
        else {
            lua_getglobal(L, "_TRACEBACK");
            if (lua_type(L, -1) == LUA_TFUNCTION)
            {
                int base = lua_gettop(L);

                lua_pushfstring(L, "SQL %d '%s'", rc, query_string.c_str());
                lua_pcall(L, 1, 0, base);
            }
            lua_settop(L, base);
        }

        SQLParamsFree(params);
        SQLFreeStmtEx(stmt);
    }
    return lua_gettop(L) - base;
}

/* r = .apply(row, [table,] function (changelog)) */
int Adapter::__apply(lua_State *L, Adapter *adp, void *udata)
{
    ResultSet::ROWS *R = (ResultSet::ROWS *)luaObjectHelper::unwrap(L, 1, NULL);

    if ((R->SIGN != ROWS_SIGN) || (R->result == NULL))
        return luaL_error(L, "%d: The data was not read.", __LINE__);
    else if (SQLIsDirtyResult(R->result))
    {
	    const int f = lua_gettop(L);
        SQLResetReport();

        SQLHSTMT stmt = SQLAllocStmtEx(adp->dbc, adp->executeTimeout);
        if (stmt == NULL)
            return luaL_error(L, "%d: Failed to allocate odbc stmt.", __LINE__);
        else {
            if (ResultSet::apply(L, stmt, R, (SQLCHAR *)
                    ((lua_type(L, 2) == LUA_TSTRING) ? lua_tostring(L, 2) : NULL)) == 0)
                switch (lua_type(L, f))
                {
                    case LUA_TFUNCTION: switch (luaObjectHelper::call(L, f, lua_gettop(L) - f))
                    {
                    	case LUA_OK: return lua_gettop(L) - f;
                    	default:
	                    {
	                    	SQLFreeStmtEx(stmt);
	                    } return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
                    }
                    default: break;
                }
        }

	    SQLFreeStmtEx(stmt);
	    return 1;
    }
    return 0;
}


int Adapter::__begin(lua_State *L, Adapter *adp, void *u)
{
    /*
    .begin([function (adp)])
    */
    SQLRETURN rc;
    int f = lua_gettop(L);

    SQLResetReport();
    if (SQL_SUCCEEDED(rc = SQLBegin(adp->dbc)) == false)
    	lua_pushinteger(L, rc);
    else if (lua_type(L, f) != LUA_TFUNCTION)
    {
        BOOL commit = false;

        luaL_pushhelper(L, adp);
        switch (luaObjectHelper::call(L, f, 1))
        {
            case LUA_OK: switch (lua_type(L, -1))
            {
                default: break;
                case LUA_TBOOLEAN: commit = lua_toboolean(L, -1);
            } break;
            default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
        }
        lua_pushinteger(L, SQLCommit(adp->dbc, commit));
    }
    return lua_gettop(L) - f;
}


int Adapter::__commit(lua_State *L, Adapter *adp, void *u)
{
    /*
    .commit()
    */
    const int base = lua_gettop(L);

    SQLResetReport();
    lua_pushinteger(L, SQLCommit(adp->dbc, false));
    return lua_gettop(L) - base;
}


int Adapter::__rollback(lua_State *L, Adapter *adp, void *u)
{
    /*
    .rollback()
    */
    const int base = lua_gettop(L);

    SQLResetReport();
    lua_pushinteger(L, SQLCommit(adp->dbc, true));
    return lua_gettop(L) - base;
}

int Adapter::__close(lua_State *L, Adapter *adp, void *u)
{
    /*
    .close()
    */
    const int base = lua_gettop(L);

    lua_pushinteger(L, adp->bind(NULL));
    return lua_gettop(L) - base;
}


Adapter::~Adapter() { this->bind(NULL); }


/*
    r = odbc.new(connect_string [, function (adapter) ])
 */
static int __new(lua_State *L)
{
    int r = 0;

    SQLResetReport();
    try {
        Adapter *adp = new Adapter();

        /* adp->link_.udata = lua_touserdata(L, lua_upvalueindex(2));
        adp->link_.f = (int (*)(const char *, void **, void *))lua_touserdata(L, lua_upvalueindex(1)); */
        switch (lua_type(L, 1))
        {
            case LUA_TSTRING:  ;
            {
                adp->executeTimeout = adp->bind(lua_tostring(L, 1), &adp->spec);
            } break;
            case LUA_TTABLE:
#define CONNECT_TIMEOUT				3
            {
                std::string connect_string;
                SQLINTEGER connectionTimeout = CONNECT_TIMEOUT,
                           loginTimeout = SQL_NTS;

                for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1))
                {
                    const char *k = lua_tostring(L, -2);

                    switch (*(k + 0))
                    {
                        case ':':
                        case '.': ++k;
                        {
                            /* .executeTimeout: 100 */
                            if (strcasecmp(k, "executeTimeout") == 0) adp->executeTimeout = (SQLINTEGER)lua_tointeger(L, -1);
                            else if (strcasecmp(k, "connectionTimeout") == 0) connectionTimeout = (SQLINTEGER)lua_tointeger(L, -1);
                            else if (strcasecmp(k, "loginTimeout") == 0) loginTimeout = (SQLINTEGER)lua_tointeger(L, -1);
                            else if (strcasecmp(k, "queryDriver") == 0)
                            {
                                const char *v = lua_tostring(L, -1);

                                if (strcasecmp(v, "mysql") == 0) adp->spec = &SQL_MySQL;
                                else if (strcasecmp(v, "mssql") == 0) adp->spec = &SQL_MSSQL;
                            }
                        } break;
                        default: connect_string += k; /* K */
                        {
                            connect_string += "=";
                            if (lua_type(L, -1) == LUA_TFUNCTION)
                                switch (luaObjectHelper::call(L, 0, 0, 1))
                                {
                                    case LUA_OK: break;
                                    default:
                                    {
                                        delete adp;
                                    } throw __LINE__;
                                }

                            connect_string += lua_tostring(L, -1); /* V */
                            connect_string += ";";
                        } break;
                    }
                }

                adp->dbc = SQLConnectEx((SQLCHAR *)connect_string.c_str(), connectionTimeout, loginTimeout);
            } break;
#undef CONNECT_TIMEOUT
        }

        if (adp->dbc == NULL)
            delete adp;
        else {
        	int f = lua_absindex(L, -1);

            luaL_pushhelper(L, adp);
            if (lua_type(L, f) == LUA_TFUNCTION)
                switch (luaObjectHelper::call(L, f, 1))
                {
                    default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
                    case LUA_OK: r = lua_gettop(L) - f;
                }
            else ++r;
        }
    } catch (...) { }
    return r;
}

#if 0
static int __close(lua_State *L)
{
    Adapter *adp = (Adapter *)luaObjectHelper::unwrap(L, 1, NULL);

    lua_pushinteger(L, adp->bind(NULL));
    return 1;
}
#endif

#include "lock.h"

struct OBJECT {
    bson             b;
    ResultSet::ROWS *R;
}; typedef std::unordered_map<std::string, OBJECT> ROWS_CacheSet;

namespace odbc {

    static struct ODBC__SYS {
        ROWS_CacheSet rows;
        LOCK_T        lock;

        ODBC__SYS() { INIT_LOCK(&lock); }
        ~ODBC__SYS() {
            for (ROWS_CacheSet::iterator r_it = rows.begin(); r_it != rows.end(); ++r_it)
            {
                OBJECT *O = &r_it->second;

                if (O->R != NULL) O->R->Unref();
                else if (O->b.data != NULL) bson_destroy(&O->b);
            }
            DESTROY_LOCK(&lock);
        }
    } *ESYS = NULL;

};

#define ESYS					odbc::ESYS


/* local row = odbc.cache(ID, function ()
 * end)
 */
static int __cache(lua_State *L)
{
    OBJECT *O = NULL;

    const char *id = luaL_checkstring(L, 1);
    ENTER_LOCK(&ESYS->lock); {
        ROWS_CacheSet::iterator r_it = ESYS->rows.find(id);
        if (r_it == ESYS->rows.end())
            switch (lua_type(L, 2))
            {
                case LUA_TNONE:
_gN:            case LUA_TNIL: LEAVE_LOCK(&ESYS->lock);
                {
                    ;
                } return 0;

                default:
                {
                    r_it = ESYS->rows.insert(ROWS_CacheSet::value_type(id, OBJECT())).first;

                    r_it->second.R = NULL;
                    r_it->second.b.data = NULL;
                    if (lua_type(L, 2) == LUA_TFUNCTION)
                    {
                        int r;

                        LEAVE_LOCK(&ESYS->lock); {
                            r = luaObjectHelper::call(L, 2, 0, 1);
                        } ENTER_LOCK(&ESYS->lock);
                        if (r != LUA_OK)
                        {
                        	_WARN("%d: %s", __LINE__, lua_tostring(L, -1));
	                        goto _gE;
                        }
                    }

                    switch (lua_type(L, -1))
                    {
                        case LUA_TUSERDATA: O = &(r_it = ESYS->rows.find(id))->second;
                        {
                            ResultSet::ROWS *R = (ResultSet::ROWS *)luaObjectHelper::unwrap(L, -1, NULL);

                            if (R->SIGN == ROWS_SIGN)
                                (O->R = R)->Ref();
                            else {
                                ESYS->rows.erase(r_it); {
                                    LEAVE_LOCK(&ESYS->lock);
                                } return luaL_error(L, std::format("Unsupport SIGN: %X", R->SIGN).c_str());
                            }
                        } break;

                        default: O = &(r_it = ESYS->rows.find(id))->second;
                        {
                            bson_init(&O->b);
                            luaL_toBSON(L, -1, "v", &O->b, NULL);
                            bson_finish(&O->b);
                        } break;

                        case LUA_TNIL:
                        case LUA_TNONE:
_gE:                    {
                            ESYS->rows.erase(id);
                        } goto _gN;
                    }
                } break;
            }
        else O = &r_it->second;

        if (O->R != NULL)
            luaL_pushhelper(L, O->R, (new ResultSet::ROWS::WRAP(O->R, 0, &O->R->result->ROWS))->Ref());
        else if (O->b.data != NULL) luaL_pushBSON(L, 0, &O->b);
        else { /* 캐싱 진행 중... 동일한 처리를 진행하고, 값은 저장하지 않는다. */
            LEAVE_LOCK(&ESYS->lock);
            switch (lua_type(L, 2))
            {
                case LUA_TFUNCTION: switch (luaObjectHelper::call(L, 2, 0, 1))
                {
                    case LUA_OK: break;
                    default: return luaL_error(L, "%d: %s", __LINE__, lua_tostring(L, -1));
                } break;

                case LUA_TNONE: return 0;
                default: lua_pushvalue(L, -1);
            } return 1;
        }
    } LEAVE_LOCK(&ESYS->lock);
    return 1;
}

/* odbc.purge(ID) */
static bool ROWS_eraseCache(const char *id, ROWS_CacheSet &purge)
{
    ROWS_CacheSet::iterator r_it = ESYS->rows.find(id);
    if (r_it == ESYS->rows.end())
        return false;
    else {
        purge.insert(ROWS_CacheSet::value_type(r_it->first, r_it->second));
        ESYS->rows.erase(r_it);
    }
    return true;
}

/* odbc.purge(cache_id) */
static int __purge(lua_State *L)
{
    ROWS_CacheSet purge;

    ENTER_LOCK(&ESYS->lock); switch (lua_type(L, 1))
    {
        case LUA_TNONE: for (ROWS_CacheSet::iterator r_it = ESYS->rows.begin(); r_it != ESYS->rows.end();)
        {
            OBJECT *O = &r_it->second;

            if ((O->R == NULL) || (SQLIsDirtyResult(O->R->result) == false))
                r_it++;
            else {
                purge.insert(ROWS_CacheSet::value_type(r_it->first, *O));
                r_it = ESYS->rows.erase(r_it);
            }
        } break;

        case LUA_TSTRING: ROWS_eraseCache(lua_tostring(L, 1), purge); break;
        case LUA_TTABLE:
        {
            for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1))
                switch (lua_type(L, -1))
                {
                    case LUA_TSTRING: ROWS_eraseCache(lua_tostring(L, -1), purge);
                    default: break;
                }
        } break;
        default: break;
    } LEAVE_LOCK(&ESYS->lock);

    lua_newtable(L); {
        int __i = 0;

        for (ROWS_CacheSet::iterator r_it = purge.begin(); r_it != purge.end(); r_it++)
        {
            lua_pushstring(L, r_it->first.c_str());
            lua_rawseti(L, -2, ++__i);
            {
                OBJECT *O = &r_it->second;

                if (O->R != NULL) O->R->Unref();
                else bson_destroy(&O->b);
            }
        }
    } return 1;
}


static const luaL_Reg odbclib[] = {
        { "new"  , __new },

        { "cache", __cache },
        { "purge", __purge },

        { NULL   , NULL    }
};


static pthread_once_t ODBC__initOnce = PTHREAD_ONCE_INIT;

static SQLRETURN LUA__SQLErrorReport(
        SQLHANDLE handle, SQLSMALLINT fHandleType, struct SQL_DIAG_REC *pDiagRec, SQLINTEGER lCountRec)
{
    for (; (--lCountRec) >= 0; pDiagRec++)
        _WARN("[SQL] %s %d %s", pDiagRec->szSqlState, pDiagRec->NativeError, pDiagRec->szErrorMsg);
    return SQL_SUCCESS;
}

static void ODBC__atExit() { delete ESYS; }
static void ODBC__atInit()
{
    SQLSetErrorReport(LUA__SQLErrorReport);

    if (SQLInit() != SQL_SUCCESS)
        _EXIT("%d: ODBC", __LINE__);
    else {
        try {
            ESYS = new odbc::ODBC__SYS();
        } catch (...) { _EXIT("%d: ODBC", __LINE__); }
    }
    atexit(ODBC__atExit);
}

};

LUALIB_API "C" int luaopen_odbc(lua_State *L)
{
    pthread_once(&lua__odbc::ODBC__initOnce, lua__odbc::ODBC__atInit);
    luaL_newlib(L, lua__odbc::odbclib);
    return 1;
}