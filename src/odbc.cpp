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
#include "odbc.h"
#include "c++/tls.hpp"
#include <stdarg.h>
#include <malloc.h>
#include <codecvt>
#include <locale>

/*! \defgroup ODBC기반으로 데이터베이스와 입출력을 할수 있도록 지원
 *  @{
 */

SQL_CUSTOM_SPEC SQL_MySQL = { (SQLCHAR *)"`", (SQLCHAR *)"`", (SQLCHAR *)"SELECT LAST_INSERT_ID()",  1 },	/* MySQL */
                SQL_MSSQL = { (SQLCHAR *)"[", (SQLCHAR *)"]", (SQLCHAR *)"SELECT SCOPE_IDENTITY()", -1 };	/* MS-SQL */


static SQLRETURN DEFAULT_SQLErrorReport(
		SQLHANDLE handle, SQLSMALLINT fHandleType, struct SQL_DIAG_REC *pDiagRec, SQLINTEGER lCountRec)
{
	for (; (--lCountRec) >= 0; pDiagRec++)
		fprintf(stderr, " [SQL] %s %d %s\n", pDiagRec->szSqlState, pDiagRec->NativeError, pDiagRec->szErrorMsg);
	return SQL_SUCCESS;
}

struct TLS_DIAG_REC {
	SQLINTEGER           lCountRec;
	struct SQL_DIAG_REC *pDiagRec;

	TLS_DIAG_REC()
		: lCountRec(0)
		, pDiagRec(NULL) { }
};

static ThreadLocalStorage<TLS_DIAG_REC> ODBC_diagRec;

static struct {
	SQLHENV env;
	fnSQLErrorReport errorReport;
} ODBC = { NULL, DEFAULT_SQLErrorReport };

void SQLSetErrorReport(fnSQLErrorReport errorReport)
{
	ODBC.errorReport = (errorReport == NULL) ? DEFAULT_SQLErrorReport: errorReport;
}


SQLINTEGER SQLGetLastError(struct SQL_DIAG_REC **pDiagRec)
{
	if (ODBC_diagRec->lCountRec == 0)
		return 0;
	else {
		if (pDiagRec != NULL)
			(*pDiagRec) = ODBC_diagRec->pDiagRec;
	}
	return ODBC_diagRec->lCountRec;
}


SQLRETURN SQLErrorReport(SQLHANDLE handle, SQLSMALLINT fHandleType)
{
	if (handle == SQL_NULL_HANDLE)
		ODBC_diagRec->lCountRec = 0;
	else {
		SQLSMALLINT len;
		SQLINTEGER lCountRec = 0;

		SQLRETURN rc = SQLGetDiagField(fHandleType, handle, 0, SQL_DIAG_NUMBER, &lCountRec, SQL_IS_INTEGER, &len);
		if (lCountRec == 0)
			;
		else {
			SQLSMALLINT i = 0;
			struct SQL_DIAG_REC *pCurrentDiagRec;

			if (!(pCurrentDiagRec = (struct SQL_DIAG_REC *)realloc(ODBC_diagRec->pDiagRec,
						sizeof(struct SQL_DIAG_REC) * (ODBC_diagRec->lCountRec + lCountRec))))
				return -1;
			else {
				ODBC_diagRec->pDiagRec = pCurrentDiagRec;
				pCurrentDiagRec += ODBC_diagRec->lCountRec;
				do {
					rc = SQLGetDiagRec(fHandleType, handle, ++i, pCurrentDiagRec->szSqlState,
										&pCurrentDiagRec->NativeError, pCurrentDiagRec->szErrorMsg, SQL_MAX_MESSAGE_LENGTH, &len);
					if (SQL_SUCCEEDED(rc) && (len > 0))
					{
						*(pCurrentDiagRec->szErrorMsg + len) = 0;
						pCurrentDiagRec++;
					}
				} while ((rc == SQL_SUCCESS) && (i < lCountRec));
			}

			if ((ODBC_diagRec->lCountRec += (SQLINTEGER)(pCurrentDiagRec - ODBC_diagRec->pDiagRec)) > 0)
				return ODBC.errorReport(handle, fHandleType, ODBC_diagRec->pDiagRec, ODBC_diagRec->lCountRec);
		}
	}
	return SQL_SUCCESS;
}


SQLRETURN SQLInit()
{
	if (ODBC.env != NULL)
		return SQL_SUCCESS;
	else {
		SQLRETURN rc;

		SQLSetEnvAttr(SQL_NULL_HANDLE, SQL_ATTR_CONNECTION_POOLING, (SQLPOINTER)SQL_CP_ONE_PER_HENV, SQL_IS_INTEGER);
		switch (rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &ODBC.env))
		{
			case SQL_SUCCESS:
			case SQL_SUCCESS_WITH_INFO:
			{
				switch (rc = SQLSetEnvAttr(ODBC.env, SQL_ATTR_ODBC_VERSION, (void *)SQL_OV_ODBC3, SQL_IS_INTEGER))
				{
					case SQL_SUCCESS:
					case SQL_SUCCESS_WITH_INFO:
					{
						SQLSetEnvAttr(ODBC.env, SQL_ATTR_CP_MATCH, (SQLPOINTER)SQL_CP_RELAXED_MATCH, SQL_IS_INTEGER);
					} return SQL_SUCCESS;
					default: break;
				}
			} SQLFreeHandle(SQL_HANDLE_ENV, ODBC.env);
			default: break;
		}
		return rc;
	}
}

SQLRETURN SQLExit()
{
	return SQLFreeHandle(SQL_HANDLE_ENV, ODBC.env);
}


SQLRETURN SQLSetAttrs(SQLHANDLE handle, SQLSMALLINT fHandleType, ...)
{
	va_list args;

	va_start(args, fHandleType); {
		SQLINTEGER attr;
		SQLRETURN (*SQLSetCallbackAttr)(SQLHANDLE h, SQLINTEGER n, SQLPOINTER v, SQLINTEGER l) =
			(SQLRETURN(*)(SQLHANDLE, SQLINTEGER, SQLPOINTER, SQLINTEGER))
			((fHandleType == SQL_HANDLE_DBC) ? SQLSetConnectAttr :
			(fHandleType == SQL_HANDLE_STMT) ? SQLSetStmtAttr : NULL);

		if (SQLSetCallbackAttr == NULL)
			;
		else {
			while ((attr = va_arg(args, SQLINTEGER)) != SQL_NTS)
			{
				SQLPOINTER value = va_arg(args, SQLPOINTER);
				SQLINTEGER length = va_arg(args, SQLINTEGER);
				SQLRETURN rc;

				switch (rc = SQLSetCallbackAttr(handle, attr, value, length))
				{
					case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(handle, fHandleType); */
					case SQL_SUCCESS: break;
					default: {
						SQLErrorReport(handle, fHandleType);
					} return rc;
				}
			}
		}
	} va_end(args);
	return SQL_SUCCESS;
}


SQLRETURN SQLIsConnectDead(SQLHDBC dbc)
{
	SQLINTEGER dead = SQL_CD_TRUE;
	SQLINTEGER length;

_gW:switch (SQLGetConnectAttr(dbc, SQL_ATTR_CONNECTION_DEAD, &dead, sizeof(dead), &length))
	{
		case SQL_STILL_EXECUTING: goto _gW;
		case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(dbc, SQL_HANDLE_DBC); */
		case SQL_SUCCESS:
		{
			;
		} return (SQLRETURN)((dead == SQL_CD_TRUE) ? SQL_SUCCESS : SQL_ERROR);
		default: break;
	}
	return SQL_ERROR;
}


static SQLHDBC _SQLConnectEx(SQLCHAR *szDsn, SQLINTEGER connectionTimeout, SQLINTEGER loginTimeout)
{
	SQLHDBC dbc;

	switch (SQLAllocHandle(SQL_HANDLE_DBC, ODBC.env, &dbc))
	{
		case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(dbc, SQL_HANDLE_DBC); */
		case SQL_SUCCESS:
		{
			if (loginTimeout == SQL_NTS) loginTimeout = connectionTimeout;
			if (SQLSetAttrs(dbc, SQL_HANDLE_DBC,
					SQL_ATTR(CONNECTION_TIMEOUT, connectionTimeout, SQL_IS_UINTEGER),
					SQL_ATTR(LOGIN_TIMEOUT, loginTimeout, SQL_IS_UINTEGER),
#if defined(USE_SQL_ASYNC)
					SQL_ATTR(ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON, SQL_IS_INTEGER),
					SQL_ATTR(ASYNC_DBC_FUNCTIONS_ENABLE, SQL_ASYNC_DBC_ENABLE_ON, SQL_IS_INTEGER),
#endif
					SQL_NTS) != SQL_SUCCESS)
				break;
			else {
				/* SQLGetConnectOption(dbc, SQL_ODBC_CURSORS, (SQLPOINTER)SQL_CUR_USE_DRIVER); */
_gW:			switch (SQLDriverConnectA(dbc, 0, szDsn, SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT))
				{
					case SQL_STILL_EXECUTING:
					{
						;
					} goto _gW;
					case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(dbc, SQL_HANDLE_DBC); */
					case SQL_SUCCESS:
					{
						;
					} return dbc;
					default:  SQLErrorReport(dbc, SQL_HANDLE_DBC);
				}
			}
		}
		default: {
			SQLErrorReport(ODBC.env, SQL_HANDLE_ENV);
		} SQLFreeHandle(SQL_HANDLE_DBC, dbc);
	}

	return NULL;
}


static SQLRETURN _SQLDisconnectEx(SQLHDBC dbc)
{
	while (SQLDisconnect(dbc) == SQL_STILL_EXECUTING)
		;
	return SQLFreeHandle(SQL_HANDLE_DBC, dbc);
}


SQLHSTMT SQLAllocStmtEx(SQLHDBC dbc, SQLINTEGER timeOut)
{
	SQLHSTMT stmt;

	switch (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))
	{
		case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(stmt, SQL_HANDLE_STMT); */
		case SQL_SUCCESS:
		{
			if ((timeOut > 0) &&
				(SQLSetAttrs(stmt, SQL_HANDLE_STMT,
					SQL_ATTR(QUERY_TIMEOUT, timeOut, SQL_IS_UINTEGER),
					SQL_QUERY(TIMEOUT, timeOut, SQL_IS_UINTEGER),
#if defined(USE_SQL_ASYNC)
					SQL_ATTR(ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON, SQL_IS_INTEGER),
#endif
					SQL_NTS) != SQL_SUCCESS))
				break;
			else {
				return stmt;
			}
		}
		default: {
			SQLErrorReport(dbc, SQL_HANDLE_DBC);
		} SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	return SQL_NULL_HSTMT;
}

SQLRETURN SQLResetStmt(SQLHSTMT stmt)
{
	SQLRETURN rc = SQLFreeStmt(stmt, SQL_CLOSE);
	if (SQL_SUCCEEDED(rc)) rc = SQLFreeStmt(stmt, SQL_UNBIND);
	if (SQL_SUCCEEDED(rc)) rc = SQLFreeStmt(stmt, SQL_RESET_PARAMS);
	return rc;
}

SQLRETURN SQLFreeStmtEx(SQLHSTMT stmt)
{
	if (stmt == SQL_NULL_HSTMT)
		return SQL_ERROR;
	else {
		SQLResetStmt(stmt);
	} return SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}


SQLRETURN SQLExecuteEx(SQLHSTMT stmt, SQLCHAR *szSqlStr, SQL_PARAM *args, SQLPOINTER pExternalBuffer)
{
	SQLRETURN rc = SQLResetStmt(stmt) || SQLPrepareA(stmt, szSqlStr, SQL_NTS);
	if (SQL_SUCCEEDED(rc) == SQL_FALSE)
		SQLErrorReport(stmt, SQL_HANDLE_STMT);
	else {
		SQLSMALLINT NumParams = 0;

		SQLNumParams(stmt, &NumParams);
        if (args != NULL) for (SQLUSMALLINT wParamIndex = 0; (++wParamIndex) <= NumParams; args++)
        {
            rc = SQLBindParameter(stmt, wParamIndex, args->fParamType, args->fCType, args->fSqlType, args->cbColDef,
                args->ibScale, (args->StrLen_or_Ind <= SQL_DATA_AT_EXEC) ? args: args->rgbValue, args->cbValueMax, &args->StrLen_or_Ind);
			if (SQL_SUCCEEDED(rc) == SQL_FALSE)
			{
				SQLErrorReport(stmt, SQL_HANDLE_STMT);
				SQLFreeStmt(stmt, SQL_DROP);
				return rc;
			}
        }

_gW:	switch (rc = SQLExecute(stmt))
		{
			case SQL_STILL_EXECUTING: goto _gW;

			case SQL_SUCCESS_WITH_INFO: /* SQLErrorReport(stmt, SQL_HANDLE_STMT); */
			case SQL_SUCCESS: break;

			case SQL_NEED_DATA: /* BLOB와 같은 데이터 등록시 이벤트 발생 */
			{
				SQL_PARAM *sp;
				SQLINTEGER l;

				while ((rc = SQLParamData(stmt, (SQLPOINTER *)&sp)) == SQL_NEED_DATA)
				{
					SQL_BINARY_CALLBACK *pBinaryCallback = (SQL_BINARY_CALLBACK *)sp->rgbValue;

					SQLINTEGER iOffset = 0;
					SQLPOINTER pValue = NULL;
					while ((l = pBinaryCallback->NEED_DATA(sp, iOffset, pExternalBuffer, &pValue)) > 0)
					{
_gPW:					switch (rc = SQLPutData(stmt, pValue, l))
						{
							case SQL_STILL_EXECUTING: goto _gPW;
							case SQL_SUCCESS: break;
							default: SQLErrorReport(stmt, SQL_HANDLE_STMT);
							{
								;
							} return rc;
						}

						iOffset += l;
					}

					pBinaryCallback->DISPOSE(); /* 데이터 처리후 삭제 */
				}

				if (SQL_SUCCEEDED(rc)) return rc;
			}
			default: SQLErrorReport(stmt, SQL_HANDLE_STMT);
		}
	}

	return rc;
}


