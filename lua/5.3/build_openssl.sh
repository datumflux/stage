#!/bin/sh

PATCH0="../lua-openssl.patch"
if [ ! -d "lua-openssl" ]; then
	git clone --recurse https://github.com/zhaozg/lua-openssl.git
fi

cd lua-openssl
patch -p1 -f < ${PATCH0}

make
if [ -f openssl.so ] && [ -d "../../../bin/lib/lua" ]; then
	echo "Install openssl.so"
	cp openssl.so ../../../bin/lib/lua/
fi
