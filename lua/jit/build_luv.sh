#!/bin/sh

CMAKE_OPTIONS="-DBUILD_MODULE=ON -DBUILD_SHARED_LIBS=OFF -DWITH_SHARED_LIBUV=OFF -DWITH_LUA_ENGINE=LuaJIT -DLUA_BUILD_TYPE=System -DLUA=../luajit-2.0/src/lua -DLUA_INCDIR=../luajit-2.0/src -DLUA_LIBDIR=../luajit-2.0/src -DINSTALL_LIB_DIR=../../bin"

if [ ! -d "luv" ]; then
	git clone https://github.com/luvit/luv.git --recursive
fi

cd luv
git pull --rebase
rm -rf build

cmake -H. -Bbuild ${CMAKE_OPTIONS}
cmake --build build --config Release 

if [ -f build/luv.so ] && [ -d "../../../bin/lib/luajit" ]; then
	echo "Install luv.so"
	cp build/luv.so ../../../bin/lib/luajit/
fi