int SQLColumnCompare(SQL_DATA l_rgbValue, SQL_DATA r_rgbValue)
{
	if (l_rgbValue->wCType != r_rgbValue->wCType)
		return l_rgbValue->wCType - r_rgbValue->wCType;
	else {
		SQLLEN l_ValueSize = std::max(l_rgbValue->StrLen_or_Ind, (SQLLEN)0),
			r_ValueSize = std::max(r_rgbValue->StrLen_or_Ind, (SQLLEN)0);

		int delta = memcmp((SQLPOINTER)(l_rgbValue + 1),
			(SQLPOINTER)(r_rgbValue + 1), std::min(l_ValueSize, r_ValueSize));
		return (delta == 0) ? (int)(l_ValueSize - r_ValueSize) : delta;
	}
}

SQL_DATA SQLColumnTo(SQLHSTMT stmt, SQL_COLUMN pColumn, SQLLEN *UsedBytes)
{
	if ((stmt == SQL_NULL_HSTMT) ||
		(pColumn->wBinding == SQL_TRUE))
		;
	else {
		SQLPOINTER __x = NULL;
		SQLLEN __l;

		if (SQLColumnData(stmt, pColumn, &__x, 0, &__l) != SQL_SUCCESS)
			return NULL;
	}

	if (UsedBytes != NULL)
		(*UsedBytes) = sizeof(SQL_DATA_STRUCT) + std::max(pColumn->StrLen_or_Ind, (SQLLEN)0);
	return (SQL_DATA)(((pColumn->StrLen_or_Ind <= 0) || (pColumn->wBinding != SQL_NTS))
		? &pColumn->wCType : pColumn->pColumnData);
}


/* SQLExecute() 이후, 컬럼에서 데이터를 읽는다. */
SQLRETURN SQLColumnData(SQLHSTMT stmt, SQL_COLUMN pColumn, SQLPOINTER *rgbValue, SQLLEN cbValueMax, SQLLEN *StrLen_or_Ind)
{
	/* static SQLCHAR __x; */
	SQLRETURN rc = SQL_SUCCESS;

	if (pColumn->wBinding != SQL_TRUE) /* 바인딩 처리가 되지 않은 컬럼만 처리 한다. */
_gW:	switch (rc = SQLGetData(stmt, pColumn->wColumnIndex, pColumn->wCType,
			((*rgbValue) == NULL) ? (SQLPOINTER)(pColumn + 1) : (*rgbValue), cbValueMax, &pColumn->StrLen_or_Ind))
		{
_gE:		default: SQLErrorReport(stmt, SQL_HANDLE_STMT);
			{
				;
			} return rc;

			case SQL_STILL_EXECUTING: goto _gW;
			case SQL_SUCCESS_WITH_INFO: pColumn->wBinding = SQL_FALSE;
			{
				if (pColumn->StrLen_or_Ind == SQL_NULL_DATA)
					;
				else {
					SQLLEN dwBindLength = (SQLLEN)(pColumn->szTableName - (SQLCHAR *)(pColumn + 1));
					if (dwBindLength >= pColumn->StrLen_or_Ind)
						(*rgbValue) = (SQLPOINTER)(pColumn + 1);
					else {
						SQL_DATA p = (SQL_DATA)realloc(pColumn->pColumnData,
							sizeof(SQL_DATA_STRUCT) + pColumn->StrLen_or_Ind + 1);
						if (p == NULL)
							return SQL_ERROR;
						else {
							p->wCType = pColumn->wCType;
							p->StrLen_or_Ind = pColumn->StrLen_or_Ind;

							pColumn->pColumnData = (SQLPOINTER)p;
							(*rgbValue) = (SQLPOINTER)(p + 1);
						}

						pColumn->wBinding = SQL_NTS;
						dwBindLength = pColumn->StrLen_or_Ind + 1;
					}

_gWG:				switch (rc = SQLGetData(stmt, pColumn->wColumnIndex, pColumn->wCType, (*rgbValue), dwBindLength, StrLen_or_Ind))
					{
						case SQL_SUCCESS:
						case SQL_SUCCESS_WITH_INFO: break;
						case SQL_STILL_EXECUTING: goto _gWG;
						default: goto _gE;
					}
				}
			} break;
			case SQL_SUCCESS:
			{
				if (pColumn->StrLen_or_Ind == SQL_NULL_DATA)
					;
				else {
					if ((*rgbValue) == NULL)
						(*rgbValue) = (SQLPOINTER)(pColumn + 1);
				}
			} break;
		}
	else { /* 바인딩된 데이터인 경우 */
		if ((*StrLen_or_Ind) != SQL_NULL_DATA)
			(*rgbValue) = (SQLPOINTER)(pColumn + 1);
	}

	(*StrLen_or_Ind) = pColumn->StrLen_or_Ind;
	return rc;
}

/* 컬럼의 정보를 얻는다. */
SQLRETURN SQLDescribeColumn(SQLHSTMT stmt, SQLUSMALLINT wColumnIndex, SQL_COLUMN *pColumn, SQLSMALLINT wBindColumn)
{
#define MAX_TABLE_NAME_LEN			128
#define SQLBUFSIZ					2048
	if ((*pColumn) != NULL)
		;
	else {
		(*pColumn) = (struct tagSQL_COLUMN_STRUCT *)calloc(1, sizeof(struct tagSQL_COLUMN_STRUCT) + SQLBUFSIZ);
		if ((*pColumn) == NULL)
			return SQL_ERROR;
		else {

			(*pColumn)->szColumnLabel = ((((SQLCHAR *)((*pColumn) + 1)) + SQLBUFSIZ) - (SQL_MAX_COLUMN_NAME_LEN + 1));
			(*pColumn)->szColumnName = ((*pColumn)->szColumnLabel - (SQL_MAX_COLUMN_NAME_LEN + 1));
			(*pColumn)->szTableName = ((*pColumn)->szColumnName - (MAX_TABLE_NAME_LEN + 1));
		}
	}

	(*pColumn)->wColumnIndex = wColumnIndex;
	(*pColumn)->StrLen_or_Ind = SQL_NULL_DATA;
	{
		SQLLEN dwBindLength = (SQLLEN)((*pColumn)->szTableName - (SQLCHAR *)((*pColumn) + 1));

		SQLSMALLINT wColumnNameLength = 0;
		SQLRETURN rc = SQLDescribeColA(stmt, wColumnIndex, (*pColumn)->szColumnLabel, SQL_MAX_COLUMN_NAME_LEN,
			&wColumnNameLength, &(*pColumn)->wSQLType, &(*pColumn)->dwPrecision, &(*pColumn)->wScale, &(*pColumn)->wNullable);
		if (SQL_SUCCEEDED(rc) == SQL_FALSE)
			return rc;
		else {
			*((*pColumn)->szColumnLabel + wColumnNameLength) = 0;

#define SQLColAttributeEx(S, I, N, V)	do {	\
	SQLLEN __x = 0; SQLColAttributeA(S, I, N, NULL, 0, NULL, &__x); *((SQLSMALLINT *)(V)) = (SQLSMALLINT)__x;	\
} while (0)

			SQLColAttributeEx(stmt, wColumnIndex, SQL_COLUMN_AUTO_INCREMENT, &(*pColumn)->wAutoIncrement);
			SQLColAttributeEx(stmt, wColumnIndex, SQL_DESC_UNSIGNED, &(*pColumn)->wUnsigned);
			SQLColAttributeEx(stmt, wColumnIndex, SQL_COLUMN_UPDATABLE, &(*pColumn)->wUpdatable);
			SQLColAttributeA(stmt, wColumnIndex, SQL_COLUMN_LENGTH, NULL, 0, NULL, &(*pColumn)->dwColumnLength);
#undef SQLColAttributeEx

			(*pColumn)->wCType = SQLTypeToC((*pColumn)->wSQLType, (*pColumn)->wUnsigned);
#define SQLColAttributeEx(S, I, N, V, M)	do {	\
	SQLSMALLINT __l = 0; SQLColAttributeA(S, I, N, (SQLPOINTER)V, (M), &__l, NULL); *((V) + __l) = 0;	\
} while (0)
			SQLColAttributeEx(stmt, wColumnIndex, SQL_COLUMN_TABLE_NAME, (*pColumn)->szTableName, MAX_TABLE_NAME_LEN);
			SQLColAttributeEx(stmt, wColumnIndex, SQL_DESC_BASE_COLUMN_NAME, (*pColumn)->szColumnName, SQL_MAX_COLUMN_NAME_LEN);
#undef SQLColAttributeEx
			if ((wBindColumn == SQL_FALSE) ||
				((*pColumn)->dwColumnLength > dwBindLength))
				(*pColumn)->wBinding = SQL_FALSE;
			else {
				rc = SQLBindCol(stmt, wColumnIndex, 
                    (*pColumn)->wCType, (SQLPOINTER)((*pColumn) + 1), dwBindLength, &(*pColumn)->StrLen_or_Ind);
				if (((*pColumn)->wBinding = SQL_SUCCEEDED(rc)) == SQL_FALSE)
					SQLErrorReport(stmt, SQL_HANDLE_STMT);
			}
			/*
			switch (wDataType)
			{
			case SQL_DECIMAL:
			case SQL_NUMERIC:

			case SQL_BIT:
			case SQL_TINYINT:
			case SQL_SMALLINT:
			case SQL_INTEGER:
			case SQL_BIGINT:

			case SQL_FLOAT:
			case SQL_REAL:
			case SQL_DOUBLE:

			case SQL_TIME:
			case SQL_DATE:
			case SQL_TIMESTAMP:

			case SQL_TYPE_TIME:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIMESTAMP:

			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARCHAR:
			case SQL_WLONGVARCHAR:
			case SQL_LONGVARBINARY:

			case SQL_CHAR:
			case SQL_VARCHAR:

			case SQL_WCHAR:
			case SQL_WVARCHAR:
			}
			*/
		}
	}

	return SQL_SUCCESS;
#undef SQL_BUFSIZ
#undef MAX_TABLE_NAME_LEN
}

SQLRETURN SQLResetColumn(SQLHSTMT stmt, SQL_COLUMN pColumn)
{
	SQLRETURN rc = SQL_SUCCESS;
	if (pColumn == NULL)
		;
	else {
		if (pColumn->pColumnData != NULL) free(pColumn->pColumnData);
		if ((pColumn->wBinding == SQL_TRUE) && (stmt != SQL_NULL_HSTMT))
			rc = SQLBindCol(stmt, pColumn->wColumnIndex, SQL_C_DEFAULT, NULL, 0, NULL);
		free(pColumn);
	}
	return rc;
}

/*
*/
struct ISQLColumnLabelCompare : public std::binary_function<SQL_COLUMN, SQL_COLUMN, bool>
{
	bool operator ()(const SQL_COLUMN &l, const SQL_COLUMN &r) const {
		int delta = strcmp((const char *)l->szColumnLabel, (const char *)r->szColumnLabel);
		if (delta != 0)
			;
		else {
			if ((l->wColumnIndex > 0) && (r->wColumnIndex > 0))
				return (l->wColumnIndex < r->wColumnIndex);
		} return (delta < 0);
	}
};

/* SQLExecute()이후 접근가능한 컬럼 정보를 확인한다. */
SQLSMALLINT SQLResultColumns(SQLHSTMT stmt, SQL_COLUMNS *pColumns, SQLSMALLINT wBindColumn)
{
	SQLSMALLINT wColumnCount = 0;

	SQLNumResultCols(stmt, &wColumnCount);
	if (wColumnCount == 0)
		;
	else {
		/*
		+---+------- ~~~ -------+------- ~~~ ------+
		| N | NO-SORTING COLUMN |  SORTING COLUMN  |
		+---+------- ~~~ -------+------- ~~~ ------+
		*/
		SQLSMALLINT *pColumnCount = (SQLSMALLINT *)
			calloc(1, sizeof(SQLSMALLINT) + ((((size_t)wColumnCount) << 1) * sizeof(SQL_COLUMN)));
		if (pColumnCount == NULL)
			return SQL_ERROR;
		else {
			(*pColumns) = (SQL_COLUMNS)(pColumnCount + 1);
			SQL_COLUMNS pSortColumns = (*pColumns) + wColumnCount;

			*(pColumnCount + 0) = wColumnCount;
			for (SQLSMALLINT wColumnIndex = 0; wColumnIndex < wColumnCount; wColumnIndex++)
			{
				if (SQLDescribeColumn(stmt, wColumnIndex + 1, &(*pColumns)[wColumnIndex], wBindColumn) == SQL_SUCCESS)
					switch ((*pColumns)[wColumnIndex]->wBinding)
					{
                        case SQL_FALSE: wBindColumn = SQL_FALSE;
                        default: break;
					}
				else {
					SQLErrorReport(stmt, SQL_HANDLE_STMT);
					return SQL_ERROR;
				}

				pSortColumns[wColumnIndex] = (*pColumns)[wColumnIndex];
			}

			std::sort(pSortColumns, pSortColumns + wColumnCount, ISQLColumnLabelCompare());
		}
	}

	return wColumnCount;
}

/* SQLExecute()이후 읽어들일 데이터에 대한 컬럼 정보를 확인한다. */
SQLRETURN SQLReviewColumns(SQLHSTMT stmt, SQL_COLUMNS pColumns, SQLLEN *pUpdateSize, SQLSMALLINT wExportSize)
{
	if (pColumns == NULL)
		return SQL_NTS;
	else {
		SQLSMALLINT *pColumnCount = ((SQLSMALLINT *)pColumns) - 1;
		SQLLEN dwUpdateSize = 0;

		if (wExportSize != SQL_FALSE) wExportSize = sizeof(SQL_DATA_STRUCT);
		for (SQLUSMALLINT wColumnIndex = 0; wColumnIndex < (*pColumnCount); wColumnIndex++)
		{
			SQLPOINTER __x = NULL;
			SQLLEN l = 0;

			SQLRETURN rc = SQLColumnData(stmt, pColumns[wColumnIndex], &__x, 0, &l);
			if (SQL_SUCCEEDED(rc) == SQL_FALSE)
				return rc;
			else {
				dwUpdateSize += (l + wExportSize);
			}
		}

		if (pUpdateSize != NULL) (*pUpdateSize) = dwUpdateSize;
	} return SQL_SUCCESS;
}

