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
#ifndef __STRING_FORMAT_HPP
#  define __STRING_FORMAT_HPP
/*!
 * COPYRIGHT 2018-2019 DATUMFLUX CORP.
 *
 * \author KANG SHIN-SUK <kang.shinsuk@datumflux.co.kr>
 */
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <deque>
#include <map>

#include <algorithm>

namespace std {
	inline string format_arg_list( const char *fmt, va_list args)
	{
		if (!fmt)
			;
		else {
			int length = BUFSIZ;
	_gW:	{
				char *buffer = (char *)alloca( length);
				if (buffer == NULL)
					return "";
				else {
					if (vsnprintf( buffer, length - 1, fmt, args) >= 0)
						return string( buffer);
				} length += BUFSIZ;
			} goto _gW;
		} return "";
	}

	inline string format( const string fmt, ...)
	{
		va_list args;

		va_start( args, fmt);
		string s = format_arg_list( fmt.c_str(), args);
		va_end( args);
		return s;
	}

	inline string trim( const char *value, int len) {
#define TRIM_WHITESPACE				"\r\n\t "
		int offset;
		while ((--len) >= 0)
        {
            if (!strchr( TRIM_WHITESPACE, value[len]))
                break;
        }
		{
			for (offset = 0; (offset <= len) &&
					strchr( TRIM_WHITESPACE, value[offset]); offset++);
#undef TRIM_WHITESPACE
		} return string( value + offset, (len - offset) + 1);
	}

	inline string trim( const string &str, int len = -1) {
		((len <= 0) ? (len = (int)str.length()): 0);
		return trim( str.c_str(), len);
	}

	typedef deque<string> stringset;
    typedef map<string, string> stringmap;

	inline size_t tokenize( const string &s,
			stringset &tokens, const string &delimiters = " ")
	{
		string::size_type last = s.find_first_not_of( delimiters, 0);
		string::size_type pos = s.find_first_of( delimiters, last);

		while ((string::npos != pos) ||
					(string::npos != last))
		{
			/*
			int length = pos - last;
			if (length > 0)
			{
				std::string v = s.substr( last, length);
				tokens.push_back( v.c_str());
			}
			else tokens.push_back( "");
			*/
			tokens.push_back(s.substr(last, (pos - last)).c_str());
			last = s.find_first_not_of( delimiters, pos);
			pos = s.find_first_of( delimiters, last);
		}
		return tokens.size();
	}

	inline string &replace( string &s, const char *f, const char *r)
	{
		size_t pos, len = strlen( f);

		while ((pos = s.find( f)) != string::npos)
			s.replace( pos, len, r);
		return s;
	}

	inline string tolower( string s)
	{
		std::transform( s.begin(), s.end(), s.begin(), ::tolower);
		return s;
	}

	inline string random(size_t length)
	{
		auto randchar = []() -> char
		{
			const char charset[] =
					"0123456789"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz";
			const size_t max_index = (sizeof(charset) - 1);
			return charset[rand() % max_index ];
		};

		std::string str(length,0);
		std::generate_n(str.begin(), length, randchar);
		return str;
	}
};
#endif
