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
#ifndef _ODBC_H
#  define _ODBC_H
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \brief ODBC기반으로 데이터베이스와 입출력을 할수 있도록 지원
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include "typedef.h"

#include <stdlib.h>
#include <memory.h>
#include <typeinfo>
#include <algorithm>

#include <sql.h>
#include <sqlext.h>
#include <stdlib.h>
#include <unordered_map>
#include <time.h>

/*! \addtogroup core_odbc
 *  @{
 */
typedef SQLULEN 			SQLROWCOUNT;

struct SQL_DIAG_REC {
	SQLINTEGER NativeError;
	SQLCHAR    szSqlState[SQL_DIAG_MESSAGE_TEXT];
	SQLCHAR    szErrorMsg[SQL_MAX_MESSAGE_LENGTH + 1];
};

SQLINTEGER SQLGetLastError(struct SQL_DIAG_REC **pDiagRec);

typedef SQLRETURN(*fnSQLErrorReport)(
	SQLHANDLE handle, SQLSMALLINT fHandleType, struct SQL_DIAG_REC *pDiagRec, SQLINTEGER lCountRec
	);

#if 0
static SQLRETURN DEFAULT_SQLErrorReport(SQLHANDLE handle, SQLSMALLINT fHandleType)
{
	SQLSMALLINT i = 0;
	SQLINTEGER native /*, count = 0 */;
	SQLCHAR state[SQL_DIAG_MESSAGE_TEXT];
	SQLCHAR text[SQL_MAX_MESSAGE_LENGTH + 1];
	SQLSMALLINT len;

	SQLRETURN rc /* = SQLGetDiagField(fHandleType, handle, 0, SQL_DIAG_NUMBER, &count, SQL_IS_INTEGER, &len) */;
	do {
		rc = SQLGetDiagRecA(fHandleType, handle, ++i, state, &native, text, SQL_MAX_MESSAGE_LENGTH, &len);
		if (SQL_SUCCEEDED(rc) && (len > 0))
		{
			*(text + len) = 0;
			fprintf(stderr, " [SQL] %s %d %s\n", state, native, text);
			/* ODBC.reportCallback(i, state, native, text); */
		}
	} while (rc == SQL_SUCCESS);
	return rc;
}
#endif

void SQLSetErrorReport(fnSQLErrorReport errorReport);

/* SQLErrorReport: handle에서 발생된 오류를 SQLErrorReportCallback()으로 전송한다. */
SQLRETURN  SQLErrorReport(SQLHANDLE handle, SQLSMALLINT fHandleType);
#define SQLResetReport()				SQLErrorReport(SQL_NULL_HANDLE, 0)

/* SQLInit/SQLExit: ODBC를 초기화 하거나, 제거한다. */
SQLRETURN SQLInit();
SQLRETURN SQLExit();


#define SQL_ATTR(N, V, L)			(SQLINTEGER)(SQL_ATTR_##N), (SQLPOINTER)((SQLULEN)(V)), (SQLINTEGER)(L)
#define SQL_QUERY(N, V, L)			(SQLINTEGER)(SQL_QUERY_##N), (SQLPOINTER)((SQLULEN)(V)), (SQLINTEGER)(L)

/* SQLSetAttrs: SQLSetConnectAttr(SQL_HANDLE_DBC)/SQLSetStmtAttr(SQL_HANDLE_STMT)를 설정한다.
- 마지막 설정에 SQL_NTS를 지정하여야 한다.
*/
SQLRETURN SQLSetAttrs(SQLHANDLE handle, SQLSMALLINT fHandleType, ...);

/* SQLIsConnectDead: 데이터베이스의 연결이 해제된 상태인지 체크한다.
*/
SQLRETURN SQLIsConnectDead(SQLHDBC dbc);

SQLCHAR  *SQLConnectString(SQLHDBC dbc, SQLCHAR *lpszExtraID = NULL);
SQLHDBC   SQLConnectEx(SQLCHAR *lpszConnectionString, SQLINTEGER connectionTimeout, SQLINTEGER loginTimeout = SQL_NTS);
SQLRETURN SQLDisconnectEx(SQLHDBC dbc);

/* 데이터베이스로 부터, 실제 명령을 수행하기 위한 Stmt를 생성하거나, 제거한다.
*/
SQLHSTMT  SQLAllocStmtEx(SQLHDBC dbc, SQLINTEGER queryTimeOut);
SQLRETURN SQLResetStmt(SQLHSTMT stmt); /* SQLExecute()된 정보를 리셋한다. */
SQLRETURN SQLFreeStmtEx(SQLHSTMT stmt);