/* SQLExecute()이후 연결된 컬럼 정보에서 szColumnLabel의 컬럼을 얻는다. */
SQL_COLUMN SQLLabelColumns(SQL_COLUMNS pColumns, SQLCHAR *szColumnLabel)
{
	if (pColumns == NULL)
		return NULL;
	else {
		SQLSMALLINT *pColumnCount = ((SQLSMALLINT *)pColumns) - 1;
		SQL_COLUMN *pSortBegin = (pColumns + (*pColumnCount)),
			       *pSortEnd = pSortBegin + (*pColumnCount);

		ISQLColumnLabelCompare __compare;
		struct tagSQL_COLUMN_STRUCT __k = { szColumnLabel, 0, };
		SQL_COLUMN *pColumn = std::lower_bound(pSortBegin, pSortEnd, &__k, __compare);
		return ((pColumn != pSortEnd) && !__compare(&__k, *pColumn)) ? *pColumn : NULL;
	}
}

/* SQLExecute()이후 연결된 컬럼 정보에서 wColumnIndex의 정보를 얻는다. */
SQL_COLUMN SQLMoreColumns(SQL_COLUMNS pColumns, SQLSMALLINT wColumnIndex)
{
	if (pColumns == NULL)
		return NULL;
	else {
		SQLSMALLINT *pColumnCount = ((SQLSMALLINT *)pColumns) - 1;

		if ((*pColumnCount) < wColumnIndex)
			return NULL;
	}

	return pColumns[wColumnIndex - 1];
}

/* SQLExecute()이후 연결된 컬림 정보를 제거한다. */
SQLRETURN SQLCancelColumns(SQLHSTMT stmt, SQL_COLUMNS pColumns)
{
	if (pColumns == NULL)
		;
	else {
		SQLSMALLINT *pColumnCount = ((SQLSMALLINT *)pColumns) - 1;

		if (stmt != SQL_NULL_HANDLE)
			while ((--(*pColumnCount)) >= 0)
			{
				SQLResetColumn(stmt, pColumns[(*pColumnCount)]);
			}

		free(pColumnCount);
	}
	return SQL_SUCCESS /* SQLFreeStmt(stmt, SQL_UNBIND) */;
}

/* SQL자료형을 C자료형으로 변환한다. */
SQLSMALLINT SQLTypeToC(SQLSMALLINT wSQLType, SQLSMALLINT wUnsigned)
{
	switch (wSQLType)
	{
		case SQL_BIT: return SQL_C_BIT;

		case SQL_TINYINT: return (wUnsigned == SQL_TRUE) ? SQL_C_UTINYINT : SQL_C_STINYINT;
		case SQL_SMALLINT: return (wUnsigned == SQL_TRUE) ? SQL_C_USHORT : SQL_C_SSHORT;
		case SQL_INTEGER: return (wUnsigned == SQL_TRUE) ? SQL_C_ULONG : SQL_C_SLONG;
		case SQL_BIGINT: return (wUnsigned == SQL_TRUE) ? SQL_C_UBIGINT : SQL_C_SBIGINT;

		case SQL_CHAR: return SQL_C_CHAR;
		case SQL_VARCHAR: return SQL_C_CHAR;
		case SQL_LONGVARCHAR: return SQL_C_CHAR;

		case SQL_WCHAR: return SQL_C_WCHAR;
		case SQL_WVARCHAR: return SQL_C_WCHAR;
        case SQL_WLONGVARCHAR: return SQL_C_WCHAR;

		case SQL_BINARY: return SQL_C_BINARY;
		case SQL_LONGVARBINARY: return SQL_C_BINARY;

		case SQL_TYPE_TIME: return SQL_C_TYPE_TIME;
		case SQL_TYPE_DATE: return SQL_C_TYPE_DATE;
		case SQL_TYPE_TIMESTAMP: return SQL_C_TYPE_TIMESTAMP;

		case SQL_TIME: return SQL_C_TYPE_TIME;
		case SQL_DATE: return SQL_C_TYPE_DATE;
		case SQL_TIMESTAMP: return SQL_C_TYPE_TIMESTAMP;

		case SQL_REAL: return SQL_C_FLOAT;
		case SQL_FLOAT: return SQL_C_DOUBLE;
		case SQL_DOUBLE: return SQL_C_DOUBLE;
		case SQL_DECIMAL: return SQL_C_CHAR;
		case SQL_NUMERIC: return SQL_C_CHAR;
		default: break;
	}

	return SQL_C_CHAR;
}

#if 0
static int fnSQLDataCompare(const void *data, int data_length, const void *value, int value_length)
{
	SQL_DATA l_data = (SQL_DATA)data;
	SQL_DATA r_data = (SQL_DATA)value;
	int delta;

	while ((delta = SQLColumnCompare(l_data, r_data)) == 0)
	{
		l_data = (SQL_DATA)(((SQLCHAR *)(l_data + 1)) + max(l_data->StrLen_or_Ind, 0));
		r_data = (SQL_DATA)(((SQLCHAR *)(r_data + 1)) + max(r_data->StrLen_or_Ind, 0));
		{
			int l_left = max(data_length - (int)(((char *)l_data) - ((char *)data)), 0);
			int r_left = max(value_length - (int)(((char *)r_data) - ((char *)value)), 0);
			if ((l_left == 0) || (r_left == 0))
				return l_left - r_left;
		}
	}

	return delta;
}
#endif

#pragma pack(push, 1)
typedef struct tagSQL_ALIGN_FIELD_STRUCT {
	SQLSMALLINT wColumnIndex;

	SQLSMALLINT wCType;
	SQLCHAR    *szColumnLabel;
} SQL_ALIGN_FIELD_STRUCT,
*SQL_ALIGN_FIELD;
#pragma pack(pop)

#define SQL_ROW_FREE(IT)		do {	\
	SQL_DATA_Free((IT)->first);	\
	SQLFreeRows((IT)->second);	\
	delete (IT)->second;	\
} while (0)

SQLRETURN SQLFreeRows(SQL_ROWS rows)
{
	switch (rows->wIsType)
	{
		case SQL_TRUE + 1: SQL_DATA_Free(rows->u.v); break;
		case SQL_TRUE + 0: for (SQLSMALLINT wI = 0; wI < SQL_ROWS_GROUPS; wI++)
		{
			if (rows->u.i[wI] == NULL)
				;
			else {
				SQLRowValueSet *entry = rows->u.i[wI];

				for (SQLRowValueSet::iterator it = entry->begin(); it != entry->end(); it++)
					SQL_ROW_FREE(it);
				delete entry;
				rows->u.i[wI] = NULL;
			}
		} break;
		default: return SQL_ERROR;
	}

	rows->wIsType = SQL_FALSE;
	return SQL_SUCCESS;
}

SQLRETURN SQLFreeResult(SQL_RESULT R)
{
	if (R == NULL)
		return SQL_ERROR;
	else {
		delete R->META;
		SQLFreeRows(&R->ROWS);

		delete R;
	}
	return SQL_SUCCESS;
}


SQL_DATA SQL_DATA_New(SQLSMALLINT wCType, SQLLEN StrLen_or_Ind, SQLCHAR *pValue, SQL_DATA k)
{
	if (k != NULL)
		;
	else {
		SQLLEN l = std::max(StrLen_or_Ind, (SQLLEN)0);

		if (!(k = (SQL_DATA)calloc(1, (sizeof(SQL_DATA_STRUCT) + (l + sizeof(wchar_t))) + sizeof(uintptr_t))))
			return NULL;
		else {
			*((uintptr_t *)(((char *)(k + 1)) + (l + sizeof(wchar_t)))) = (uintptr_t)k;
		}
		k->dwColumnLength = l;
	}

	k->wCType = wCType;
    k->StrLen_or_Ind = StrLen_or_Ind;
	if ((pValue != NULL) && (StrLen_or_Ind > 0))
		memcpy(k + 1, pValue, StrLen_or_Ind);
	return k;
}

int SQL_DATA_Free(SQL_DATA data)
{
	if (data == NULL)
		;
	else {
		uintptr_t *p = ((uintptr_t *)(((char *)(data + 1)) + (data->dwColumnLength + sizeof(wchar_t))));
		if (*(p + 0) == (uintptr_t)data)
		{
			free(data);
			return 0;
		}
	}
	return -1;
}

static size_t SQLAlignFieldSize(SQLCHAR **pAlignKeys)
{
	size_t dwAlignSize = 0;
	for (SQLSMALLINT wI = 0; ; wI++)
	{
		dwAlignSize += sizeof(SQL_ALIGN_FIELD_STRUCT);
		if (pAlignKeys[wI] == NULL)
			break;
		else {
			dwAlignSize += (strlen((char *)pAlignKeys[wI]) + 1);
		}
	}
	return dwAlignSize;
}


SQL_RESULT SQLAllocResult(SQL_COLUMNS pColumns, SQLCHAR **pAlignKeys, SQL_CUSTOM_SPEC *CUSTOM_SPEC)
{
	try {
		SQL_RESULT R = new SQL_RESULT_STRUCT;

		R->wUpdatable = 0;
		R->META = new SQLRowKeySet();

		SQLRowKeySet::iterator f_it = R->META->insert(
			SQLRowKeySet::value_type(".KEYS", std::buffer(SQLAlignFieldSize(pAlignKeys) + sizeof(SQLSMALLINT), 0))
		).first;

		SQLSMALLINT *wFieldCount = (SQLSMALLINT *)f_it->second.data();
		SQL_ALIGN_FIELD pAlignFields = (SQL_ALIGN_FIELD)(wFieldCount + 1);

		(*wFieldCount) = 0;
		for (SQL_ALIGN_FIELD pAlignKey = pAlignFields; ; (*wFieldCount)++)
		{
			pAlignKey->wCType = SQL_C_DEFAULT;
			pAlignKey->szColumnLabel = NULL;
			if (pAlignKeys[(*wFieldCount)] != NULL)
				pAlignKey->wColumnIndex = 0;
			else {
				pAlignKey->wColumnIndex = SQL_NTS;
				break;
			}

			pAlignKey++;
		}

		SQLCHAR *szTableName = NULL;
		SQLINTEGER iNameOffset = sizeof(SQL_ALIGN_FIELD_STRUCT) * ((*wFieldCount) + 1);
		SQLSMALLINT *pColumnCount = ((SQLSMALLINT *)pColumns) - 1;
		for (SQLUSMALLINT wColumnIndex = 0; wColumnIndex < (*pColumnCount); wColumnIndex++)
		{
			SQL_COLUMN pColumn = pColumns[wColumnIndex];

			SQLRowKeySet::iterator it = R->META->find((char *)pColumn->szColumnLabel);
			if (it != R->META->end())
				throw __LINE__;
			else {
				it = R->META->insert(
					SQLRowKeySet::value_type((char *)pColumn->szColumnLabel, std::buffer(sizeof(SQL_FIELD_STRUCT), 0))
				).first;
				{
					SQL_FIELD pAlign = (SQL_FIELD)it->second.data();

					pAlign->wAlignIndex = SQL_NTS;
					for (SQLSMALLINT wAlignIndex = 0; pAlignKeys[wAlignIndex] != NULL; wAlignIndex++)
					{
						if (pAlignFields[wAlignIndex].wColumnIndex > 0);
						else if (strcmp((char *)pColumn->szColumnLabel, (char *)pAlignKeys[wAlignIndex]) == 0)
                        {
                            SQL_ALIGN_FIELD pAlignKey = pAlignFields + wAlignIndex;
                            SQLINTEGER wKeySize = (SQLINTEGER)strlen((char *)pAlignKeys[wAlignIndex]) + 1;

                            pAlign->wAlignIndex = wAlignIndex;

                            pAlignKey->wCType = pColumn->wCType;
                            pAlignKey->wColumnIndex = wColumnIndex + 1;
                            pAlignKey->szColumnLabel = (SQLCHAR *)
                                memcpy(((SQLCHAR *)pAlignFields) + iNameOffset, pAlignKeys[wAlignIndex], wKeySize);
                            iNameOffset += wKeySize;

                            R->wUpdatable++;
                            break;
                        }
					}

					pAlign->dwPrecision = pColumn->dwPrecision;
					pAlign->dwColumnLength = pColumn->dwColumnLength;
					pAlign->wSQLType = pColumn->wSQLType;
					pAlign->wCType = pColumn->wCType;
					pAlign->wScale = pColumn->wScale;
					pAlign->wUpdatable = pColumn->wUpdatable;
					pAlign->wAutoIncrement = pColumn->wAutoIncrement;
				}

				if (szTableName == NULL) szTableName = pColumn->szTableName;
				else if (szTableName != (SQLCHAR *)SQL_NTS)
				{
					if (strcmp((char *)szTableName, (char *)pColumn->szTableName) != 0)
						szTableName = (SQLCHAR *)SQL_NTS;
				}
			}

			if (pColumn->wAutoIncrement)
				R->META->insert(SQLRowKeySet::value_type(".AUTO_INCREMENT", pColumn->szColumnLabel));
		}

		if ((szTableName != NULL) && 
				(szTableName != (SQLCHAR *)SQL_NTS) && (*(szTableName + 0) != 0))
		{
			static SQLCHAR *FIELD_TABLE_NAME = (SQLCHAR *)".TABLE";

			R->META->insert(
				SQLRowKeySet::value_type((char *)FIELD_TABLE_NAME, std::buffer((unsigned char *)szTableName))
			);
		}

		R->ROWS.k = NULL;
		R->ROWS.p = NULL;
		R->ROWS.u.i[0] = new SQLRowValueSet();
		R->ROWS.u.i[1] = (R->wUpdatable > 0) ? new SQLRowValueSet() : NULL;
        R->ROWS.wIsState = 0;
		R->ROWS.wIsType = SQL_TRUE + 0;

        R->SPEC = CUSTOM_SPEC;
		return R;
	}
	catch (...) { }
	return NULL;
}


