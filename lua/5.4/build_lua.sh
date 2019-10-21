#!/bin/sh

SOURCE0="lua-5.4.0-beta-rc1.tar.gz"
SOURCE4="../luaconf.h"

PATCH0="../lua-5.4.0-autotoolize.patch"
PATCH1="../lua-5.3.0-idsize.patch"
PATCH3="../lua-5.4.0-configure-linux.patch"
PATCH4="../lua-5.4.0-configure-compat-module.patch"

BUILD_ROOT="lua-5.4.0-beta"

rm -rf ${BUILD_ROOT}
tar xvzf ${SOURCE0}

cd ${BUILD_ROOT}
mv src/luaconf.h src/luaconf.h.template.in
#
patch -p1 -E -z .autocxx < ${PATCH0}
patch -p1 -z .idsize < ${PATCH1}
patch -p1 -z .configure-linux < ${PATCH3}
patch -p1 -z .configure-compat-all < ${PATCH4}
#
autoreconf -ifv
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

# cd usr/local
# tar cf - . | (cd ../../../../; tar xfp -)