SQLRETURN SQLBegin(SQLHDBC dbc);
SQLRETURN SQLCommit(SQLHDBC dbc, BOOL IsRollback);

/* SQLExecuteEx에 파라메터를 설정하기 위해서, 사용되는 구조체로 SQLBindParameter()의 인자와 1:1 대응된다.
*/
#include <vector>

typedef struct tagSQL_PARAM {
	SQLSMALLINT  fParamType;
	SQLSMALLINT  fCType, fSqlType;
	SQLULEN      cbColDef;
	SQLUSMALLINT ibScale;
	SQLPOINTER   rgbValue;
	SQLLEN       cbValueMax;
	SQLLEN       StrLen_or_Ind;
} SQL_PARAM;

typedef std::vector<SQL_PARAM> SQL_PARAMS;

#define SQL_PARAM_DECLARE(PARAM, C_TYPE, SQL_TYPE, P, S, V, L, I)		\
	{ SQL_PARAM_##PARAM, SQL_C_##C_TYPE, SQL_##SQL_TYPE, P, S, (SQLPOINTER)V, L, (SQLLEN)(I) }

#define SQL_PARAM_NIL()					{ 0, 0, 0, 0, 0, 0, 0, 0 }
/*
*/
#define SQL_PARAM_CHAR(V, L)			SQL_PARAM_DECLARE(INPUT, CHAR, CHAR, (SQLULEN)L, 0, V, 0, L)
#define SQL_PARAM_STR(V, L)				SQL_PARAM_DECLARE(INPUT, CHAR, VARCHAR, (SQLULEN)L, 0, V, 0, SQL_NTS)

#define SQL_PARAM_WCHAR(V, L)			SQL_PARAM_DECLARE(INPUT, WCHAR, WCHAR, (SQLULEN)L, 0, V, 0, L)
#define SQL_PARAM_WSTR(V, L)			SQL_PARAM_DECLARE(INPUT, WCHAR, WVARCHAR, (SQLULEN)L, 0, V, 0, SQL_NTS)

#define SQL_PARAM_INT8(V)				SQL_PARAM_DECLARE(INPUT, TINYINT, TINYINT, 0, 0, V, 0, 0)
#define SQL_PARAM_INT16(V)				SQL_PARAM_DECLARE(INPUT, SHORT, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_INT32(V)				SQL_PARAM_DECLARE(INPUT, LONG, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_INT64(V)				SQL_PARAM_DECLARE(INPUT, BIGINT, BIGINT, 0, 0, V, 0, 0)

#define SQL_PARAM_SINT8(V)				SQL_PARAM_DECLARE(INPUT, STINYINT, TINYINT, 0, 0, V, 0, 0)
#define SQL_PARAM_SINT16(V)				SQL_PARAM_DECLARE(INPUT, SSHORT, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_SINT32(V)				SQL_PARAM_DECLARE(INPUT, SLONG, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_SINT64(V)				SQL_PARAM_DECLARE(INPUT, SBIGINT, BIGINT, 0, 0, V, 0, 0)

#define SQL_PARAM_UINT8(V)				SQL_PARAM_DECLARE(INPUT, UTINYINT, TINYINT, 0, 0, V, 0, 0)
#define SQL_PARAM_UINT16(V)				SQL_PARAM_DECLARE(INPUT, USHORT, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_UINT32(V)				SQL_PARAM_DECLARE(INPUT, ULONG, INTEGER, 0, 0, V, 0, 0)
#define SQL_PARAM_UINT64(V)				SQL_PARAM_DECLARE(INPUT, UBIGINT, BIGINT, 0, 0, V, 0, 0)

#define SQL_PARAM_FLOAT(V)				SQL_PARAM_DECLARE(INPUT, FLOAT, FLOAT, 0, 0, V, 0, 0)
#define SQL_PARAM_DOUBLE(V)				SQL_PARAM_DECLARE(INPUT, DOUBLE, DOUBLE, 0, 0, V, 0, 0)

#define SQL_PARAM_DECIMAL(V, P, S)		SQL_PARAM_DECLARE(INPUT, CHAR, DECIMAL, P, S, V, 0, SQL_NTS)
#define SQL_PARAM_NUMERIC(V, P, S)		SQL_PARAM_DECLARE(INPUT, CHAR, NUMERIC, P, S, V, 0, SQL_NTS)
/* #define SQL_PARAM_DECIMAL(V, P, S)		SQL_PARAM_DECLARE(INPUT, DOUBLE, DECIMAL, P, S, V, 0, 0) */
/* #define SQL_PARAM_NUMERIC(V, P, S)		SQL_PARAM_DECLARE(INPUT, NUMERIC, NUMERIC, P, S, V, sizeof(SQL_NUMERIC_STRUCT), 0) */

#define SQL_PARAM_TYPE_TIME(V)			SQL_PARAM_DECLARE(INPUT, TYPE_TIME, TYPE_TIME, SQL_TIME_LEN + 1, 0, V, sizeof(TIME_STRUCT), 0)
#define SQL_PARAM_TYPE_DATE(V)			SQL_PARAM_DECLARE(INPUT, TYPE_DATE, TYPE_DATE, SQL_DATE_LEN + 1, 0, V, sizeof(DATE_STRUCT), 0)
#define SQL_PARAM_TYPE_TIMESTAMP(V)		SQL_PARAM_DECLARE(INPUT, TYPE_TIMESTAMP, TYPE_TIMESTAMP, SQL_TIMESTAMP_LEN + 1, 0, V, sizeof(TIMESTAMP_STRUCT), 0)

#define SQL_PARAM_NULL()				SQL_PARAM_DECLARE(INPUT, DEFAULT, CHAR, 0, 0, NULL, 0, SQL_NULL_DATA)
/* SQL_BINARY_CALLBACK: SQL_PARAM_BINARY()로 설정시 사용되는 Callback 클래스로, 데이터베이스에 BINARY데이터를
저장하는 용도로, 사용이 되어 진다.

NEED_DATA - 데이터베이스로 저장할 버퍼를 요청한다.
DISPOSE - 사용을 마쳤으며, 제거를 요청한다.
*/
class SQL_BINARY_CALLBACK {
public:
	virtual SQLINTEGER NEED_DATA(SQL_PARAM *pParam,
		SQLINTEGER iOffset, SQLPOINTER pExternalBuffer, SQLPOINTER *pValue) = 0;
	virtual void       DISPOSE() { }
};

#define SQL_PARAM_BINARY(T, CB, L)	    SQL_PARAM_DECLARE(INPUT, BINARY, T, (SQLULEN)INT32_MAX, 0, CB, 0, SQL_LEN_DATA_AT_EXEC(L))

SQLRETURN SQLExecuteEx(SQLHSTMT stmt, SQLCHAR *szSqlStr, SQL_PARAM *args, SQLPOINTER pExternalBuffer = NULL);

/* SQLExecuteEx() 이후, 결과에 대한 컬럼 정보를 처리하거나, SQLColumnData()를 사용해 데이터를 얻기위해 사용된다.
*/
#pragma pack(push, 1)
typedef struct tagSQL_FIELD_STRUCT {
	SQLSMALLINT  wAlignIndex;
	SQLSMALLINT  wCType;
	SQLSMALLINT  wSQLType;			/* SQL_COLUMN_TYPE */
	SQLLEN       dwColumnLength;	/* SQL_COLUMN_LENGTH */
	SQLSMALLINT  wScale;
	SQLULEN      dwPrecision;

	SQLSMALLINT  wUpdatable;		/* SQL_COLUMN_UPDATABLE */
	SQLSMALLINT  wAutoIncrement;	/* SQL_COLUMN_AUTO_INCREMENT  */
} SQL_FIELD_STRUCT,
*SQL_FIELD;

typedef struct tagSQL_DATA_STRUCT {
	SQLSMALLINT wCType;
	SQLLEN      StrLen_or_Ind; /* SQL_NULL_DATA		NULL 인 상태
							   0					데이터가 없는 상태
							   */
	SQLLEN      dwColumnLength;		/* 할당된 최대 크기 */
} SQL_DATA_STRUCT,
*SQL_DATA; /* SQL_DATA + 1 == data */

#define SQL_DATA_LEN(D)				max((D)->StrLen_or_Ind, 0)

/*
 *
 */
#include <time.h>

inline time_t SQLFromTIME(SQL_TIME_STRUCT *t) /* SQL_TIME -> time_t */
{
	struct tm timeNow; memset(&timeNow, 0, sizeof(timeNow));

	timeNow.tm_hour = t->hour;
	timeNow.tm_min = t->minute;
	timeNow.tm_sec = t->second;
	return mktime(&timeNow);
}

inline SQL_TIME_STRUCT SQLToTIME(time_t tm) /* SQL_TIME <- time_t */
{
	SQL_TIME_STRUCT t; memset(&t, 0, sizeof(t));
	struct tm timeNow; localtime_r(&tm, &timeNow);

	t.hour = timeNow.tm_hour;
	t.minute = timeNow.tm_min;
	t.second = timeNow.tm_sec;
	return t;
}


inline time_t SQLFromDATE(SQL_DATE_STRUCT *d) /* SQL_DATE -> time_t */
{
	struct tm timeNow; memset(&timeNow, 0, sizeof(timeNow));

	timeNow.tm_year = d->year - 1900;
	timeNow.tm_mon = d->month - 1;
	timeNow.tm_mday = d->day;
	return mktime(&timeNow);
}

inline SQL_DATE_STRUCT SQLToDATE(time_t tm) /* SQL_DATE <- time_t */
{
	SQL_DATE_STRUCT d; memset(&d, 0, sizeof(d));
	struct tm timeNow; localtime_r(&tm, &timeNow);

	d.year = timeNow.tm_year + 1900;
	d.month = timeNow.tm_mon + 1;
	d.day = timeNow.tm_mday;
	return d;
}


inline time_t SQLFromTIMESTAMP(SQL_TIMESTAMP_STRUCT *ts) /* SQL_TIMESTAMP -> time_t */
{
	struct tm timeNow; memset(&timeNow, 0, sizeof(timeNow));

	timeNow.tm_year = ts->year - 1900;
	timeNow.tm_mon = ts->month - 1;
	timeNow.tm_mday = ts->day;
	timeNow.tm_hour = ts->hour;
	timeNow.tm_min = ts->minute;
	timeNow.tm_sec = ts->second;
	return mktime(&timeNow);
}

inline SQL_TIMESTAMP_STRUCT SQLToTIMESTAMP(time_t tm) /* SQL_TIMESTAMP <- time_t */
{
	SQL_TIMESTAMP_STRUCT ts; memset(&ts, 0, sizeof(ts));
	struct tm timeNow; localtime_r(&tm, &timeNow);

	ts.year = timeNow.tm_year + 1900;
	ts.month = timeNow.tm_mon + 1;
	ts.day = timeNow.tm_mday;
	ts.hour = timeNow.tm_hour;
	ts.minute = timeNow.tm_min;
	ts.second = timeNow.tm_sec;
	return ts;
}

/*
*
*/
template <typename T>
T *SQL_DATA_PTR(SQL_DATA pSqlData, T *vDef = NULL) {
	return ((pSqlData->StrLen_or_Ind >= 0) ? (T *)(pSqlData + 1) : vDef);
}

template <typename T> T SQL_DATA_VALUE(SQL_DATA pSqlData, T vDef = (T)0)
{
	if (pSqlData->StrLen_or_Ind <= 0)
		return vDef;
	else {
		SQLPOINTER v = (SQLPOINTER)(pSqlData + 1);
		switch (pSqlData->wCType)
		{
#define RETURN_TIME(S, F)	do {	\
	if (typeid(T) == typeid(time_t)) return (T)F((S *)v);	\
	else if (typeid(T) == typeid(S)) return *((T *)v);	\
	else throw std::bad_cast();	\
} while (0)
			case SQL_C_TYPE_TIME: RETURN_TIME(SQL_TIME_STRUCT, SQLFromTIME);
			case SQL_C_TYPE_DATE: RETURN_TIME(SQL_DATE_STRUCT, SQLFromDATE);
			case SQL_C_TYPE_TIMESTAMP: RETURN_TIME(SQL_TIMESTAMP_STRUCT, SQLFromTIMESTAMP);
#undef RETURN_TIME
            case SQL_C_WCHAR:
            {
#define RETURN_NUMBER(F)	do {	\
	wchar_t *EndPtr; T _to = (T)F((const wchar_t *)v, &EndPtr, 10); \
	return (EndPtr == (wchar_t *)v) ? vDef : _to;	\
} while (0)
				if (typeid(T) == typeid(int8_t))        RETURN_NUMBER(wcstol);
				else if (typeid(T) == typeid(int16_t))  RETURN_NUMBER(wcstoll);
				else if (typeid(T) == typeid(int32_t))  RETURN_NUMBER(wcstoll);
				else if (typeid(T) == typeid(int64_t))  RETURN_NUMBER(wcstoll);
				else if (typeid(T) == typeid(uint8_t))  RETURN_NUMBER(wcstoull);
				else if (typeid(T) == typeid(uint16_t)) RETURN_NUMBER(wcstoull);
				else if (typeid(T) == typeid(uint32_t)) RETURN_NUMBER(wcstoull);
				else if (typeid(T) == typeid(uint64_t)) RETURN_NUMBER(wcstoull);
#define RETURN_DOUBLE(F)	do {	\
	wchar_t *EndPtr; T _to = (T)F((const wchar_t *)v, &EndPtr); \
	return (EndPtr == (wchar_t *)v) ? vDef : _to;	\
} while (0)
				else if (typeid(T) == typeid(float))    RETURN_DOUBLE(wcstold);
				else if (typeid(T) == typeid(double))   RETURN_DOUBLE(wcstold);
#undef RETURN_DOUBLE
#undef RETURN_NUMBER
				else throw std::bad_cast();
			} return vDef;

		    case SQL_C_CHAR:
			{
#define RETURN_NUMBER(F)	do {	\
	char *EndPtr; T _to = (T)F((const char *)v, &EndPtr, 10); \
	return (EndPtr == (char *)v) ? vDef : _to;	\
} while (0)
				if (typeid(T) == typeid(int8_t))        RETURN_NUMBER(strtoll);
				else if (typeid(T) == typeid(int16_t))  RETURN_NUMBER(strtoll);
				else if (typeid(T) == typeid(int32_t))  RETURN_NUMBER(strtoll);
				else if (typeid(T) == typeid(int64_t))  RETURN_NUMBER(strtoll);
				else if (typeid(T) == typeid(uint8_t))  RETURN_NUMBER(strtoull);
				else if (typeid(T) == typeid(uint16_t)) RETURN_NUMBER(strtoull);
				else if (typeid(T) == typeid(uint32_t)) RETURN_NUMBER(strtoull);
				else if (typeid(T) == typeid(uint64_t)) RETURN_NUMBER(strtoull);
#define RETURN_DOUBLE(F)	do {	\
	char *EndPtr; T _to = (T)F((const char *)v, &EndPtr); \
	return (EndPtr == (char *)v) ? vDef : _to;	\
} while (0)
				else if (typeid(T) == typeid(float))    RETURN_DOUBLE(strtold);
				else if (typeid(T) == typeid(double))   RETURN_DOUBLE(strtold);