/* */
static SQL_ROWS SQLCloneRows(SQL_ROWS rows, SQL_ROWS f)
{
	SQL_DATA k;

	if ((k = SQL_DATA_New(f->k->wCType, f->k->StrLen_or_Ind, (SQLCHAR *)(f->k + 1))) != NULL)
		try {
			SQLRowValueSet::iterator it =
				rows->u.i[0]->insert(SQLRowValueSet::value_type(k, new SQL_ROWS_STRUCT())).first;

			it->second->wIsState = f->wIsState;
			it->second->p = rows;
			it->second->k = k;

			if ((it->second->wIsType = f->wIsType) == (SQL_TRUE + 1))
				it->second->u.v = SQL_DATA_New(f->u.v->wCType, f->u.v->StrLen_or_Ind, (SQLCHAR *)(f->u.v + 1));
			else {
				it->second->u.i[0] = new SQLRowValueSet();
				it->second->u.i[1] = ((f->u.i[1] != NULL) ? new SQLRowValueSet() : NULL);
				for (SQLRowValueSet::iterator v_it = f->u.i[0]->begin(); v_it != f->u.i[0]->end(); v_it++)
					if (SQLCloneRows(it->second, v_it->second) == NULL)
					{
						break;
					}
			}
			return it->second;
		}
		catch (...) { SQL_DATA_Free(k); }
	return NULL;
}


SQL_RESULT SQLCloneResult(SQL_RESULT pSource)
{
	try {
		SQL_RESULT pClone = new SQL_RESULT_STRUCT;

		pClone->META = new SQLRowKeySet();
		for (SQLRowKeySet::iterator m_it = pSource->META->begin(); m_it != pSource->META->end(); m_it++)
			pClone->META->insert(SQLRowKeySet::value_type(m_it->first, m_it->second));

		pClone->ROWS.k = NULL;
		pClone->ROWS.p = NULL;
		pClone->ROWS.u.i[0] = new SQLRowValueSet();
		pClone->ROWS.u.i[1] = ((pClone->wUpdatable = pSource->wUpdatable) > 0) ? new SQLRowValueSet() : NULL;
		pClone->ROWS.wIsType = SQL_TRUE + 0;
		for (SQLRowValueSet::iterator
				v_it = pSource->ROWS.u.i[0]->begin(); v_it != pSource->ROWS.u.i[0]->end(); v_it++)
			SQLCloneRows(&pClone->ROWS, v_it->second);
		return pClone;
	}
	catch (...) { return NULL; }
}


SQL_ROWS SQLAllocRows(SQL_DATA k, SQL_ROWS pParentRow, SQLSMALLINT wUpdatable)
{
	SQL_ROWS R;

	if (pParentRow == NULL) R = new SQL_ROWS_STRUCT();
	else if (pParentRow->wIsType != (SQL_TRUE + 0)) return NULL;
	else {
		SQLRowValueSet::iterator it = pParentRow->u.i[0]->find(k);
		if (it == pParentRow->u.i[0]->end())
			pParentRow->u.i[0]->insert(SQLRowValueSet::value_type(k, R = new SQL_ROWS_STRUCT()));
		else {
			SQL_DATA_Free(k);
			return it->second;
		}

		wUpdatable = (pParentRow->u.i[1] != NULL);
	}

	R->k = k;
	R->p = pParentRow;
	R->u.i[0] = new SQLRowValueSet();
	R->u.i[1] = (wUpdatable > 0) ? new SQLRowValueSet() : NULL;
	R->wIsType = SQL_TRUE + 0;
	return R;
}

/* SQLExecute()처리 이후, 하나의 ROW을 읽어 들인다. */
SQLRETURN SQLPumpRows(SQL_ROWS rows, SQL_COLUMN *pColumns)
{
	SQLUSMALLINT wColumnIndex = 0;
	SQLSMALLINT  wColumnCount = *(((SQLSMALLINT *)pColumns) - 1);

	do {
		SQL_COLUMN pColumn = pColumns[wColumnIndex];

		SQL_DATA k = SQL_DATA_New(SQL_C_CHAR, strlen((const char *)pColumn->szColumnLabel), pColumn->szColumnLabel);
		if (k == NULL)
			return SQL_ERROR;
		else {
			SQLLEN dwUsedBytes;
			SQL_DATA v = SQLColumnTo(SQL_NULL_HSTMT, pColumn, &dwUsedBytes);

		    SQLRowValueSet::iterator it = rows->u.i[0]->find(k);
			if (it != rows->u.i[0]->end())
				SQL_DATA_Free(k);
			else {
				// fprintf(stderr, " * PUMP.%d: '%s'\n", wColumnIndex, std::string((char *)(k + 1), k->StrLen_or_Ind).c_str());
				try {
					it = rows->u.i[0]->insert(SQLRowValueSet::value_type(k, new SQL_ROWS_STRUCT())).first;

					it->second->wIsState = SQL_FALSE;
					it->second->wIsType = SQL_TRUE + 1;
					it->second->p = rows;
					it->second->k = k;
				}
				catch (...) { SQL_DATA_Free(k); return SQL_ERROR; }

				if (v->StrLen_or_Ind == 0)
					switch (v->wCType)
					{
						case SQL_C_TYPE_TIME: v->StrLen_or_Ind = sizeof(SQL_TIME_STRUCT); break;
						case SQL_C_TYPE_DATE: v->StrLen_or_Ind = sizeof(SQL_DATE_STRUCT); break;
						case SQL_C_TYPE_TIMESTAMP: v->StrLen_or_Ind = sizeof(SQL_TIMESTAMP_STRUCT); break;
						default: break;
					}

				it->second->u.v = SQL_DATA_New(v->wCType, v->StrLen_or_Ind, (SQLCHAR *)(v + 1));
			}
		}
	} while ((++wColumnIndex) < wColumnCount);
	return SQL_SUCCESS;
}


SQLRETURN SQLPumpResult(SQL_RESULT R, SQLROWCOUNT dwRowIndex, SQL_COLUMN *pColumns)
{
	SQL_ROWS rows = &R->ROWS;
	{
		SQLRowKeySet::iterator f_it = R->META->find(".KEYS");
		if (f_it == R->META->end()) return SQL_ERROR;

		SQL_ALIGN_FIELD pAlignFields = (SQL_ALIGN_FIELD)(((SQLSMALLINT *)f_it->second.data()) + 1);
		for (SQLSMALLINT wKeyIndex = 0; ; wKeyIndex++)
		{
			SQLSMALLINT wColumnIndex = pAlignFields[wKeyIndex].wColumnIndex;

			if (wColumnIndex == SQL_NTS)
				break;
			else {
				SQL_DATA k = NULL;
				SQLRowValueSet::iterator it;
				SQLSMALLINT wIsCopy = SQL_FALSE;

				if (wColumnIndex == 0)
					*((SQLROWCOUNT *)((k = SQL_DATA_New(SQL_C_UBIGINT, sizeof(uint64_t), NULL)) + 1)) = dwRowIndex;
				else {
					SQLLEN dwUsedBytes;
					k = SQLColumnTo(SQL_NULL_HSTMT, pColumns[wColumnIndex - 1], &dwUsedBytes);
					if ((k == NULL) || (dwUsedBytes >= INT32_MAX))
						return SQL_ERROR;
					else {
						wIsCopy = SQL_TRUE;
						if (k->StrLen_or_Ind == 0)
							switch (k->wCType)
							{
								case SQL_C_TYPE_TIME: k->StrLen_or_Ind = sizeof(SQL_TIME_STRUCT); break;
								case SQL_C_TYPE_DATE: k->StrLen_or_Ind = sizeof(SQL_DATE_STRUCT); break;
								case SQL_C_TYPE_TIMESTAMP: k->StrLen_or_Ind = sizeof(SQL_TIMESTAMP_STRUCT); break;
								default: break;
							}
					}
				}

				if ((it = rows->u.i[0]->find(k)) != rows->u.i[0]->end())
					((wIsCopy == SQL_TRUE) ? 0 : SQL_DATA_Free(k));
				else {
					if ((wIsCopy == SQL_TRUE) &&
						((k = SQL_DATA_New(k->wCType, k->StrLen_or_Ind, (SQLCHAR *)(k + 1))) == NULL))
						return SQL_ERROR;
					else {
						try {
							it = rows->u.i[0]->insert(SQLRowValueSet::value_type(k, new SQL_ROWS_STRUCT())).first;

							it->second->wIsState = SQL_FALSE;
							it->second->wIsType = SQL_TRUE + 0;
							it->second->p = rows;
							it->second->k = k;
						}
						catch (...) { SQL_DATA_Free(k); return SQL_ERROR; }
						it->second->u.i[0] = new SQLRowValueSet();
						it->second->u.i[1] = ((R->wUpdatable > 0) ? new SQLRowValueSet() : NULL);
					}
				} rows = it->second;
				if (wColumnIndex == SQL_NTS)
					break;
			}
		}
	}

	return SQLPumpRows(rows, pColumns);
}


SQL_FIELD SQLFieldFromResult(SQL_RESULT R, SQLCHAR *pColumnLabel)
{
	SQLRowKeySet::iterator f_it = R->META->find((char *)pColumnLabel);
	return (f_it == R->META->end()) ? NULL : (SQL_FIELD)f_it->second.data();
}


SQL_FIELD SQLFieldFromResult(SQL_RESULT R, SQLSMALLINT wFieldIndex, SQLCHAR **szColumnLabel)
{
	SQLRowKeySet::iterator f_it = R->META->find(".KEYS");
	if (f_it == R->META->end())
		;
	else {
		SQLSMALLINT *wFieldCount = (SQLSMALLINT *)f_it->second.data();
		if ((*wFieldCount) < wFieldIndex)
			;
		else {
			SQL_ALIGN_FIELD pAlignField = ((SQL_ALIGN_FIELD)(wFieldCount + 1)) + wFieldIndex;
			if (pAlignField->wColumnIndex > 0)
			{
				if (szColumnLabel != NULL)
					(*szColumnLabel) = pAlignField->szColumnLabel;
				return SQLFieldFromResult(R, pAlignField->szColumnLabel);
			}
			else if (pAlignField->wColumnIndex == 0) /* ROW COUNT */
			{
				static SQL_FIELD_STRUCT __x = {
					SQL_NTS,
					SQL_C_UBIGINT,
					SQL_NTS,
					SQL_MAX_COLUMN_NAME_LEN,
					0,
					0,
					SQL_FALSE,
				};

				if (szColumnLabel != NULL)
					(*szColumnLabel) = NULL;
				return &__x;
			}
			else if (pAlignField->wColumnIndex == SQL_NTS) /* 접근위치가 컬럼명에 대한 접근인 경우 */
			{
				/*
				SQLSMALLINT  wAlignIndex;
				SQLSMALLINT  wCType;
				SQLSMALLINT  wSQLType;
				SQLLEN       dwColumnLength;
				SQLSMALLINT  wScale;
				SQLULEN      dwPrecision;
				SQLSMALLINT  wUpdatable;
				*/
				static SQL_FIELD_STRUCT __x = {
					SQL_NTS,
					SQL_C_CHAR,
					SQL_NTS,
					SQL_MAX_COLUMN_NAME_LEN,
					0,
					0,
					SQL_FALSE,
				};

				if (szColumnLabel != NULL)
					(*szColumnLabel) = NULL;
				return &__x;
			}
		}
	}
	return NULL; /* 정렬되는 키의 범위를 넘어가는 경우 */
}

#if 0
/* SQLIsValueField: wFieldIndex가 컬럼 데이터를 저장하고 있는 위치인지 확인한다.
SQLRETURN >= SQL_TRUE  데이터 저장 위치 (> SQL_TRUE - 이전 항목에 AlignKey가 존재)
== SQL_FALSE Key 위치
*/
SQLRETURN SQLIsValueField(SQL_RESULT *pRows, SQLSMALLINT wFieldIndex)
{
	SQLRowKeySet::iterator f_it = (*pRows)->META->find(".KEYS");
	if (f_it == (*pRows)->META->end())
		return SQL_FALSE;
	else {
		SQLSMALLINT *wFieldCount = (SQLSMALLINT *)f_it->second.data();

		if ((*wFieldCount) > wFieldIndex) return SQL_FALSE;
		else return (wFieldIndex - (*wFieldCount)) + SQL_TRUE;
	}
}
#endif


SQL_DATA SQLStepKey(SQL_ROWS rows, SQL_DATA k)
{
	if (rows->wIsType != (SQL_TRUE + 0)) return NULL;
	{
		SQLRowValueSet::iterator it;
		
		if ((k == NULL) || (k->StrLen_or_Ind == 0)) it = rows->u.i[0]->begin();
		else if ((it = rows->u.i[0]->lower_bound(k)) == rows->u.i[0]->end())
			return NULL;
		else {
			if (SQLColumnCompare(it->first, k) > 0) 
				return it->first;
			it++;
		}
		return (it == rows->u.i[0]->end()) ? NULL : it->first;
	}
}

SQL_ROWS SQLStepRows(SQL_ROWS rows, SQL_DATA k, SQLSMALLINT *wIsNew, SQLStepKeys *lKey)
{
	if (rows->wIsType != (SQL_TRUE + 0))
		return NULL;
	else {
		SQLRowValueSet::iterator it = rows->u.i[0]->find(k);

		if (it != rows->u.i[0]->end())
			;
		else if ((wIsNew == NULL) || (rows->u.i[1] == NULL)) return NULL;
		else {
			if ((k = SQL_DATA_New(k->wCType, k->StrLen_or_Ind, (SQLCHAR *)(k + 1))) == NULL)
				return NULL;
			else {
				try {
					it = rows->u.i[0]->insert(SQLRowValueSet::value_type(k, new SQL_ROWS_STRUCT())).first;
				}
				catch (...) { SQL_DATA_Free(k); return NULL; }

				it->second->wIsType = SQL_TRUE + 0;
				it->second->wIsState = SQL_FIELD_INSERT;
				it->second->p = rows;
				it->second->k = k;
				it->second->u.i[0] = new SQLRowValueSet();
				it->second->u.i[1] = ((rows->u.i[1] != NULL) ? new SQLRowValueSet() : NULL);
			}

			(*wIsNew)++;
		} rows = it->second;
		if (lKey != NULL) lKey->push_back(it->first);
	}
	return rows;
}

