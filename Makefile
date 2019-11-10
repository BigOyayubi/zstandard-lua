.DEFAULT_GOAL := help


## Install dependencies
prepare:
	go get github.com/Songmu/make2help/cmd/make2help
	(cd xLua && git clean -fd && git reset --hard HEAD)
	cp CMakeLists.txt xLua/build/

## Build All
all: prepare linux32-lua53 linux32-luajit linux64-lua53 linux64-luajit

## Build linux 32bit Lua53
linux32-lua53: prepare
	cd xLua/build/ && sh make_linux32_lua53.sh

## Build linux 32bit LuaJIT
linux32-luajit: prepare
	cd xLua/build/ && sh make_linux32_luajit.sh

## Build linux 64bit Lua53
linux64-lua53: prepare
	cd xLua/build/ && sh make_linux64_lua53.sh

## Build linux 64bit LuaJIT
linux64-luajit: prepare
	cd xLua/build/ && sh make_linux64_luajit.sh



## Show help
help: prepare
	@make2help $(MAKEFILE_LIST)

.PHONY: prepare help