#undef RETURN_DOUBLE
#undef RETURN_NUMBER
				else throw std::bad_cast();
			} return vDef;
		}

		return *((T *)v);
	}
}

inline SQL_DATA SQL_DATA_NEXT(SQL_DATA pData) {
	return (SQL_DATA)(((SQLCHAR *)(pData + 1)) + std::max(pData->StrLen_or_Ind, (SQLLEN)0));
}

SQL_DATA SQL_DATA_New(SQLSMALLINT wCType, SQLLEN StrLen_or_Ind, SQLCHAR *pValue = NULL, SQL_DATA k = NULL);
int      SQL_DATA_Free(SQL_DATA data);

typedef struct tagSQL_COLUMN_STRUCT {
	SQLCHAR     *szColumnLabel;

	SQLUSMALLINT wColumnIndex;
	SQLSMALLINT  wSQLType;			/* SQL_COLUMN_TYPE */
	/* SQLLEN       dwColumnLength;	/ * SQL_COLUMN_LENGTH */
	SQLSMALLINT  wScale;
	SQLULEN      dwPrecision;
	SQLSMALLINT  wNullable;			/* SQL_COLUMN_NULLABLE */

	SQLCHAR     *szColumnName;		/* SQL_DESC_BASE_COLUMN_NAME */
	SQLCHAR     *szTableName;		/* SQL_COLUMN_TABLE_NAME */
	SQLSMALLINT  wAutoIncrement;	/* SQL_COLUMN_AUTO_INCREMENT  */
	SQLSMALLINT  wUnsigned;			/* SQL_DESC_UNSIGNED */
	SQLSMALLINT  wUpdatable;		/* SQL_COLUMN_UPDATABLE */

	SQLSMALLINT  wBinding;			/* SQLBindCol() 유무 */
	SQLPOINTER   pColumnData;

	/* -[SQL_DATA]-------------- */
	SQLSMALLINT  wCType;
	SQLLEN       StrLen_or_Ind;
	SQLLEN       dwColumnLength;	/* SQL_COLUMN_LENGTH */
} SQL_COLUMN_STRUCT,
 *SQL_COLUMN;
