#!/bin/sh

SOURCE0="LuaJIT-2.0.5.tar.gz"

PATCH0="../LuaJIT-2.0.5-path.patch"
PATCH1="../LuaJIT-2.0.5-compat.patch"

BUILD_ROOT="LuaJIT-2.0.5"

# rm -rf ${BUILD_ROOT}
# tar xvzf ${SOURCE0}
# cd ${BUILD_ROOT}
if [ ! -d "luajit-2.0" ]; then
	git clone git://repo.or.cz/luajit-2.0.git --recursive
fi

cd luajit-2.0
patch -p1 -z .path < ${PATCH0}
patch -p1 -z .compat < ${PATCH1}

make all
make DESTDIR=$PWD install

cd usr/local

rm -rf bin/man
tar cf - . | (cd ../../../; tar xfp -)
cd ../../../

if [ -f bin/luajit-2.0.5 ] && [ -d "../../bin" ]; then
	echo "Install luajit"
	cp bin/luajit ../../bin
	if [ -f lib/libluajit-5.1.so.2 ] && [ -d "../../bin/lib" ]; then
		echo "Install libluajit-5.1.so.2"
		cp lib/libluajit-5.1.so.2 ../../bin/lib
	fi
fi