SQL_ROWS SQLStepRows(SQL_ROWS rows, SQL_DATA *keys, SQLSMALLINT *wFieldCount, SQLSMALLINT *wIsNew, SQLStepKeys *lKey)
{
	for (SQLSMALLINT wKeyIndex = 0; --(*wFieldCount) >= 0; wKeyIndex++)
		if (rows->wIsType != (SQL_TRUE + 0))
			return NULL;
		else {
			if ((rows = SQLStepRows(rows, keys[wKeyIndex], wIsNew, lKey)) == NULL)
				break;
		}
	return rows;
}

#include "c++/string_format.hpp"

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
/*
#include <iostream>
#include <sstream>

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
*/

int SQLExportRows(SQL_ROWS rows, int depth, SQL_DATA __k, bson *b)
{
	std::string k;

	if ((__k == NULL) || (__k->StrLen_or_Ind <= 0)) return -1;
	switch (__k->wCType)
	{
		case SQL_C_CHAR: k.assign(SQL_DATA_PTR<char>(__k), __k->StrLen_or_Ind); break;
        case SQL_C_WCHAR:
        {
#if 1
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
            {
                wchar_t *w = SQL_DATA_PTR<wchar_t>(__k);
                k = utf8_conv.to_bytes(w, w + (__k->StrLen_or_Ind / sizeof(wchar_t)));
            }
#else
            k = narrow(SQL_DATA_PTR<wchar_t>(__k), __k->StrLen_or_Ind / sizeof(wchar_t));
#endif
        } break;
#define CASE_ASSIGN_KEY(ST, F, CT, OT)	case SQL_C_##ST: do { \
	k = std::format(F, (CT)SQL_DATA_VALUE<OT>(__k)).c_str(); \
} while (0)
		case SQL_C_BIT:
		CASE_ASSIGN_KEY(STINYINT, "#%d", int, int8_t); break;
		CASE_ASSIGN_KEY(SSHORT, "#%d", int, int16_t); break;
		CASE_ASSIGN_KEY(SLONG, "#%d", int, int32_t); break;
		CASE_ASSIGN_KEY(SBIGINT, "#%I64d", long, int64_t); break;

		CASE_ASSIGN_KEY(UTINYINT, "#%u", unsigned int, uint8_t); break;
		CASE_ASSIGN_KEY(USHORT, "#%u", unsigned int, uint16_t); break;
		CASE_ASSIGN_KEY(ULONG, "#%u", unsigned int, uint32_t); break;
		CASE_ASSIGN_KEY(UBIGINT, "#%I64u", unsigned long, uint64_t); break;

		CASE_ASSIGN_KEY(FLOAT, "#%.f", float, float); break;
		CASE_ASSIGN_KEY(DOUBLE, "#%.g", double, double); break;

		case SQL_C_TYPE_TIME:
		case SQL_C_TYPE_DATE:
		CASE_ASSIGN_KEY(TYPE_TIMESTAMP, "#%u", unsigned int, time_t); break;
#undef CASE_ASSIGN_KEY
		default: return -1;
	}

	if (rows->wIsType == (SQL_TRUE + 1))
		switch (rows->u.v->wCType)
		{
            case SQL_C_WCHAR:
            {
#if 1
                std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> utf8_conv;
                std::string s;
                {
                    wchar_t *w = SQL_DATA_PTR<wchar_t>(__k);
                    s = utf8_conv.to_bytes(w, w + (__k->StrLen_or_Ind / sizeof(wchar_t)));
                }
#else
                std::string s = narrow(SQL_DATA_PTR<wchar_t>(__k), __k->StrLen_or_Ind / sizeof(wchar_t));
#endif
                bson_append_string_n(b, k.c_str(), s.c_str(), (int)s.length());
            } break;
			case SQL_C_CHAR:
			{
				bson_append_string_n(b, k.c_str(), 
					SQL_DATA_PTR<char>(rows->u.v), (int)rows->u.v->StrLen_or_Ind);
			} break;

#define CASE_ASSIGN_VALUE(ST, CT, OT)	case SQL_C_##ST: do {	\
	bson_append_##CT(b, k.c_str(), (CT)SQL_DATA_VALUE<OT>(rows->u.v)); 	\
} while (0)
			case SQL_C_BIT:
			CASE_ASSIGN_VALUE(STINYINT, int, int8_t); break;
			CASE_ASSIGN_VALUE(SSHORT, int, int16_t); break;
			CASE_ASSIGN_VALUE(SLONG, int, int32_t); break;
			CASE_ASSIGN_VALUE(SBIGINT, long, int64_t); break;

			CASE_ASSIGN_VALUE(UTINYINT, int, uint8_t); break;
			CASE_ASSIGN_VALUE(USHORT, int, uint16_t); break;
			CASE_ASSIGN_VALUE(ULONG, long, uint32_t); break;
			CASE_ASSIGN_VALUE(UBIGINT, long, uint64_t); break;

			CASE_ASSIGN_VALUE(FLOAT, double, float); break;
			CASE_ASSIGN_VALUE(DOUBLE, double, double); break;

			case SQL_C_TYPE_TIME:
			case SQL_C_TYPE_DATE:
			CASE_ASSIGN_VALUE(TYPE_TIMESTAMP, time_t, time_t); break;
#undef CASE_ASSIGN_VALUE

            case SQL_C_BINARY:
            {
                const char *v = SQL_DATA_PTR<char>(rows->u.v);

                if (!IsBSON(v, (int)rows->u.v->StrLen_or_Ind))
                    return bson_append_binary(b, k.c_str(), BSON_BIN_BINARY, v, (int)rows->u.v->StrLen_or_Ind);
                else {
                    bson s_b;

                    bson_init_finished_data(&s_b, (char *)v);
                    return bson_append_bson(b, k.c_str(), &s_b);
                }
            } break;

			default: return -1;
		}
	else {
		bson_append_start_object(b, k.c_str());
		for (SQLRowValueSet::iterator it = rows->u.i[0]->begin(); it != rows->u.i[0]->end(); it++)
		{
			SQLExportRows(it->second, depth + 1, it->first, b);
		}
		bson_append_finish_object(b);
	}
	return 0;
}

#if 0
static SQLRETURN SQL_ROWS_DATA_ISEQUAL(SQL_ROWS l, SQL_ROWS r)
{
	if (l->wIsType != r->wIsType)
		return SQL_FALSE;
	else {
		switch (l->wIsType)
		{
			case SQL_TRUE + 0: return (l->u.i[0] == r->u.i[0]);
			case SQL_TRUE + 1: return (l->u.v == r->u.v);
			default: break;
		}
	}
	return SQL_TRUE;
}
#endif

#include <cstring>
#include <unordered_map>

struct HASH_SQLFieldValue
{
	size_t operator ()(const std::string &k) const { return std::hash<std::string>()(k); }
};

typedef std::unordered_map<std::string, SQL_UPDATE_DATE_STRUCT, HASH_SQLFieldValue> SQLFieldValueSet;

typedef struct tagSQL_FIELD_VALUE_STRUCT {
	SQLSMALLINT wIsState;

	SQLFieldValueSet where;
	SQLFieldValueSet values;
} SQL_FIELD_VALUE_STRUCT,
*SQL_FIELD_VALUE;

struct HASH_SQLFieldValueSet
{
	size_t operator ()(const SQLFieldValueSet *k) const
	{
		size_t r = 0;

		for (SQLFieldValueSet::const_iterator k_it = k->begin(); k_it != k->end(); k_it++)
			r = (r ^ (std::hash<std::string>()(k_it->first) << 1)) >> 1;
		return r;
	}

	bool operator ()(const SQLFieldValueSet *l, const SQLFieldValueSet *r) const {
		SQLFieldValueSet::const_iterator l_it = l->begin();
		SQLFieldValueSet::const_iterator r_it = r->begin();

		for (; (l_it != l->end()) && (r_it != r->end()); )
		{
			if (l_it->first.compare(r_it->first))
				return false;
		}

		return ((l_it == l->end()) && (r_it == r->end()));
	}
};

typedef std::list<SQL_FIELD_VALUE> SQLFieldValueList;
typedef std::unordered_map<
	SQLFieldValueSet *, SQLFieldValueList, HASH_SQLFieldValueSet, HASH_SQLFieldValueSet
> SQLQuerySet_t;

#define SQL_DATA_COPY(V)			SQL_DATA_New(V->wCType, V->StrLen_or_Ind, (SQLCHAR *)(V + 1))
#define SQL_DATA_ISEQUAL(L, R)		(((L)->StrLen_or_Ind == (R)->StrLen_or_Ind) && \
										(memcmp(SQL_DATA_PTR<char>((L)), SQL_DATA_PTR<char>((R)), (R)->StrLen_or_Ind) == 0))


static void SQLOriginCopy(SQLSMALLINT wFieldIndex, SQL_ROWS rows, SQLFieldValueSet *values)
{
	for (SQLRowValueSet::iterator l_it = rows->u.i[0]->begin(); l_it != rows->u.i[0]->end(); l_it++)
	{
		if (l_it->second->wIsType == (SQL_TRUE + 0))
			SQLOriginCopy(wFieldIndex + 1, l_it->second, values);
		else {
			std::string n(SQL_DATA_PTR<char>(l_it->first), l_it->first->StrLen_or_Ind);
			values->insert(SQLFieldValueSet::value_type(n, { l_it->second->u.v, NULL }));
		}
	}
}

/* 데이터를 데이터베이스에 적용하기 위해 변경된 데이터를 수집한다. */
static void SQLMergeCommit(SQLSMALLINT *wFieldCount, SQLSMALLINT wFieldIndex,
	SQL_ROWS rows, SQLFieldValueSet *__x, SQLFieldValueSet *values, SQLQuerySet_t *querys)
{
#define SQL_FIELD_VALUE_MOVE(SRC, DEST)		do {	\
	for (SQLFieldValueSet::iterator w_it = SRC.begin(); w_it != SRC.end(); w_it++)	\
		if (DEST.find(w_it->first) == DEST.end())	\
			DEST.insert(SQLFieldValueSet::value_type(w_it->first, w_it->second));	\
	SRC.clear();	\
} while (0)


#define SQL_QUERY_INSERT(K, V)				do {	\
	SQLQuerySet_t::iterator q_it = querys->find(K);	\
	if (q_it != querys->end())	\
		;	\
	else {	\
		q_it = querys->insert(	\
			SQLQuerySet_t::value_type(k, SQLFieldValueList())	\
		).first;	\
	}	\
	q_it->second.push_back(V);	\
} while (0)

	SQL_ALIGN_FIELD pAlignFields = (SQL_ALIGN_FIELD)(wFieldCount + 1);

	for (SQLRowValueSet::iterator l_it = rows->u.i[1]->begin(); l_it != rows->u.i[1]->end(); l_it++)
	{
		if (wFieldIndex >= (*wFieldCount))
		{
			/* VALUE */
			SQLRowValueSet::iterator it = rows->u.i[0]->find(l_it->first);
			std::string n(SQL_DATA_PTR<char>(l_it->first), l_it->first->StrLen_or_Ind);

			values->insert(SQLFieldValueSet::value_type(n, {
				l_it->second->u.v, (it == rows->u.i[0]->end()) ? NULL : it->second->u.v
			}));
		}
		else if ((l_it->second->wIsState & SQL_FIELD_DELETE) || (l_it->second->wIsType == (SQL_TRUE + 0)))
		{
			SQL_FIELD_VALUE f = new SQL_FIELD_VALUE_STRUCT;
			SQLFieldValueSet *k = &f->where;

			if (__x != NULL) f->where.insert(__x->begin(), __x->end());
			/* WHERE */
			f->where.insert(
				SQLFieldValueSet::value_type((char *)(pAlignFields + wFieldIndex)->szColumnLabel, { NULL, l_it->first })
			);

			f->wIsState = l_it->second->wIsState;
			if (l_it->second->wIsState & SQL_FIELD_DELETE)
				SQLOriginCopy(wFieldIndex + 1, l_it->second, &f->values) /* SQLFreeRows(l_it->second) */;
			else {
				SQLMergeCommit(wFieldCount, wFieldIndex + 1, l_it->second, &f->where, &f->values, querys);
				if (f->values.empty() == false)
					switch (f->wIsState & SQL_FIELD_INSERT)
					{
						case SQL_FIELD_INSERT: k = &f->values; /* where값은 values로 사용 */
						{
							SQL_FIELD_VALUE_MOVE(f->where, f->values);
						} break;
						/* case SQL_LOG_UPDATE: break; */
						default:break;
					}
				else {
					/* SQL_DATA_Free(l_it->first); */
					{
						delete f;
					} goto _gN;
				}
			}

			SQL_QUERY_INSERT(k, f);
		}
_gN:;
	}

	if (wFieldIndex < (*wFieldCount))
		for (SQLRowValueSet::iterator l_it = rows->u.i[0]->begin(); l_it != rows->u.i[0]->end(); l_it++)
		{
			if (l_it->second->wIsType != (SQL_TRUE + 0))
				;
			else {
				SQL_FIELD_VALUE f = new SQL_FIELD_VALUE_STRUCT;
				SQL_DATA x_k = l_it->first /* SQL_DATA_COPY(l_it->first) */;

				if (__x != NULL) f->where.insert(__x->begin(), __x->end());
				/* WHERE */
				f->where.insert(
					SQLFieldValueSet::value_type((char *)(pAlignFields + wFieldIndex)->szColumnLabel, { NULL, x_k })
				);
				SQLMergeCommit(wFieldCount, wFieldIndex + 1, l_it->second, &f->where, &f->values, querys);
				if (f->values.empty())
					delete f;
				else {
					f->wIsState = l_it->second->wIsState;
					{
						SQLFieldValueSet *k = &f->where;

						if ((f->wIsState & SQL_FIELD_INSERT) == 0)
							(f->wIsState == 0) ? f->wIsState = SQL_FIELD_UPDATE : 0;
						else {
							k = &f->values;
							SQL_FIELD_VALUE_MOVE(f->where, f->values);
						}

						SQL_QUERY_INSERT(k, f);
					}
				}
			}
		}

	/* rows->u.i[1]->clear(); */