#pragma pack(pop)

/* SQLTypeToC: SQL_TYPE을 C_TYPE으로 변환한다. */
SQLSMALLINT SQLTypeToC(SQLSMALLINT wSQLType, SQLSMALLINT wUnsigned = SQL_FALSE);

SQLRETURN   SQLColumnData(SQLHSTMT stmt, SQL_COLUMN pColumn, SQLPOINTER *rgbValue, SQLLEN cbValueMax, SQLLEN *StrLen_or_Ind);

/* SQLColumnTo: SQL_DATA형태의 저장가능한 버퍼형태로, 데이터를 추출한다. (stmt != SQL_NULL_HSTMT 가 아닌경우 SQLColumnData() 수행)
      UsedByte: SQL_DATA를 저장하는데 필요한 크기
*/
SQL_DATA  SQLColumnTo(SQLHSTMT stmt, SQL_COLUMN pColumn, SQLLEN *UsedByte = NULL);
int       SQLColumnCompare(SQL_DATA l_rgbValue, SQL_DATA r_rgbValue); /* SQLColumnTo()로 추출된 데이터를 비교한다. */

#include <functional>

/* std::map<.., ISQLColumnCompare> 로 사용하기 위해 정의되는 비교함수 */
struct ISQLColumnDataCompare : public std::binary_function<SQL_DATA, SQL_DATA, bool>
{
	bool operator ()(const SQL_DATA &l, const SQL_DATA &r) const {
		return (SQLColumnCompare(l, r) < 0);
	}
};

