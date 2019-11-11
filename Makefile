.DEFAULT_GOAL := help

# for xLua

## Install dependencies
prepare:
	go get github.com/Songmu/make2help/cmd/make2help
	cp CMakeLists.txt xLua/build/

## Build linux 32bit Lua53 in xLua
linux32-lua53: prepare
	cd xLua/build/ && sh make_linux32_lua53.sh

## Build linux 32bit LuaJIT in xLua
linux32-luajit: prepare
	cd xLua/build/ && sh make_linux32_luajit.sh

## Build linux 64bit Lua53 in xLua
linux64-lua53: prepare
	cd xLua/build/ && sh make_linux64_lua53.sh

## Build linux 64bit LuaJIT in xLua
linux64-luajit: prepare
	cd xLua/build/ && sh make_linux64_luajit.sh

## Build OSX Lua53 in xLua
osx-lua53: prepare
	cd xLua/build/ && sh make_osx_lua53.sh

## Build OSX LuaJIT in xLua
osx-luajit: prepare
	cd xLua/build/ && sh make_osx_luajit.sh

## Build iOS Lua53 in xLua
ios-lua53: prepare
	cd xLua/build/ && sh make_ios_lua53.sh

## Build iOS LuaJIT in xLua
ios-luajit: prepare
	cd xLua/build/ && sh make_ios_luajit.sh



# for local test 

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
LUA_INCDIR ?= /usr/include/lua5.1
LUA_LIBDIR ?= /usr/lib
LIBFLAGS   ?= -shared
endif
ifeq ($(UNAME), Darwin)
LUA_INCDIR  ?= /usr/local/opt/lua/include/lua5.3
LUA_LIBDIR  ?= /usr/local/opt/lua/lib
LIBFLAGS    ?= -bundle -undefined dynamic_lookup -all_load
INST_LIBDIR ?= /usr/local/lib/lua/5.3
endif

LUALIB     ?= zstd.so
LUA_CFLAGS ?= -O2 -fPIC

ZSTDOBJS    = zstd/lib/decompress/zstd_ddict.o zstd/lib/decompress/huf_decompress.o \
              zstd/lib/decompress/zstd_decompress.o zstd/lib/decompress/zstd_decompress_block.o \
	      zstd/lib/legacy/zstd_v05.o zstd/lib/legacy/zstd_v01.o \
	      zstd/lib/legacy/zstd_v06.o zstd/lib/legacy/zstd_v02.o \
	      zstd/lib/legacy/zstd_v07.o zstd/lib/legacy/zstd_v03.o \
	      zstd/lib/legacy/zstd_v04.o zstd/lib/common/entropy_common.o \
	      zstd/lib/common/fse_decompress.o zstd/lib/common/debug.o \
	      zstd/lib/common/xxhash.o zstd/lib/common/pool.o \
	      zstd/lib/common/threading.o zstd/lib/common/zstd_common.o zstd/lib/common/error_private.o \

CMOD        = $(LUALIB)
OBJS        = source/zstd_wrap.o

CFLAGS      = $(LUA_CFLAGS) -I$(LUA_INCDIR) -Izstd/lib -Izstd/lib/common
CXXFLAGS    = $(LUA_CFLAGS) -I$(LUA_INCDIR) -Izstd/lib -Izstd/lib/common
LDFLAGS     = $(LIBFLAGS) -L$(LUA_LIBDIR)

## install module to local test
install: $(CMOD)
	cp $(CMOD) $(INST_LIBDIR)

## uninstall module to local test
uninstall:
	$(RM) $(INST_LIBDIR)/$(CMOD)

## clean
clean:
	(cd zstd/ && git clean -fd && git reset --hard HEAD)
	(cd xLua/ && git clean -fd && git reset --hard HEAD)
	$(RM) $(CMOD) $(OBJS) $(ZSTDOBJS)

## clean all
cleanall: clean uninstall

## Build module to local test
zstd.so: $(OBJS) $(ZSTDOBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(ZSTDOBJS) -o $(CMOD)

## Show help
help: prepare
	@make2help $(MAKEFILE_LIST)

.PHONY: prepare help