#undef SQL_QUERY_INSERT
#undef SQL_FIELD_VALUE_MOVE
}


class CopyBLOB : public SQL_BINARY_CALLBACK {
public:
	CopyBLOB(SQL_DATA data) {
		this->buffer_ = SQL_DATA_PTR<const char>(data);
		this->length_ = data->StrLen_or_Ind;
	}
	virtual ~CopyBLOB() { }

	virtual SQLINTEGER NEED_DATA(SQL_PARAM *pParam, SQLINTEGER iOffset, SQLPOINTER pExternalBuffer, SQLPOINTER *pValue)
	{
#define MAX_BLOB_SIZE			(size_t)(16 * 1024)
		(*pValue) = (SQLPOINTER)(buffer_ + iOffset);
		return (SQLINTEGER)std::min(length_ - iOffset, MAX_BLOB_SIZE);
#undef MAX_BLOB_SIZE
	}

	/* virtual void DISPOSE() { delete this; } */
private:
	const char *buffer_;
	size_t      length_;
};


static SQL_PARAM SQL_CToSQL(SQL_DATA v)
{
	// SQL_FIELD f = SQLFieldFromResult(pRows, (SQLCHAR *)i_it->first.c_str());
	if (v == NULL) ;
	else switch (v->wCType)
	{
		case SQL_C_STINYINT: return SQL_PARAM_SINT8(SQL_DATA_PTR<int8_t>(v)); break;
		case SQL_C_SSHORT: return SQL_PARAM_SINT16(SQL_DATA_PTR<int16_t>(v)); break;
		case SQL_C_SLONG: return SQL_PARAM_SINT32(SQL_DATA_PTR<int32_t>(v)); break;
		case SQL_C_SBIGINT: return SQL_PARAM_SINT64(SQL_DATA_PTR<int64_t>(v)); break;

		case SQL_C_UTINYINT: return SQL_PARAM_UINT8(SQL_DATA_PTR<uint8_t>(v)); break;
		case SQL_C_USHORT: return SQL_PARAM_UINT16(SQL_DATA_PTR<uint16_t>(v)); break;
		case SQL_C_ULONG: return SQL_PARAM_UINT32(SQL_DATA_PTR<uint32_t>(v)); break;
		case SQL_C_UBIGINT: return SQL_PARAM_UINT64(SQL_DATA_PTR<uint64_t>(v)); break;

		case SQL_C_FLOAT: return SQL_PARAM_FLOAT(SQL_DATA_PTR<float>(v)); break;
		case SQL_C_DOUBLE: return SQL_PARAM_DOUBLE(SQL_DATA_PTR<double>(v)); break;

		case SQL_C_CHAR:
		{
			return SQL_PARAM_CHAR(SQL_DATA_PTR<SQLCHAR>(v), v->StrLen_or_Ind);
		} break;
        case SQL_C_WCHAR:
        {
			return SQL_PARAM_WCHAR(SQL_DATA_PTR<SQLWCHAR>(v), v->StrLen_or_Ind);
        } break;

		case SQL_C_TYPE_TIME: return SQL_PARAM_TYPE_TIME(SQL_DATA_PTR<TIME_STRUCT>(v)); break;
		case SQL_C_TYPE_DATE: return SQL_PARAM_TYPE_DATE(SQL_DATA_PTR<DATE_STRUCT>(v)); break;
		case SQL_C_TYPE_TIMESTAMP: return SQL_PARAM_TYPE_TIMESTAMP(SQL_DATA_PTR<TIMESTAMP_STRUCT>(v)); break;

		case SQL_C_BINARY:
		{
			return SQL_PARAM_BINARY(LONGVARBINARY, new CopyBLOB(v), v->StrLen_or_Ind);
		} break;

		default: break;
	}
	return SQL_PARAM_NULL();
}

#if 0
static void SQLClearCommit(SQLSMALLINT *wFieldCount, SQLSMALLINT wFieldIndex, SQL_ROWS rows)
{
	SQL_ALIGN_FIELD pAlignFields = (SQL_ALIGN_FIELD)(wFieldCount + 1);

	for (SQLRowValueSet::iterator l_it = rows->u.i[1]->begin(); l_it != rows->u.i[1]->end(); l_it++)
	{
		if ((wFieldIndex >= (*wFieldCount)) ||
			(l_it->second->wIsState & SQL_FIELD_DELETE) ||
			(l_it->second->wIsType != (SQL_TRUE + 0)))
			SQLFreeRows(l_it->second);
		else {
			SQLClearCommit(wFieldCount, wFieldIndex + 1, l_it->second);
		}

		SQL_DATA_Free(l_it->first);
		delete l_it->second;
	}

	if (wFieldIndex < (*wFieldCount))
		for (SQLRowValueSet::iterator l_it = rows->u.i[0]->begin(); l_it != rows->u.i[0]->end(); l_it++)
		{
			if (l_it->second->wIsType != (SQL_TRUE + 0))
				SQLFreeRows(l_it->second);
			else {
				SQLClearCommit(wFieldCount, wFieldIndex + 1, l_it->second);
			}
		}

	rows->u.i[1]->clear();
}
#endif

#define GENERATED_KEYS				".GENERATED_KEYS"

SQL_DATA SQLGetGeneratedKeys(SQL_RESULT R, SQLLEN *lRowCount)
{
	SQLRowKeySet::iterator f_it = R->META->find(GENERATED_KEYS);
	if (f_it == R->META->end())
		return NULL;
	else {
		SQLLEN *lGeneratedKeys = (SQLLEN *)f_it->second.data();

		(*lRowCount) = *lGeneratedKeys;
		return (SQL_DATA)(lGeneratedKeys + 1);
	}
}

/* */
SQLRETURN SQLFetchGeneratedKeys(SQLHSTMT stmt, SQL_RESULT R, SQLLEN lRowCount)
{
	SQLRETURN rc = SQL_SUCCESS;

	// fprintf(stderr, " * [DEBUG.%d] %s - %ld ['%s']\n", __LINE__, __FUNCTION__, lRowCount, R->SPEC->SELECT_INSERT_ID);
	if ((lRowCount > 0) &&
		SQL_SUCCEEDED(rc = SQLExecDirectA(stmt, (SQLCHAR *)R->SPEC->SELECT_INSERT_ID, SQL_NTS)))
	{
		SQL_COLUMNS pColumns = NULL;
		SQLSMALLINT wColumnCount = SQLResultColumns(stmt, &pColumns, SQL_TRUE);
		// fprintf(stderr, " * [DEBUG.%d] %s - %d\n", __LINE__, __FUNCTION__, wColumnCount);
		if (wColumnCount > 0)
		{
			SQLRowKeySet::iterator f_it = R->META->find(GENERATED_KEYS);
			if (f_it == R->META->end())
			{
				f_it = R->META->insert(
					SQLRowKeySet::value_type(GENERATED_KEYS, std::buffer(sizeof(SQLLEN), 0))
				).first;
				*((SQLLEN *)f_it->second.data()) = 0;
			}

			// fprintf(stderr, " * [DEBUG.%d] %s - %d\n", __LINE__, __FUNCTION__, f_it->second.size());
			for (; SQLFetch(stmt) != SQL_NO_DATA_FOUND;)
				if (SQLReviewColumns(stmt, pColumns) == SQL_SUCCESS)
				{
					SQLLEN dwUsedBytes;
					SQL_DATA v = SQLColumnTo(SQL_NULL_HSTMT, pColumns[0], &dwUsedBytes);
					if ((v == NULL) || (dwUsedBytes >= INT32_MAX))
						return SQL_ERROR;
					else {
						SQLCHAR __x[BUFSIZ];
						SQL_DATA g_v = NULL;

						*((SQLLEN *)f_it->second.data()) = lRowCount;
						if (v->wCType != SQL_C_CHAR)
							g_v = v;
						else {
							SQL_FIELD F = SQLFieldFromResult(R, SQLAutoIncrementResult(R));

							SQL_DATA __p = (SQL_DATA)__x; __p->dwColumnLength = 0;
							g_v = SQL_DATA_New(F->wCType, F->dwColumnLength, NULL, __p);
							{
								SQLROWCOUNT i = strtoll(std::string((char *)(v + 1), v->StrLen_or_Ind).c_str(), NULL, 10);

#define DECLARE_ASSIGN_VALUE(v, CT, DT)	\
	case SQL_C_S ## CT : *SQL_DATA_PTR<DT>(v) = (DT)i; break;	\
	case SQL_C_U ## CT : *SQL_DATA_PTR<u ## DT>(v) = (DT)i; break;
								switch (g_v->wCType)
								{
									DECLARE_ASSIGN_VALUE(g_v, TINYINT, int8_t)
									DECLARE_ASSIGN_VALUE(g_v, SHORT, int16_t)
									DECLARE_ASSIGN_VALUE(g_v, LONG, int32_t)
									DECLARE_ASSIGN_VALUE(g_v, BIGINT, int64_t)
								}
#undef DECLARE_ASSIGN_VALUE
							}
						}

#if 0 // def DEBUG_ODBC
#define DEBUG_GENERATED_KEY(v, CT, DT, DF)		\
case SQL_C_S ## CT: fprintf(stderr, " * GENERATED_KEY.%d: " DF "\n", v->wCType, *SQL_DATA_PTR<DT>(v)); break;	\
case SQL_C_U ## CT: fprintf(stderr, " * GENERATED_KEY.%d: " DF "\n", v->wCType, *SQL_DATA_PTR<u ## DT>(v)); break;

						switch (g_v->wCType)
						{
							DEBUG_GENERATED_KEY(g_v, TINYINT, int8_t, "%d")
							DEBUG_GENERATED_KEY(g_v, SHORT, int16_t, "%d")
							DEBUG_GENERATED_KEY(g_v, LONG, int32_t, "%d")
							DEBUG_GENERATED_KEY(g_v, BIGINT, int64_t, "%ld")
						}
#undef DEBUG_GENERATED_KEY
#endif
						dwUsedBytes = (sizeof(SQL_DATA_STRUCT) + g_v->StrLen_or_Ind);
						f_it->second.resize(f_it->second.size() + (dwUsedBytes * lRowCount), 0);
						{
							SQLLEN dwOffset = (R->SPEC->INCREMENT_DIRECTION > 0)
								? sizeof(SQLLEN): f_it->second.size() + (dwUsedBytes = -dwUsedBytes);

							do {
								SQL_DATA __p = (SQL_DATA)(f_it->second.data() + dwOffset); __p->dwColumnLength = 0;
								SQL_DATA_New(g_v->wCType, g_v->StrLen_or_Ind, (SQLCHAR *)(g_v + 1), __p);
								if ((--lRowCount) <= 0)
									break;
								else {
#define DECLARE_GENERATED_KEY(v, CT, DT)	\
	case SQL_C_S ## CT : *SQL_DATA_PTR<DT>(v) += (DT)R->SPEC->INCREMENT_DIRECTION; break;	\
	case SQL_C_U ## CT : *SQL_DATA_PTR<u ## DT>(v) += (DT)R->SPEC->INCREMENT_DIRECTION; break;
									switch (g_v->wCType)
									{
										DECLARE_GENERATED_KEY(g_v, TINYINT, int8_t)
										DECLARE_GENERATED_KEY(g_v, SHORT, int16_t)
										DECLARE_GENERATED_KEY(g_v, LONG, int32_t)
										DECLARE_GENERATED_KEY(g_v, BIGINT, int64_t)
									}
#undef DECLARE_GENERATED_KEY
									dwOffset += dwUsedBytes;
								}
							} while (SQL_TRUE);
#undef GENERATED_STEP
						}
					}
					break;
				}
		}

		SQLCancelColumns(stmt, pColumns);
	}
	return rc;
}


static BOOL SQLIsMergeDirty(SQLSMALLINT *wFieldCount, SQLSMALLINT wFieldIndex, SQL_ROWS rows)
{
	for (SQLRowValueSet::iterator l_it = rows->u.i[1]->begin(); l_it != rows->u.i[1]->end(); l_it++)
	{
		if ((wFieldIndex >= (*wFieldCount)) ||
			(l_it->second->wIsState & SQL_FIELD_DELETE) ||
			(l_it->second->wIsType != (SQL_TRUE + 0)) ||
			SQLIsMergeDirty(wFieldCount, wFieldIndex + 1, l_it->second))
			return SQL_TRUE;
	}

	if (wFieldIndex < (*wFieldCount))
		for (SQLRowValueSet::iterator l_it = rows->u.i[0]->begin(); l_it != rows->u.i[0]->end(); l_it++)
		{
			if ((l_it->second->wIsType != (SQL_TRUE + 0)) ||
				SQLIsMergeDirty(wFieldCount, wFieldIndex + 1, l_it->second))
				return SQL_TRUE;
		}
	return SQL_FALSE;
}



BOOL SQLIsDirtyResult(SQL_RESULT R) {
    if (R == NULL)
        return SQL_FALSE;
    else {
	    SQLRowKeySet::iterator f_it = R->META->find(".KEYS");
	    return ((R->wUpdatable == 0) || (f_it == R->META->end()))
		    ? SQL_FALSE : SQLIsMergeDirty((SQLSMALLINT *)f_it->second.data(), 0, &R->ROWS);
    }
}

