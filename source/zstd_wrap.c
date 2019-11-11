#include <lauxlib.h>
#include <lua.h>
#include "zstd.h"

static int decompress(lua_State* L)
{
  size_t len = 0;
  char*       dst     = (char*)      luaL_checklstring(L, 1, &len);
  size_t      dstSize = (size_t)     luaL_checkinteger(L, 2      );
  const char* src     = (const char*)luaL_checklstring(L, 3, &len);
  size_t      srcSize = (size_t)     luaL_checkinteger(L, 4      );

  size_t result = ZSTD_decompress(dst, dstSize, src, srcSize);
  lua_pushinteger(L, result);
  return 1;
}

static int isError(lua_State* L)
{
  size_t code = (size_t)luaL_checkinteger(L, 1);
  unsigned result = ZSTD_isError(code);
  lua_pushboolean(L, result);
  return 1;
}

static const luaL_Reg zstd_functions[] = {
  { "decompress", decompress },
  { "isError",    isError    },
  { NULL,         NULL       }
};

LUALIB_API int luaopen_zstd(lua_State* L)
{
#if LUA_VERSION_NUM >= 502
  luaL_newlib(L, zstd_functions);
#else
  luaL_register(L, "zstd", zstd_functions);
#endif
  return 1;
}

