#!/bin/sh

SOURCE0="lua-5.3.5.tar.gz"
SOURCE4="../luaconf.h"

PATCH0="../lua-5.3.0-autotoolize.patch"
PATCH1="../lua-5.3.0-idsize.patch"
PATCH3="../lua-5.2.2-configure-linux.patch"
PATCH4="../lua-5.3.0-configure-compat-module.patch"
PATCH9="../CVE-2019-6706-use-after-free-lua_upvaluejoin.patch"

PATCH98="../lua-5.3.x-undump.patch"
PATCH99="../lua-5.3.5-luaconf.patch"
PATCH100="../lua-5.3.5-path.patch"

BUILD_ROOT="lua-5.3.5"

rm -rf ${BUILD_ROOT}
tar xvzf ${SOURCE0}

cd ${BUILD_ROOT}
patch -p1 -z .luaconf < ${PATCH99}
mv src/luaconf.h src/luaconf.h.template.in
#
patch -p1 -E -z .autocxx < ${PATCH0}
patch -p1 -z .idsize < ${PATCH1}
patch -p1 -z .configure-linux < ${PATCH3}
patch -p1 -z .configure-compat-all < ${PATCH4}
patch -p1 -z .CVE-2019-6706 < ${PATCH9}
#
# patch -p0 -z .undump < ${PATCH98}
#
sed -i 's|5.3.0|5.3.5|g' configure.ac
sed -i 's|LUA_USE_LINUX|LUA_USE_POSIX|g' configure.ac
sed -i 's|LUA_DL_DLOPEN|LUA_USE_DLOPEN|g' configure.ac
autoreconf -ifv
#

patch -p1 -z .path < ${PATCH100}
#
./configure --without-readline --with-compat-module
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool
# Autotools give me a headache sometimes.
sed -i 's|/usr/local/||g' src/luaconf.h.template
sed -i 's|@pkgdatadir@/||g' src/luaconf.h.template
sed -i 's|${exec_prefix}/||g' src/luaconf.h.template

# hack so that only /usr/bin/lua gets linked with readline as it is the
# only one which needs this and otherwise we get License troubles
make LIBS="-lm -ldl"
# only /usr/bin/lua links with readline now #luac_LDADD="liblua.la -lm -ldl"

make DESTDIR=$PWD/ROOT install
#
mv ROOT/usr/local/include/luaconf.h ROOT/usr/local/include/luaconf-`arch`.h
cp ${SOURCE4} ROOT/usr/local/include

cd ROOT/usr/local

rm -rf share
tar cf - . | (cd ../../../../; tar xfp -)
cd ../../../../

if [ -f bin/lua ] && [ -d "../../bin" ]; then
	echo "Install lua"
	cp bin/lua* ../../bin
	if [ -f lib/liblua-5.3.so ] && [ -d "../../bin/lib" ]; then
		echo "Install liblua-5.3.so"
		cp lib/liblua-5.3.so ../../bin/lib
	fi
fi
