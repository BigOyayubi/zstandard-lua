//#define  ENABLE_LOG

#include <lauxlib.h>
#include <lua.h>
#include "zstd.h"
#include "decompress/zstd_decompress_internal.h"

#ifdef ENABLE_LOG
  #include <stdio.h>
  #define debug_log( fmt, ... ) \
    fprintf( stderr, fmt, ##__VA_ARGS__ )
#else
  #define debug_log( fmt, ... ) ((void)0)
#endif

#if LUA_VERSION_NUM >= 502
  #define LUA_REGISTER(l, n, f) \
    luaL_newlib(l, f)
#else
  #define LUA_REGISTER(l, n, f) \
    luaL_register(l, n, f);
#endif
 

/***************************************************************
 * Basic functions 
 * *************************************************************/

/* 
 * check is error that zstd function return value
 */
static int isError(lua_State* L)
{
  size_t code = (size_t)luaL_checkinteger(L, 1);
  unsigned result = ZSTD_isError(code);
  lua_pushboolean(L, result);
  return 1;
}

/*
 * Decompress without dictionary
 * lua sample
 *   local zstd = require("zstd")
 *   local dst = (" "):rep(256)
 *   local f = io.open("path/to/src")
 *   local src = f:read("*a")
 *   f:close()
 *
 *   local result = zstd.decompress(dst, string.len(dst), src, string.len(src))
 *   local isError = zstd.isError(result)
 */
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

/*
 * Compress without dictionary
 * lua sample
 *   local zstd = require('zstd')
 *   local src = ("abc"):rep(1000)
 *   local compressedSize, compressed = zstd.compress(src, string.len(src))
 */
static int compress(lua_State* L)
{
  size_t len = 0;
  const char* src     = (const char*)luaL_checklstring(L, 1, &len);
  size_t      srcSize = (size_t)     luaL_checkinteger(L, 2      );
  int         level   = (int)        luaL_optinteger  (L, 3, 1   );

  debug_log("src     : %p\n", src);
  debug_log("srcSize : %zu\n", srcSize);
  debug_log("level   : %d\n", level);

  size_t const dstSize = ZSTD_compressBound(srcSize);

  debug_log("dstSize : %zu\n", dstSize);

  luaL_Buffer lbuf;
  char* cbuf = luaL_buffinitsize(L, &lbuf, dstSize);

  debug_log("cbuf    : %p\n", cbuf);

  size_t result = ZSTD_compress(cbuf, dstSize, src, srcSize, level);
  if(ZSTD_isError(result))
  {
    lua_pushliteral(L, "call to ZSTD_compress failed");
    lua_error(L);
  }

  debug_log("result  : %zu\n", result);
  lua_pushinteger(L, result);
  luaL_pushresultsize(&lbuf, result);
  return 2;
}

/***************************************************************
 * Dictionary Decompress
 * *************************************************************/

typedef struct
{
  ZSTD_DDict* ddict;
  ZSTD_DCtx*  dctx;
} zstd_decompress_dict_t;
#define ZSTD_DICTHANDLE "zstd.decomp_dictionary"

static zstd_decompress_dict_t* _get_zstd_decompress_dict_t(lua_State *L, int index)
{
  return (zstd_decompress_dict_t*)luaL_checkudata(L, index, ZSTD_DICTHANDLE);
}

static int zstd_decompress_dict_tostring(lua_State *L)
{
  zstd_decompress_dict_t* context = _get_zstd_decompress_dict_t(L, 1);
  lua_pushfstring(L, "%s (%p)", ZSTD_DICTHANDLE, context);
  return 1;
}

static int zstd_decompress_dict_gc(lua_State *L)
{
  zstd_decompress_dict_t* context = _get_zstd_decompress_dict_t(L, 1);
  ZSTD_freeDDict(context->ddict);
  ZSTD_freeDCtx(context->dctx);
  return 0;
}

static int decompressDict(lua_State *L)
{
  zstd_decompress_dict_t* context = _get_zstd_decompress_dict_t(L, 1);

  size_t len = 0;
  char*       dst      = (char*)      luaL_checklstring(L, 2, &len);
  size_t      dstSize  = (size_t)     luaL_checkinteger(L, 3      );
  const char* src      = (const char*)luaL_checklstring(L, 4, &len);
  size_t      srcSize  = (size_t)     luaL_checkinteger(L, 5      );

  debug_log("context->dctx    %p\n", context->dctx);
  debug_log("context->ddict   %p\n", context->ddict);
  debug_log("dst     %p\n", dst);
  debug_log("dstSize %zu\n", dstSize);
  debug_log("src     %p\n", src);
  debug_log("srcSize %zu\n", srcSize);

  size_t result = ZSTD_decompress_usingDDict(context->dctx, dst, dstSize, src, srcSize, context->ddict);
  debug_log("result %zu\n", result);
  if(result != dstSize)
  {
    lua_pushliteral(L, "call to ZSTD_decompress_usingDDict failed");
    lua_error(L);
  }

  lua_pushinteger(L, result);
  return 1;
}

static const luaL_Reg decompress_dict_functions[] = {
  { "decompressDict", decompressDict },
  { NULL,             NULL           }
};

/*
 * load zstd dictionary
 * lua sample
 *   local zstd = require("zstd")
 *   local f = io.open("path/to/dict")
 *   local data = f:read("*a")
 *   f:close()
 *   local dataSize = string.len(data)
 *   local dict = zstd.loadDictionary( data, dataSize )
 *   f = io.open("path/to/src")
 *   local src = f:read("*a")
 *   f:close()
 *   local srcSize = string.len(src)
 *   local dst = (" "):rep(100)
 *   local dstSize = string.len(dst)
 *   local result = dict:decompressDict(dst, dstSize, src, srcSize)
 *   local isError = zstd.isError( result )
 */
static int loadDictionary(lua_State* L)
{
  size_t len = 0;
  const char* data = (const char*)luaL_checklstring(L, 1, &len);
  size_t      dataSize = (size_t)luaL_checkinteger(L, 2);

  ZSTD_DDict* const ddict = ZSTD_createDDict(data, dataSize);
  if(ddict == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createDDict failed");
    lua_error(L);
  }

  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  if(dctx == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createDCtx failed");
    lua_error(L);
  }

  debug_log("dctx    %p\n", dctx);
  debug_log("ddict   %p\n", ddict);
 
  zstd_decompress_dict_t* context = (zstd_decompress_dict_t*)lua_newuserdata(L, sizeof(zstd_decompress_dict_t));
  context->ddict = ddict;
  context->dctx  = dctx;
  if(luaL_newmetatable(L, ZSTD_DICTHANDLE))
  {
    // new method table
    LUA_REGISTER(L, ZSTD_DICTHANDLE, decompress_dict_functions);

    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, zstd_decompress_dict_tostring);
    lua_setfield(L, -2, "__tostring");
    
    // metatable.__gc
    lua_pushcfunction(L, zstd_decompress_dict_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

static const luaL_Reg zstd_functions[] = {
    {"compress",   compress},
    {"decompress", decompress},
    {"isError", isError},
    {"loadDictionary", loadDictionary},
    {NULL, NULL}};

LUALIB_API int luaopen_zstd(lua_State* L)
{
  LUA_REGISTER(L, "zstd", zstd_functions);
  return 1;
}