/* 변경된 데이터를 적용한다.
*   해당 처리를 통해, 쿼리문을 생성하여 실행하고 그에 대한 결과를 반환한다.
*/
SQLRETURN SQLCommitResult(SQLHSTMT stmt, SQL_RESULT R, SQLCHAR *szSchemaTableName, SQLCommitCALLBACK *commitCallback)
{
	SQLRETURN rc = SQL_ERROR;

	SQLRowKeySet::iterator f_it = R->META->find(".KEYS");
	if ((R->wUpdatable == 0) || (f_it == R->META->end()))
		;
	else {
		SQLFieldValueList merge;

		SQL_PARAMS params;
#define SQL_PARAMS_RESET(__E__)	do {    \
	for (SQL_PARAMS::iterator it = __E__.begin(); it != __E__.end(); it++) {	\
        if (it->fCType == SQL_C_BINARY) delete (CopyBLOB *)it->rgbValue;    \
    } __E__.clear(); \
} while (0)
		SQLQuerySet_t querys;
		SQLLEN lRowCommit = 0;

		R->META->erase(".COMMIT");
		R->META->erase(GENERATED_KEYS);

		if (szSchemaTableName != NULL)
			;
		else {
			if ((szSchemaTableName = SQLTableResult(R)) == NULL)
				return SQL_ERROR;
		}
		if (*(szSchemaTableName + 0) == 0) return SQL_ERROR;

		SQLMergeCommit((SQLSMALLINT *)f_it->second.data(), 0, &R->ROWS, NULL, NULL, &querys);
		for (SQLQuerySet_t::iterator q_it = querys.begin(); q_it != querys.end(); q_it++)
		{
			SQLFieldValueList *l = &q_it->second;

			for (SQLFieldValueList::iterator l_it = l->begin(); l_it != l->end(); l_it++)
			{
				if ((*l_it)->wIsState & SQL_FIELD_INSERT)
					merge.push_back(*l_it);
				else {
					SQLCommitCALLBACK::WhereSet  l_wheres;
					SQLCommitCALLBACK::ChangeSet l_changes;

					std::string query;

					switch ((*l_it)->wIsState & SQL_FIELD_DELETE)
					{
						default /* case SQL_LOG_UPDATE */:
						{
							std::string values;
							for (SQLFieldValueSet::iterator
								i_it = (*l_it)->values.begin(); i_it != (*l_it)->values.end(); i_it++)
							{
								SQLCHAR *k = (SQLCHAR *)i_it->first.c_str();
								SQL_FIELD f = SQLFieldFromResult(R, k);
								if ((f != NULL) &&
									((commitCallback == NULL) || commitCallback->allowColumn(szSchemaTableName, k)))
								{
									if (values.length() > 0) values.append(",");
									{

										values.append((const char *)R->SPEC->PREFIX_NAME);
										values.append(i_it->first);
										values.append((const char *)R->SPEC->SUBFIX_NAME);

									} values.append(" = ?");

									// SQL_FIELD f = SQLFieldFromResult(pRows, (SQLCHAR *)i_it->first.c_str());
									params.push_back(SQL_CToSQL(i_it->second.v));
									if (commitCallback != NULL)
									{
										l_changes.insert(
											SQLCommitCALLBACK::ChangeSet::value_type(k, i_it->second)
										);
									}
									// if (i_it->second.o != NULL) SQL_DATA_Free(i_it->second.o);
								}
							}

							query.append("UPDATE "); {
								query.append((char *)szSchemaTableName);
							} query.append(" SET " + values);
							// fprintf(stderr, " UPDATE TABLE SET %s WHERE %s\n", values.c_str(), where.c_str());
						} break;
						case SQL_FIELD_DELETE:
						{
							if (commitCallback != NULL)
							{
								for (SQLFieldValueSet::iterator
									i_it = (*l_it)->values.begin(); i_it != (*l_it)->values.end(); i_it++)
								{
									SQLCHAR *k = (SQLCHAR *)i_it->first.c_str();
									SQL_FIELD f = SQLFieldFromResult(R, k);
									if ((f != NULL) &&
										((commitCallback == NULL) || commitCallback->allowColumn(szSchemaTableName, k)))
										l_changes.insert(
											SQLCommitCALLBACK::ChangeSet::value_type(k, i_it->second)
										);
								}
							}
						} 
						
						query.append("DELETE FROM "); {
							query.append((char *)szSchemaTableName);
						} query.append("");
						//  fprintf(stderr, " DELETE TABLE FROM WHERE %s\n", where.c_str());
					}

					query.append(" WHERE ");
					{
						std::string where;
						for (SQLFieldValueSet::iterator w_it = (*l_it)->where.begin(); w_it != (*l_it)->where.end(); w_it++)
						{
							SQLCHAR *k = (SQLCHAR *)w_it->first.c_str();
							if ((commitCallback == NULL) || commitCallback->allowColumn(szSchemaTableName, k))
							{
								if (where.length() > 0) where.append(" AND ");
								{

									where.append((const char *)R->SPEC->PREFIX_NAME);
									where.append(w_it->first);
									where.append((const char *)R->SPEC->SUBFIX_NAME);

								} where.append(" = ?");

								params.push_back(SQL_CToSQL(w_it->second.v));
								if (commitCallback != NULL)
								{
									l_wheres.insert(
										SQLCommitCALLBACK::WhereSet::value_type(k, w_it->second.v)
									);
								}
							}
							// SQL_DATA_Free(w_it->second);
						}
						query.append(where);
					}

					if ((rc = SQLExecuteEx(stmt, (SQLCHAR *)query.c_str(), params.data(), NULL)) != SQL_SUCCESS)
_gX:				{
						SQLErrorReport(stmt, SQL_HANDLE_STMT);
						SQL_PARAMS_RESET(params);
						do {
							for (; l_it != l->end(); l_it++) delete (*l_it);
							if ((++q_it) == querys.end())
								break;
							l_it = (l = &q_it->second)->begin();
						} while (true);
						for (SQLFieldValueList::iterator m_it = merge.begin(); m_it != merge.end(); m_it++)
							delete *m_it;
						return rc;
					}
					else {
						SQLLEN lRowCount = 0;

						if ((rc = SQLRowCount(stmt, &lRowCount)) == SQL_SUCCESS)
							lRowCommit++;
						else {
							goto _gX;
						}

						SQL_PARAMS_RESET(params);
						if (commitCallback != NULL)
							commitCallback->ChangeLog((*l_it)->wIsState, szSchemaTableName, l_wheres, l_changes);
					}
					delete (*l_it);
				}
			}
		}

		if (merge.size() > 0)
		{
#define MAX_TABLE_COLUMNS					256
			SQLSMALLINT  wAllowColumns[MAX_TABLE_COLUMNS], /* 사용 가능한 필드인지 확인한다. */
				        *pLastColumns = wAllowColumns;

			std::string i_columns;
			std::string i_values;

			BOOL autoIncrementColumn = (R->META->find(".AUTO_INCREMENT") != R->META->end());
			{
				SQLFieldValueSet *i = &merge.front()->values;
				for (SQLFieldValueSet::iterator i_it = i->begin(); i_it != i->end(); i_it++)
				{
					SQLCHAR *k = (SQLCHAR *)i_it->first.c_str();
					SQL_FIELD f = SQLFieldFromResult(R, k);
					if ((f == NULL) ||
						((commitCallback != NULL) && (commitCallback->allowColumn(szSchemaTableName, k) == false)))
						*pLastColumns++ = -1; /* 존재하지 않는 필드 */
					else {
						if (f->wAutoIncrement == 0)
						{
							if (i_columns.length() > 0) i_columns.append(",");
							{
								i_columns.append((const char *)R->SPEC->PREFIX_NAME);
								i_columns.append((const char *)k);
								i_columns.append((const char *)R->SPEC->SUBFIX_NAME);
							}
						}

						*pLastColumns++ = f->wAutoIncrement;
					}
				}
			}

#undef MAX_TABLE_COLUMNS
			std::string query = "INSERT INTO "; {
				query.append((char *)szSchemaTableName);
			} query.append(" (" + i_columns + ") VALUES ");
			for (SQLFieldValueList::iterator l_it = merge.begin(); l_it != merge.end(); l_it++)
			{
#define MAX_INSERT_COLUMNS			100 /* (sizeof(ParamBuffer) / sizeof(SQL_PARAM)) */
				if ((i_values.size() + (*l_it)->values.size()) >= MAX_INSERT_COLUMNS)
				{
					if ((rc = SQLExecuteEx(stmt, (SQLCHAR *)(query + i_values).c_str(), params.data(), NULL)) != SQL_SUCCESS)
_gIX:				{
						SQLErrorReport(stmt, SQL_HANDLE_STMT);
						SQL_PARAMS_RESET(params);
						for (; l_it != merge.end(); l_it++) delete (*l_it);
						return rc;
					}
					else {
						SQLLEN lRowCount = 0;

						if ((rc = SQLRowCount(stmt, &lRowCount)) == SQL_SUCCESS)
							lRowCommit += lRowCount;
						else {
							goto _gIX;
						}

						if (autoIncrementColumn)
						{
							if ((rc = SQLFetchGeneratedKeys(stmt, R, lRowCount)) != SQL_SUCCESS)
								goto _gIX;
						}

						SQL_PARAMS_RESET(params);
					}

					i_values.clear();
				}
#undef MAX_INSERT_COLUMNS

				if (i_values.length() > 0) i_values.append(",(");
				else                       i_values.append("(");
				{
					SQLCommitCALLBACK::WhereSet  l_wheres;
					SQLCommitCALLBACK::ChangeSet l_changes;
					SQLSMALLINT *pColumnState = wAllowColumns;

					std::string values;
					for (SQLFieldValueSet::iterator
						i_it = (*l_it)->values.begin(); i_it != (*l_it)->values.end(); i_it++)
					{
						if (pColumnState >= pLastColumns)
							break;
						else {
							if (*pColumnState++ == 0)
							{
								if (values.length() > 0) values.append(",");
								values.append("?");

								params.push_back(SQL_CToSQL(i_it->second.v));
								if (commitCallback != NULL)
								{
									SQLCHAR *k = (SQLCHAR *)i_it->first.c_str();
									l_changes.insert(
										SQLCommitCALLBACK::ChangeSet::value_type(k, i_it->second)
									);
								}
							}
						}
						// SQL_DATA_Free(i_it->second);
					}

					if (commitCallback != NULL)
						commitCallback->ChangeLog((*l_it)->wIsState, szSchemaTableName, l_wheres, l_changes);
					i_values.append(values);
				}
				i_values.append(")");
				delete (*l_it);
			}

			merge.clear();
			if (params.empty() == false)
			{
				if ((rc = SQLExecuteEx(stmt, (SQLCHAR *)(query + i_values).c_str(), params.data(), NULL)) != SQL_SUCCESS)
_gFX:			{
					SQLErrorReport(stmt, SQL_HANDLE_STMT);
					SQL_PARAMS_RESET(params);
					return rc;
				}
				else {
					SQLLEN lRowCount = 0;

					if ((rc = SQLRowCount(stmt, &lRowCount)) == SQL_SUCCESS)
						lRowCommit += lRowCount;
					else {
						goto _gFX;
					}

					if (autoIncrementColumn)
					{
						if ((rc = SQLFetchGeneratedKeys(stmt, R, lRowCount)) != SQL_SUCCESS)
							goto _gFX;
					}

                    SQL_PARAMS_RESET(params);
				}
			}
		}

		{
			SQLRowKeySet::iterator f_it = R->META->find(".COMMIT");
			if (f_it == R->META->end())
				f_it = R->META->insert(
					SQLRowKeySet::value_type(".COMMIT", std::buffer(sizeof(SQLLEN), 0))
				).first;
			*((SQLLEN *)f_it->second.data()) = lRowCommit;
		}
#undef SQL_PARAMS_RESET
	}
#undef SQL_BUFSIZ

	/* SQLClearCommit((SQLSMALLINT *)f_it->second.data(), 0, &(*pRows)->ROWS); */
	return rc;
}
#undef GENERATED_KEYS


