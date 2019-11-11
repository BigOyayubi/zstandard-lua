package = "zstandard-lua"
version = "dev-1"
source = {
   url = "git+https://github.com/BigOyayubi/zstandard-lua"
}
description = {
   summary = "zstd interface for Lua.",
   detailed = [[
      zstd interface for Lua.
      This project mainly uses with Unity and xLua.
      This project is hosted on github.
   ]],
   homepage = "https://github.com/BigOyayubi/zstandard-lua",
   license = "MIT"
}
dependencies = {
   "lua >= 5.1, < 5.4"
}

build = {
   type = "make",
   build_variables = {
      LUA_CFLAGS="$(CFLAGS)",
      LIBFLAG="$(LIBFLAG)",
      LUA_LIBDIR="$(LUA_LIBDIR)",
      LUA_BINDIR="$(LUA_BINDIR)",
      LUA_INCDIR="$(LUA_INCDIR)",
      LUA="$(LUA)",
   },
   install_variables = {
      INST_PREFIX="$(PREFIX)",
      INST_BINDIR="$(BINDIR)",
      INST_LIBDIR="$(LIBDIR)",
      INST_LUADIR="$(LUADIR)",
      INST_CONFDIR="$(CONFDIR)",
   }
}

