AM_CFLAGS = -Wall

include_HEADERS = lua.h lualib.h lauxlib.h lua.hpp

nodist_include_HEADERS = luaconf.h

lib_LTLIBRARIES = liblua.la
liblua_la_LDFLAGS = -release @MAJOR_VERSION@
liblua_la_SOURCES = \
	lapi.c lauxlib.c lbaselib.c lbitlib.c lcode.c lcorolib.c lctype.c ldblib.c \
	ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c lmathlib.c lmem.c \
	loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c lstrlib.c \
	ltable.c ltablib.c ltm.c lundump.c lutf8lib.c lvm.c lzio.c \
	lapi.h lcode.h lctype.h ldebug.h ldo.h lfunc.h lgc.h llex.h llimits.h \
	lmem.h lobject.h lopcodes.h lparser.h lstate.h lstring.h ltable.h ltm.h \
	lundump.h lvm.h lzio.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lua.pc

bin_PROGRAMS = lua luac

lua_SOURCES = lua.c
lua_LDADD = liblua.la @LUA_LIBS@ @READLINE_LIBS@
lua_DEPENDENCIES = liblua.la

luac_SOURCES = luac.c
# Statically link liblua against luac since luac uses symbols not exported in liblua
luac_LDADD = .libs/liblua.a @LUA_LIBS@
luac_DEPENDENCIES = liblua.la

EXTRA_DIST = luaconf.h.template
BUILT_SOURCES = luaconf.h
CLEANFILES = luaconf.h luaconf.h.template

readline_defs = @READLINE_DEFS@

edit = 	sed \
	  -e 's,%prefix%,$(prefix),g' \
	  -e 's,%lua_datadir%,$(datadir),g' \
	  -e 's,%lua_libdir%,$(libdir),g'

luaconf.h : luaconf.h.template
	rm -f $@ $@.tmp
	$(edit) $< >$@.tmp
	mv $@.tmp $@