/* 데이터를 업데이트 한다.
*   rows: 업데이트할 ROWS
*   keys: 변경하고자 하는 키
*   wFieldCount: keys의 count
*   v: 변경하고자 하는 값 (SQL_DATA_New()로 할당되어 넘어몬)
*   lKeys: 접근하는 경로 (OUT)
*/
SQLRETURN SQLUpdateRow(SQL_ROWS rows, SQL_DATA k, SQL_DATA v)
{
#define SQL_ROW_ADD_VALUE(K, T, V, IT)		do {	\
	SQL_DATA x_k = SQL_DATA_COPY(K);	\
	if (x_k == NULL) throw __LINE__;	\
	else {	\
		SQL_DATA x_v = (SQL_DATA)(V);	\
		IT = rows->u.i[(T) != SQL_FALSE]->insert(	\
			SQLRowValueSet::value_type(x_k, new SQL_ROWS_STRUCT())	\
		).first;	\
		IT->second->wIsType = SQL_TRUE + 1;	\
		IT->second->wIsState = (T);	\
		IT->second->p = rows; \
		IT->second->k = x_k; \
		IT->second->u.v = x_v;	\
	}	\
} while (0)
	/* k->StrLen_or_Ind == 0 인 경우, rows의 k 이름을 사용한다. */
	for (; k->StrLen_or_Ind == 0; rows = rows->p)
	{
		if (rows->p == NULL)
			throw __LINE__;
		else {
			k = rows->k;
		}
	}

	SQLRETURN rc = SQL_SUCCESS;
	SQLRowValueSet::iterator it = rows->u.i[0]->find(k);
	if (it == rows->u.i[0]->end())
	{
		if (v == NULL)
		{
			SQL_DATA_Free(v);
			return SQL_SUCCESS;
		}
		/* INSERT */ {
			SQLRowValueSet::iterator l_it = rows->u.i[1]->find(k);
			if (l_it == rows->u.i[1]->end())
				;
			else {
				l_it->second->wIsState &= ~SQL_FIELD_DELETE;
				if (SQL_DATA_ISEQUAL(l_it->second->u.v, v))
				{
					/* 데이터 복구 됨. */
					rows->u.i[0]->insert(
						SQLRowValueSet::value_type(l_it->first, l_it->second)
					);

					SQL_DATA_Free(v);
					rows->u.i[1]->erase(l_it);
					return SQL_SUCCESS;
				}
			}

			try {
				SQL_ROW_ADD_VALUE(k, SQL_FALSE, v, it);
				if (l_it == rows->u.i[1]->end())
					SQL_ROW_ADD_VALUE(k, SQL_FIELD_INSERT, NULL, l_it);
				else if ((l_it->second->wIsState & SQL_FIELD_INSERT) == 0) /* 삭제 상태인 경우 */
					l_it->second->wIsState |= SQL_FIELD_UPDATE;	/* UPDATE */
			}
			catch (...) { SQL_DATA_Free(v); return SQL_ERROR; }
			rc = l_it->second->wIsState;
		}
	}
	else if (v == NULL)
	{
		/* DELETE */
		try {
			SQLRowValueSet::iterator l_it = rows->u.i[1]->find(k);
			if (l_it != rows->u.i[1]->end())
				SQL_ROW_FREE(it);
			else {
				if (it->second->wIsState == SQL_FIELD_INSERT)
				{
					SQL_ROW_FREE(it);
					goto _gE;
				}
				else {
					l_it = rows->u.i[1]->insert(
						SQLRowValueSet::value_type(it->first, it->second)
					).first;

					l_it->second->wIsState |= SQL_FIELD_DELETE;
				}
			}

			rc = l_it->second->wIsState;
		}
		catch (...) { SQL_DATA_Free(v); return SQL_ERROR; }
_gE:	rows->u.i[0]->erase(it);
	}
	else {
		if (it->second->wIsType != (SQL_TRUE + 1))
		{
			SQL_DATA_Free(v);
			return SQL_ERROR;
		}
		else {
			if (SQL_DATA_ISEQUAL(it->second->u.v, v))
			{
				SQL_DATA_Free(v);
				return SQL_SUCCESS;
			}
			/* UPDATE */
		}

		try {
			SQLRowValueSet::iterator l_it = rows->u.i[1]->find(k);
			if (l_it == rows->u.i[1]->end())
				SQL_ROW_ADD_VALUE(it->first, rc = SQL_FIELD_UPDATE, it->second->u.v, l_it);
			else if (l_it->second->u.v != NULL) /* 신규 생성된 데이터인 경우 (데이터베이스에 없음) */
			{
				if (SQL_DATA_ISEQUAL(l_it->second->u.v, v) == false)
					rc = (l_it->second->wIsState |= SQL_FIELD_UPDATE);
				else {
					/* 데이터 복구 됨. */
					/* SQL_DATA_Free(l_it->first);
					l_it->second->wIsState &= ~SQL_FIELD_UPDATE; */
					SQL_DATA_Free(v);

					v = l_it->second->u.v;
					l_it->second->u.v = NULL;

					rc = l_it->second->wIsState &= ~SQL_FIELD_UPDATE;
					SQL_ROW_FREE(l_it);
					rows->u.i[1]->erase(l_it);

				}
				SQL_DATA_Free(it->second->u.v);
			}
			else rc = l_it->second->wIsState;
		}
		catch (...) { SQL_DATA_Free(v); return SQL_ERROR; }
		it->second->u.v = v;
	}

	return rc;
#undef SQL_ROW_ADD_VALUE
}


SQLRETURN SQLUpdateRows(SQL_ROWS rows, SQL_DATA *keys, SQLSMALLINT wFieldCount, SQL_DATA v, SQLStepKeys *lKeys)
{
	SQL_DATA k = keys[wFieldCount - 1];
	SQLSMALLINT wIsNew = 0;

	--wFieldCount;
	if ((rows->u.i[1] == NULL) ||
		/* wFieldCount > 0 인 경우에만 rows의 하위로 접근한다. 그 이외는 rows를 그대로 사용 */
		((rows = SQLStepRows(rows, keys, &wFieldCount, &wIsNew, lKeys)) == NULL) ||
		(wFieldCount > 0) || (rows->wIsType != (SQL_TRUE + 0)))
	{
		SQL_DATA_Free(v);
		return SQL_ERROR;
	}

	return SQLUpdateRow(rows, k, v);
}


#if 0
#include <stdarg.h>

SQL_DATA SQLKeyResult(SQL_RESULT *pRows, ...)
{
	va_list args;

	va_start(args, pRows);
	for (SQLSMALLINT wKeyIndex = 0; ; wKeyIndex++)
	{
		SQL_ALIGN_KEY pAlignKey = ((SQL_ALIGN_KEY)((*pRows) + 1)) + wKeyIndex;
		if (pAlignKey->wColumnIndex == SQL_NTS)
			break;
		else if (pAlignKey->wCType != SQL_C_DEFAULT)
		{
			/*
			char *szColumnLabel = (char *)(((SQLCHAR *)((*pRows) + 1)) + pAlignKey->iNameOffset);

			case SQL_BIT          : return SQL_C_BIT;

			case SQL_TINYINT      : return (wUnsigned == SQL_TRUE) ? SQL_C_UTINYINT : SQL_C_STINYINT;
			case SQL_SMALLINT     : return (wUnsigned == SQL_TRUE) ? SQL_C_USHORT   : SQL_C_SSHORT;
			case SQL_INTEGER      : return (wUnsigned == SQL_TRUE) ? SQL_C_ULONG    : SQL_C_SLONG;
			case SQL_BIGINT       : return (wUnsigned == SQL_TRUE) ? SQL_C_UBIGINT  : SQL_C_SBIGINT;

			case SQL_CHAR         : return SQL_C_CHAR;
			case SQL_VARCHAR      : return SQL_C_CHAR;
			case SQL_LONGVARCHAR  : return SQL_C_CHAR;

			case SQL_BINARY       : return SQL_C_BINARY;
			case SQL_LONGVARBINARY: return SQL_C_BINARY;

			case SQL_TYPE_TIME     : return SQL_C_TIME;
			case SQL_TYPE_DATE     : return SQL_C_DATE;
			case SQL_TYPE_TIMESTAMP: return SQL_C_TIMESTAMP;

			case SQL_TIME          : return SQL_C_TIME;
			case SQL_DATE          : return SQL_C_DATE;
			case SQL_TIMESTAMP     : return SQL_C_TIMESTAMP;

			case SQL_REAL         : return SQL_C_FLOAT;
			case SQL_FLOAT        : return SQL_C_DOUBLE;
			case SQL_DOUBLE       : return SQL_C_DOUBLE;
			case SQL_DECIMAL      : return SQL_C_CHAR;
			case SQL_NUMERIC      : return SQL_C_CHAR;
			*/
			switch (pAlignKey->wCType)
			{
			case SQL_C_CHAR:
			{
				const char *v = va_arg(args, const char *);
				SQL_DATA pColumnKey = (SQL_DATA)SQLResultRealloc(pRows, dwKeySize, SQL_DATA_SIZE + dwLabelSize);
				if (pColumnKey == NULL)

			} break;
			case SQL_C_TIME:
			case SQL_C_DATE:
			case SQL_C_TIMESTAMP:

			case SQL_C_BINARY:

			case SQL_C_FLOAT:
			case SQL_C_DOUBLE:

			case SQL_C_BIT:

			case SQL_C_STINYINT:
			case SQL_C_SSHORT:
			case SQL_C_SLONG:
			case SQL_C_SBIGINT:

			case SQL_C_UTINYINT:
			case SQL_C_USHORT:
			case SQL_C_ULONG:
			case SQL_C_UBIGINT:
			}
		}
	}
	va_end(args);

	return (SQL_DATA)(((SQLCHAR *)((*pRows) + 1)) + (*pRows)->dwStart);
}
#endif
#undef SQL_BUFSIZ

/* Cached */
typedef std::basic_string<SQLCHAR, std::char_traits<SQLCHAR>, std::allocator<SQLCHAR> > SQL_STRING;
typedef std::unordered_map<std::string, std::string> SQLCustomSet;

#include <atomic>

struct SQLDBC {
	SQLHDBC dbc;
	struct {
		std::atomic_int16_t ref;
	} T;

    std::atomic_int16_t ref;
	SQL_STRING dsn;

	SQLCustomSet custom;
};

struct HASH_SQLHDBC {
	size_t operator()(const SQLHDBC k) const { return std::hash<uintptr_t>()((uintptr_t)k); }
	bool   operator()(const SQLHDBC a, const SQLHDBC b) const { return a == b; }
};

typedef std::unordered_map<SQLHDBC, struct SQLDBC *, HASH_SQLHDBC, HASH_SQLHDBC> SQLDBC_CACHED;

struct _SQL {
	SQLDBC_CACHED _dbc;

	~_SQL() {
		for (SQLDBC_CACHED::iterator p_it = _dbc.begin(); p_it != _dbc.end(); p_it++)
		{
			struct SQLDBC *C = p_it->second;

			_SQLDisconnectEx(C->dbc);
			delete C;
		}
	}

	SQLHDBC Connect(SQLCHAR *lpszConnectString, SQLINTEGER iConnectTimeout, SQLINTEGER iLoginTimeout = SQL_NTS);

	const SQLCHAR* ConnectString(SQLHDBC dbc, SQLCHAR *lpszConnectID);

	SQLRETURN Disconnect(SQLHDBC dbc);
	SQLRETURN Begin(SQLHDBC dbc);
	SQLRETURN Commit(SQLHDBC dbc, BOOL rollback = false);
}; static ThreadLocalStorage<_SQL> SQL;


SQLHDBC _SQL::Connect(SQLCHAR *lpszConnectString, SQLINTEGER iConnectTimeout, SQLINTEGER iLoginTimeout)
{
	struct SQLDBC* R = new SQLDBC();

	std::stringset s; std::tokenize((const char *)lpszConnectString, s, ";");
	for (std::stringset::iterator it = s.begin(); it != s.end(); it++)
	{
		if (*((*it).c_str() + 0) != '.')
			R->dsn += (SQLCHAR *)((*it) + ";").c_str();
		else {
			char *v = (char *)strchr((*it).c_str(), '=');
			if (v != NULL)
			{
				*v++ = 0;

				SQLCustomSet::iterator c_it = R->custom.find((*it).c_str());
				if (c_it == R->custom.end())
					R->custom.insert(SQLCustomSet::value_type((*it).c_str(), v));
				else {
					c_it->second.assign(v);
				}
			}
		}
	}

	R->dbc = _SQLConnectEx((SQLCHAR *)R->dsn.c_str(), iConnectTimeout, iLoginTimeout);
	if (R->dbc == NULL)
		delete R;
	else {
		R->ref = 1;

		R->T.ref = 0;
		R->custom.insert(SQLCustomSet::value_type("", (const char *)lpszConnectString));
		{
			_dbc.insert(SQLDBC_CACHED::value_type(R->dbc, R));
		}
		return R->dbc;
	}
	return NULL;
}


const SQLCHAR *_SQL::ConnectString(SQLHDBC dbc, SQLCHAR *lpszExtraID)
{
	SQLDBC_CACHED::iterator it = _dbc.find(dbc);
	if (it == _dbc.end()) return NULL;
	{
		SQLDBC *R = it->second;

		if (lpszExtraID != NULL)
		{
			SQLCustomSet::iterator c_it = R->custom.find((char *)lpszExtraID);
			return (c_it == R->custom.end()) ? NULL : (SQLCHAR *)c_it->second.c_str();
		}

		return R->dsn.c_str();
	}
}


SQLRETURN _SQL::Disconnect(SQLHDBC dbc)
{
	SQLDBC_CACHED::iterator p_it = _dbc.find(dbc);
	if (p_it != _dbc.end())
	{
		struct SQLDBC* R = p_it->second;

        if ((--R->ref) > 0)
			return SQL_SUCCESS;
		else
		{
			delete R;
			_dbc.erase(p_it);
		}
	}
	return _SQLDisconnectEx(dbc);
}


SQLRETURN _SQL::Begin(SQLHDBC dbc)
{
	SQLDBC_CACHED::iterator p_it = _dbc.find(dbc);
	if (p_it == _dbc.end())
		return SQL_ERROR;
	else {
		struct SQLDBC* R = p_it->second;

		if (R->T.ref > 0);
		else {
			const SQLRETURN rc = SQLSetAttrs(dbc, SQL_HANDLE_DBC,
			                                 SQL_ATTR(AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, SQL_NTS), SQL_NTS);
			// fprintf(stderr, " * SQL: BEGIN - '%s' %d\n", (char *)R->I->i.c_str(), rc);
			if (rc != SQL_SUCCESS)
				return rc;
		}
		R->T.ref++;
	}
	return SQL_SUCCESS;
}


SQLRETURN _SQL::Commit(SQLHDBC dbc, BOOL rollback)
{
	SQLDBC_CACHED::iterator p_it = _dbc.find(dbc);
	if (p_it == _dbc.end())
		return SQL_ERROR;
	else {
		struct SQLDBC* R = p_it->second;

		if (R->T.ref == 0) return SQL_SUCCESS;
		else if (rollback == SQL_NTS) R->T.ref = 1;
		{
            SQLRETURN rc = SQLEndTran(SQL_HANDLE_DBC, R->dbc, (rollback == true) ? SQL_ROLLBACK : SQL_COMMIT);
            if ((--R->T.ref) == 0)
                SQLSetAttrs(R->dbc, SQL_HANDLE_DBC,SQL_ATTR(AUTOCOMMIT, SQL_AUTOCOMMIT_ON, SQL_NTS), SQL_NTS);
            return rc;
		}
	}
}


SQLHDBC SQLConnectEx(SQLCHAR *szDsn, SQLINTEGER connectionTimeout, SQLINTEGER loginTimeout) {
	return SQL->Connect(szDsn, connectionTimeout, loginTimeout);
}

SQLCHAR  *SQLConnectString(SQLHDBC dbc, SQLCHAR *lpszExtraID) {
	return (SQLCHAR *)SQL->ConnectString(dbc, lpszExtraID);
}

SQLRETURN SQLDisconnectEx(SQLHDBC dbc) { return SQL->Disconnect(dbc); }
SQLRETURN SQLBegin(SQLHDBC dbc) { return SQL->Begin(dbc); }
SQLRETURN SQLCommit(SQLHDBC dbc, BOOL rollback) { return SQL->Commit(dbc, rollback); }

/* @} */