/* SQLDescribeColumn: 특정 컬럼의 정보를 얻는다. 만약, wBindColumn == SQL_TRUE인 경우 SQLBindCol()을 수행한다.
사용을 마친 정보는 SQLResetColumn()을 통해 제거한다.
*/
SQLRETURN SQLDescribeColumn(SQLHSTMT stmt,
	SQLUSMALLINT wColumnIndex, SQL_COLUMN *pColumnStruct, SQLSMALLINT wBindColumn = SQL_FALSE);
SQLRETURN SQLResetColumn(SQLHSTMT stmt, SQL_COLUMN pColumn);

/* SQLResultColumns: 결과에 대한 전체 컬럼 정보를 얻는다. 만약, wBindColumn == SQL_TRUE인 경우 SQLBindCol()을 수행한다.
사용을 마친 정보는 SQLCancelColumns()을 통해 해제한다.
*/
typedef SQL_COLUMN *SQL_COLUMNS;

SQLSMALLINT SQLResultColumns(SQLHSTMT stmt, SQL_COLUMNS *pColumns, SQLSMALLINT wBindColumn = SQL_FALSE);
SQLRETURN   SQLCancelColumns(SQLHSTMT stmt, SQL_COLUMNS pColumns);

SQL_COLUMN  SQLLabelColumns(SQL_COLUMNS pColumns, SQLCHAR *szColumnLabel); /* 컬럼명으로 정보를 얻는다. */
SQL_COLUMN  SQLMoreColumns(SQL_COLUMNS pColumns, SQLSMALLINT wColumnIndex); /* 컬럼 정보를 얻는다. */

