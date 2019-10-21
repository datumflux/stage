#!/bin/sh

CMAKE_OPTIONS="-DBUILD_MODULE=ON -DBUILD_SHARED_LIBS=OFF -DWITH_SHARED_LIBUV=OFF -DWITH_LUA_ENGINE=Lua -DLUA_BUILD_TYPE=System -DLUA_COMPAT53_DIR=deps/lua-compat-5.3 -DLUA=../../bin/lua -DLUA_INCDIR=../../include -DLUA_LIBDIR=../../lib -DINSTALL_LIB_DIR=../../bin"

if [ ! -d "luv" ]; then
	git clone https://github.com/luvit/luv.git --recursive
fi

cd luv
git pull --rebase
rm -rf build
cmake -H. -Bbuild ${CMAKE_OPTIONS}
cmake --build build --config Release 

if [ -f build/luv.so ] && [ -d "../../../bin/lib/lua" ]; then
	echo "Install luv.so"
	cp build/luv.so ../../../bin/lib/lua/
fi