/* SQLReviewColumns: SQLFetch()이후, 컬럼의 데이터를 모두 갱신한다.
     wExportSize == SQL_TRUE 이면, SQLColumnTo()로 추출하는 경우의 데이터 필요 크기로 처리한다.
*/
SQLRETURN   SQLReviewColumns(SQLHSTMT stmt,
	SQL_COLUMNS pColumns, SQLLEN *pUpdateSize = NULL, SQLSMALLINT wExportSize = SQL_FALSE);


/*
*/
#include <map>

/* */
struct tagSQL_ROWS_DATA_STRUCT;

namespace std {
	typedef basic_string<unsigned char> buffer;
};

typedef std::map<std::string, std::buffer> SQLRowKeySet;
typedef std::map<SQL_DATA, struct tagSQL_ROWS_STRUCT *, ISQLColumnDataCompare> SQLRowValueSet;

#define SQL_FIELD_UPDATE			(SQL_TRUE << 1)
#define SQL_FIELD_INSERT			(SQL_TRUE << 2)
#define SQL_FIELD_DELETE			(SQL_TRUE << 3)

typedef struct tagSQL_ROWS_STRUCT {
#define SQL_ROWS_GROUPS				2
	SQLSMALLINT wIsType;		/* 접근 형태 (SQL_TRUE + 0 - u.i 접근, + 1 - u.v 접근) */
	SQLSMALLINT wIsState;		/* 변경 상태 - ORIG: 0, UPDATE: 2, INSERT: 4, DELETE: 8 */
	union {
		SQLRowValueSet *i[SQL_ROWS_GROUPS];	/* SQL_TRUE + 0 (data, log) */
		SQL_DATA        v;		/* SQL_TRUE + 1 */
	} u;

	SQL_DATA                   k; /* key */
	struct tagSQL_ROWS_STRUCT *p; /* parent */
} SQL_ROWS_STRUCT,
 *SQL_ROWS;



struct SQL_CUSTOM_SPEC
{
	SQLCHAR *PREFIX_NAME;                /* 컬럼명 표현 시작 문자열 ('[') */
	SQLCHAR *SUBFIX_NAME;                /* 컬럼명 표현 완료 문자열 (']') */

	SQLCHAR *SELECT_INSERT_ID;	        /* SELECT LAST_INSERT_ID() */
	SQLSMALLINT INCREMENT_DIRECTION;	/* 값 증가 방향 */
};

extern SQL_CUSTOM_SPEC SQL_MySQL,	/* MySQL */
                       SQL_MSSQL;	/* MS-SQL */

typedef struct tagSQL_RESULT_STRUCT {
	SQLSMALLINT wUpdatable;		/* 데이터의 변경 가능 여부 */

	SQLRowKeySet    *META;		/* 저장된 메타 데이터 (컬럼 정보 등) */
	SQL_ROWS_STRUCT  ROWS;
    SQL_CUSTOM_SPEC *SPEC;
} SQL_RESULT_STRUCT,
 *SQL_RESULT;

SQL_RESULT SQLCloneResult(SQL_RESULT);

/* SQLStepRows: data의 위치로 부터, keys에 정의된 위치를 찾는다.
wFieldCount - keys의 갯수를 설정한다. (반환은, data항목에서 부터, 처리되고 남은 keys의 갯수를 반환한다.)
wIsNew - keys가 존재하는 않는 경우, 값을 생성할지에 대한 여부
lKey - 마지막 선택된 키
*/
#include <list>

typedef std::list<SQL_DATA> SQLStepKeys;

SQL_DATA  SQLStepKey(SQL_ROWS rows, SQL_DATA k); /* k 다음 키를 얻는다. */

#include "bson.h"

int SQLExportRows(SQL_ROWS rows, int depth, SQL_DATA k, bson *b); /* rows -> bson */

/* SQL_ROWS에서 keys에 접근한 경로를 반환한다. (데이터의 접근 처리)
*   - SQLUpdateRows() 접근시 내부에서 호출됨
*
*   - wIsNew != NULL 인 경우, 항목을 생성
*/
SQL_ROWS  SQLStepRows(SQL_ROWS data, SQL_DATA key, SQLSMALLINT *wIsNew = NULL, SQLStepKeys *lKey = NULL);
SQL_ROWS  SQLStepRows(SQL_ROWS data, SQL_DATA *keys,
	SQLSMALLINT *wFieldCount, SQLSMALLINT *wIsNew = NULL, SQLStepKeys *lKey = NULL);

/* 데이터를 변경한다.
*   data - 변경하고자 하는 ROWS
*   keys - 접근하는 컬럼 명
*   wFieldCount - keys에 저장된 갯수
*   v - 변경하고자 하는 데이터 (SQL_DATA_New() 된 데이터)
*
*   OUT lKey - 생성된 경로 (새로 추가된 컬럼 명)
*              keys에 접근하면서, 신규로 생성된 컬럼명
*/
SQLRETURN SQLUpdateRow(SQL_ROWS rows, SQL_DATA k, SQL_DATA v);
SQLRETURN SQLUpdateRows(SQL_ROWS data, SQL_DATA *keys, SQLSMALLINT wFieldCount, SQL_DATA v, SQLStepKeys *lKey = NULL);

/* SQLAllocResult: Result에 데이터를 저장하기 위한 초기 작업을 진행한다.
   pAlignKeys의 순서로 정렬이 되어 진다. 만약, dwRowIndex를 키로 사용하고자 하는 경우 키값을 "$"로 지정하여 사용한다.
   pColumns은 SQLResultColumns()에서 생성된 값을 사용한다.
   예)
     SQLCHAR *AlignKeys[] = {
       "userid",
       "$",
     };

     SQLAllocResult(&pRows, pColumns, AlignKeys);
*/

SQL_ROWS  SQLAllocRows(SQL_DATA k = NULL, SQL_ROWS pParentRow = NULL, SQLSMALLINT wUpdatable = SQL_FALSE);
SQLRETURN SQLPumpRows(SQL_ROWS rows, SQL_COLUMN *pColumns);
SQLRETURN SQLFreeRows(SQL_ROWS rows);

/* SQLPumpResultSet: 해당 ROW의 데이터를 ResultSet에 저장한다.
  저장되는 키값은 SQLAllowResultSet()에 지정된 pAllowKeys에 따라 저장된다. */
SQL_RESULT SQLAllocResult(SQL_COLUMNS pColumns, SQLCHAR **pAlignKeys, SQL_CUSTOM_SPEC *CUSTOM_SPEC = &SQL_MySQL);
SQLRETURN  SQLPumpResult(SQL_RESULT result, SQLROWCOUNT dwRowIndex, SQL_COLUMN *pColumns);
SQLRETURN  SQLFreeResult(SQL_RESULT result); /* 사용된 pRows를 해제한다. */

inline int SQLExportResult(SQL_RESULT result, bson *b)
{
	for (SQLRowValueSet::iterator it = result->ROWS.u.i[0]->begin();
		it != result->ROWS.u.i[0]->end(); it++)
	{
		if (SQLExportRows(it->second, 1, it->first, b) < 0)
			break;
	}
	return 0;
}

SQL_FIELD SQLFieldFromResult(SQL_RESULT result, SQLCHAR *pColumnLabel); /* 컬럼명의 존재 여부를 확인한다. */
SQL_FIELD SQLFieldFromResult(SQL_RESULT result, SQLSMALLINT wFieldIndex, SQLCHAR **szColumnLabel = NULL);

#define SQLTableResult(result)			((SQLCHAR *)SQLFieldFromResult(result, (SQLCHAR *)".TABLE"))
#define SQLAutoIncrementResult(result)	((SQLCHAR *)SQLFieldFromResult(result, (SQLCHAR *)".AUTO_INCREMENT"))
#define SQLCommitCount(result)			*((SQLLEN *)SQLFieldFromResult(result, (SQLCHAR *)".COMMIT"))

/* SQLCommitResult: pRows의 변경 내용을 데이터베이스에 적용한다. */
typedef struct tagSQL_UPDATE_DATA_STRUCT {
	SQL_DATA o; /* db에서 쿼리된 데이터 */
	SQL_DATA v; /* 변경된 데이터 */
} SQL_UPDATE_DATE_STRUCT;


struct SQLCommitCALLBACK {
	struct HASH_SQLSTR {
		size_t operator ()(const SQLCHAR *k) const { return std::hash<std::string>()(std::string((char *)k)); }
		bool operator()(const SQLCHAR *a, const SQLCHAR *b) const { return strcmp((char *)a, (char *)b) == 0; }
	};

	typedef std::unordered_map<SQLCHAR *, SQL_UPDATE_DATE_STRUCT, HASH_SQLSTR> ChangeSet;
	typedef std::unordered_map<SQLCHAR *, SQL_DATA, HASH_SQLSTR> WhereSet;

	virtual bool allowColumn(SQLCHAR *szTableName, SQLCHAR *szColumnName) { return true; }
	virtual void ChangeLog(SQLSMALLINT wChangeType, SQLCHAR *szTableName, WhereSet &where, ChangeSet &changes) = 0;
};

/* SQLCommitResult에서 자동으로 호출되어 진다. 만약, 별도의 INSERT 쿼리를 수행하는 경우에만 사용. */
SQLRETURN SQLFetchGeneratedKeys(SQLHSTMT stmt, SQL_RESULT result, SQLLEN lRowCount);

/* SQL_RESULT에 커밋할 데이터가 있는지 확인한다. */
BOOL SQLIsDirtyResult(SQL_RESULT result);

/* SQL_RESULT의 변경 내용을 szTableName에 commit한다. */
SQLRETURN SQLCommitResult(SQLHSTMT stmt, SQL_RESULT result, SQLCHAR *szTableName = NULL, SQLCommitCALLBACK *commitCallback = NULL);

/* SQLCommitResult 처리 중 auto increment된 데이터의 insert id를 반환한다. */
SQL_DATA SQLGetGeneratedKeys(SQL_RESULT result, SQLLEN *lRowCount);

/* @} */
#endif
