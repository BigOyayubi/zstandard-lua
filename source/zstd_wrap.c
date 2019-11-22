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
 *   local compressed = zstd.compress(src, string.len(src))
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
  luaL_pushresultsize(&lbuf, result);
  return 1;
}

/***************************************************************
 * Streaming Decompress
 * *************************************************************/
typedef struct
{
  ZSTD_DCtx* dctx;
  const char* src;
  size_t srcSize;
  size_t readSrcSize;
} zstd_stream_decompress_t;
#define ZSTD_STREAM_DECOMPRESS_HANDLE "zstd.stream_decomp"

static zstd_stream_decompress_t *_get_zstd_stream_decompress_t(lua_State *L, int index)
{
  return (zstd_stream_decompress_t *)luaL_checkudata(L, index, ZSTD_STREAM_DECOMPRESS_HANDLE);
}

static int zstd_stream_decompress_tostring(lua_State *L)
{
  zstd_stream_decompress_t *context = _get_zstd_stream_decompress_t(L, 1);
  lua_pushfstring(L, "%s (%p)", ZSTD_STREAM_DECOMPRESS_HANDLE, context);
  return 1;
}

static int zstd_stream_decompress_gc(lua_State *L)
{
  zstd_stream_decompress_t *context = _get_zstd_stream_decompress_t(L, 1);
  ZSTD_freeDCtx(context->dctx);
  return 0;
}

static int setDecompressStream(lua_State* L)
{
  zstd_stream_decompress_t *context = _get_zstd_stream_decompress_t(L, 1);
  size_t len = 0;
  char *src = (char *)luaL_checklstring(L, 2, &len);
  size_t srcSize = (size_t)luaL_checkinteger(L, 3);

  ZSTD_DCtx_reset(context->dctx, ZSTD_reset_session_only);
  context->src = src;
  context->srcSize = srcSize;
  context->readSrcSize = 0;

  debug_log("src %p\n", context->src);
  debug_log("srcSize %zu\n", context->srcSize);

  return 0;
}

static int decompressStream(lua_State *L)
{
  zstd_stream_decompress_t *context = _get_zstd_stream_decompress_t(L, 1);

  if(context->src == NULL)
  {
    lua_pushliteral(L, "call to decompressStream failed. src is NULL");
    lua_error(L);
  }
  if(context->readSrcSize >= context->srcSize)
  {
    lua_pushstring(L, "");
    lua_pushboolean(L, 1);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 4;
  }

  size_t buffInSize = ZSTD_DStreamInSize();

  const char* const readPtr = context->src + context->readSrcSize;

  size_t readSize = context->srcSize - context->readSrcSize;
  if (readSize > buffInSize)
  {
    readSize = buffInSize;
  }

  ZSTD_inBuffer input = {readPtr, readSize, 0};
  context->readSrcSize += readSize;

  luaL_Buffer buff;
  luaL_buffinit(L, &buff);

  size_t lastRet;
  size_t decompressSize = 0;
  while (input.pos < input.size)
  {
    char *tmp = luaL_prepbuffer(&buff);
    ZSTD_outBuffer output = {tmp, LUAL_BUFFERSIZE, 0};
    // when return code is zero, the frame is complete
    size_t const ret = ZSTD_decompressStream(context->dctx, &output, &input);
    if(ZSTD_isError(ret))
    {
      lua_pushliteral(L, "call to ZSTD_decompressStream failed");
      lua_error(L);
    }
    lastRet = ret;
    luaL_addsize(&buff, output.pos);
    decompressSize += output.pos;
  }

  if(lastRet != 0)
  {
    lua_pushliteral(L, "call to ZSTD_decompressStream failed");
    lua_error(L);
  }

  luaL_pushresult(&buff);
  lua_pushboolean(L, context->readSrcSize >= context->srcSize);
  lua_pushinteger(L, readSize);
  lua_pushinteger(L, decompressSize);
  return 4;
}

static const luaL_Reg decompress_stream_functions[] = {
    {"set",        setDecompressStream},
    {"decompress", decompressStream},
    {NULL, NULL}
};

/*
 * load zstd decompress stream
 * lua sample
 *   local zstd = require("zstd")
 *   local stream = zstd.createDecompressStream()
 *   local compressed = zstd.compress( text, #text )
 *   local compressedSize = string.len(compressed)
 *   stream:set(compressed, compressedSize)
 *   repeat
 *     local decompressed, done, read_in, read_out = stream:decompress()
 *     -- do xxx
 *   until done
 */
static int createDecompressStream(lua_State *L)
{
  ZSTD_DCtx *const dctx = ZSTD_createDCtx();
  if (dctx == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createDCtx failed");
    lua_error(L);
  }
  debug_log("dctx    %p\n", dctx);

  zstd_stream_decompress_t *context = (zstd_stream_decompress_t *)lua_newuserdata(L, sizeof(zstd_stream_decompress_t));
  context->dctx = dctx;
  context->src = NULL;
  context->srcSize = 0;
  context->readSrcSize = 0;

  if (luaL_newmetatable(L, ZSTD_STREAM_DECOMPRESS_HANDLE))
  {
    // new method table
    LUA_REGISTER(L, ZSTD_STREAM_DECOMPRESS_HANDLE, decompress_stream_functions);

    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, zstd_stream_decompress_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, zstd_stream_decompress_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

/***************************************************************
 * Dictionary Decompress
 * *************************************************************/

typedef struct
{
  ZSTD_DDict* ddict;
  ZSTD_DCtx*  dctx;
} zstd_decompress_dict_t;
#define ZSTD_DECOMPRESS_DICTHANDLE "zstd.decomp_dictionary"

static zstd_decompress_dict_t* _get_zstd_decompress_dict_t(lua_State *L, int index)
{
  return (zstd_decompress_dict_t*)luaL_checkudata(L, index, ZSTD_DECOMPRESS_DICTHANDLE);
}

static int zstd_decompress_dict_tostring(lua_State *L)
{
  zstd_decompress_dict_t* context = _get_zstd_decompress_dict_t(L, 1);
  lua_pushfstring(L, "%s (%p)", ZSTD_DECOMPRESS_DICTHANDLE, context);
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
  if(ZSTD_isError(result))
  {
    lua_pushliteral(L, "call to ZSTD_decompress_usingDDict failed");
    lua_error(L);
  }

  lua_pushinteger(L, result);
  return 1;
}

static const luaL_Reg decompress_dict_functions[] = {
  { "decompress",     decompressDict },
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
 *   local dict = zstd.loadDecompressDictionary( data, dataSize )
 *   f = io.open("path/to/src")
 *   local src = f:read("*a")
 *   f:close()
 *   local srcSize = string.len(src)
 *   local dst = (" "):rep(100)
 *   local dstSize = string.len(dst)
 *   local result = dict:decompress(dst, dstSize, src, srcSize)
 *   local isError = zstd.isError( result )
 */
static int loadDecompressDictionary(lua_State* L)
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
  if(luaL_newmetatable(L, ZSTD_DECOMPRESS_DICTHANDLE))
  {
    // new method table
    LUA_REGISTER(L, ZSTD_DECOMPRESS_DICTHANDLE, decompress_dict_functions);

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

/***************************************************************
 * Streaming Compress
 * *************************************************************/
typedef struct
{
  ZSTD_CCtx *cctx;
  const char *src;
  size_t srcSize;
  size_t readSrcSize;
} zstd_stream_compress_t;
#define ZSTD_STREAM_COMPRESS_HANDLE "zstd.stream_comp"

static zstd_stream_compress_t *_get_zstd_stream_compress_t(lua_State *L, int index)
{
  return (zstd_stream_compress_t *)luaL_checkudata(L, index, ZSTD_STREAM_COMPRESS_HANDLE);
}

static int zstd_stream_compress_tostring(lua_State *L)
{
  zstd_stream_compress_t *context = _get_zstd_stream_compress_t(L, 1);
  lua_pushfstring(L, "%s (%p)", ZSTD_STREAM_COMPRESS_HANDLE, context);
  return 1;
}

static int zstd_stream_compress_gc(lua_State *L)
{
  zstd_stream_compress_t *context = _get_zstd_stream_compress_t(L, 1);
  ZSTD_freeCCtx(context->cctx);
  return 0;
}

static int setCompressStream(lua_State *L)
{
  zstd_stream_compress_t *context = _get_zstd_stream_compress_t(L, 1);
  size_t len = 0;
  char *src = (char *)luaL_checklstring(L, 2, &len);
  size_t srcSize = (size_t)luaL_checkinteger(L, 3);

  ZSTD_CCtx_reset(context->cctx, ZSTD_reset_session_only);
  context->src = src;
  context->srcSize = srcSize;
  context->readSrcSize = 0;

  debug_log("src %p\n", context->src);
  debug_log("srcSize %zu\n", context->srcSize);

  return 0;
}

static int compressStream(lua_State *L)
{
  zstd_stream_compress_t *context = _get_zstd_stream_compress_t(L, 1);

  if (context->src == NULL)
  {
    lua_pushliteral(L, "call to compressStream failed. src is NULL");
    lua_error(L);
  }
  if (context->readSrcSize >= context->srcSize)
  {
    lua_pushstring(L, "");
    lua_pushboolean(L, 1);
    lua_pushinteger(L, 0);
    lua_pushinteger(L, 0);
    return 4;
  }

  size_t buffInSize = ZSTD_CStreamInSize();

  const char *const readPtr = context->src + context->readSrcSize;

  size_t readSize = context->srcSize - context->readSrcSize;
  ZSTD_EndDirective mode = ZSTD_e_end;
  if (readSize > buffInSize)
  {
    readSize = buffInSize;
    mode = ZSTD_e_continue;
  }

  ZSTD_inBuffer input = {readPtr, readSize, 0};
  context->readSrcSize += readSize;

  luaL_Buffer buff;
  luaL_buffinit(L, &buff);

  size_t lastRet;
  size_t compressSize = 0;
  int finished;
  do
  {
    char *tmp = luaL_prepbuffer(&buff);
    ZSTD_outBuffer output = {tmp, LUAL_BUFFERSIZE, 0};

    // when return code is zero, the frame is complete
    size_t const remaining = ZSTD_compressStream2(context->cctx, &output, &input, mode);
    if (ZSTD_isError(remaining))
    {
      lua_pushliteral(L, "call to ZSTD_compressStream2 failed");
      lua_error(L);
    }

    compressSize += output.pos;
    luaL_addsize(&buff, output.pos);
    finished = (mode == ZSTD_e_end) ? (remaining == 0) : (input.pos == input.size);
  } while (!finished);

  if(input.pos != input.size)
  {
    lua_pushliteral(L, "Impossible: zstd only returns 0 when the input is completely consumed!");
    lua_error(L);
  }

  if (lastRet != 0)
  {
    lua_pushliteral(L, "call to ZSTD_compressStream failed");
    lua_error(L);
  }

  luaL_pushresult(&buff);
  lua_pushboolean(L, context->readSrcSize >= context->srcSize);
  lua_pushinteger(L, readSize);
  lua_pushinteger(L, compressSize);
  return 4;
}

static const luaL_Reg compress_stream_functions[] = {
    {"set", setCompressStream},
    {"compress", compressStream},
    {NULL, NULL}};

/*
 * load zstd compress stream
 * lua sample
 *   local zstd = require("zstd")
 *   local stream = zstd.createDecompressStream()
 *   local text = ("aaa"):rep(100)
 *   stream:set( text, #text )
 *   repeat
 *     local compressed, done, read_in, read_out = stream:compress()
 *     -- do xxx
 *   until done
 */
static int createCompressStream(lua_State *L)
{
  ZSTD_CCtx *const cctx = ZSTD_createCCtx();
  if (cctx == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createCCtx failed");
    lua_error(L);
  }
  debug_log("cctx    %p\n", cctx);

  zstd_stream_compress_t *context = (zstd_stream_compress_t *)lua_newuserdata(L, sizeof(zstd_stream_compress_t));
  context->cctx = cctx;
  context->src = NULL;
  context->srcSize = 0;
  context->readSrcSize = 0;

  if (luaL_newmetatable(L, ZSTD_STREAM_COMPRESS_HANDLE))
  {
    // new method table
    LUA_REGISTER(L, ZSTD_STREAM_COMPRESS_HANDLE, compress_stream_functions);

    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, zstd_stream_compress_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, zstd_stream_compress_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

/***************************************************************
 * Dictionary Compress
 * *************************************************************/

typedef struct
{
  ZSTD_CDict *cdict;
  ZSTD_CCtx *cctx;
} zstd_compress_dict_t;
#define ZSTD_COMPRESS_DICTHANDLE "zstd.comp_dictionary"

static zstd_compress_dict_t *_get_zstd_compress_dict_t(lua_State *L, int index)
{
  return (zstd_compress_dict_t *)luaL_checkudata(L, index, ZSTD_COMPRESS_DICTHANDLE);
}

static int zstd_compress_dict_tostring(lua_State *L)
{
  zstd_compress_dict_t *context = _get_zstd_compress_dict_t(L, 1);
  lua_pushfstring(L, "%s (%p)", ZSTD_COMPRESS_DICTHANDLE, context);
  return 1;
}

static int zstd_compress_dict_gc(lua_State *L)
{
  zstd_compress_dict_t *context = _get_zstd_compress_dict_t(L, 1);
  ZSTD_freeCDict(context->cdict);
  ZSTD_freeCCtx(context->cctx);
  return 0;
}

static int compressDict(lua_State *L)
{
  zstd_compress_dict_t *context = _get_zstd_compress_dict_t(L, 1);

  size_t len = 0;
  const char *src     = (const char *)luaL_checklstring(L, 2, &len);
  size_t      srcSize = (size_t)      luaL_checkinteger(L, 3);

  debug_log("context->cctx    %p\n", context->cctx);
  debug_log("context->cdict   %p\n", context->cdict);
  debug_log("src     %p\n",  src);
  debug_log("srcSize %zu\n", srcSize);

  size_t const dstSize = ZSTD_compressBound(srcSize);
  debug_log("dstSize : %zu\n", dstSize);

  luaL_Buffer lbuf;
  char *cbuf = luaL_buffinitsize(L, &lbuf, dstSize);
  debug_log("cbuf    : %p\n", cbuf);

  size_t result = ZSTD_compress_usingCDict(context->cctx, cbuf, dstSize, src, srcSize, context->cdict);
  if (ZSTD_isError(result))
  {
    lua_pushliteral(L, "call to ZSTD_compress_usingCDict failed");
    lua_error(L);
  }

  debug_log("result : %zu\n", result);
  luaL_pushresultsize(&lbuf, result);
  return 1;
}

static const luaL_Reg compress_dict_functions[] = {
    {"compress", compressDict},
    {NULL, NULL}
};

/*
 * load zstd compress dictionary
 * lua sample
 *   local zstd = require("zstd")
 *   local f = io.open("path/to/dict")
 *   local data = f:read("*a")
 *   f:close()
 *   local dataSize = string.len(data)
 *   local level = 1
 *   local dict = zstd.loadCompressDictionary( data, dataSize, level )
 *   f = io.open("path/to/src")
 *   local src = f:read("*a")
 *   f:close()
 *   local srcSize = string.len(src)
 *   local compressed = dict:compress(src, srcSize)
 */
static int loadCompressDictionary(lua_State *L)
{
  size_t len = 0;
  const char *data = (const char *)luaL_checklstring(L, 1, &len);
  size_t dataSize  = (size_t)      luaL_checkinteger(L, 2      );
  int    level     = (int)         luaL_optinteger  (L, 3, 1   );

  ZSTD_CDict *const cdict = ZSTD_createCDict(data, dataSize, level);
  if (cdict == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createCDict failed");
    lua_error(L);
  }

  ZSTD_CCtx *const cctx = ZSTD_createCCtx();
  if (cctx == NULL)
  {
    lua_pushliteral(L, "call to ZSTD_createCCtx failed");
    lua_error(L);
  }

  debug_log("cctx    %p\n", cctx);
  debug_log("cdict   %p\n", cdict);

  zstd_compress_dict_t *context = (zstd_compress_dict_t *)lua_newuserdata(L, sizeof(zstd_compress_dict_t));
  context->cdict = cdict;
  context->cctx  = cctx;
  if (luaL_newmetatable(L, ZSTD_COMPRESS_DICTHANDLE))
  {
    // new method table
    LUA_REGISTER(L, ZSTD_COMPRESS_DICTHANDLE, compress_dict_functions);

    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, zstd_compress_dict_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, zstd_compress_dict_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

static const luaL_Reg zstd_functions[] = {
    {"compress", compress},
    {"decompress", decompress},
    {"isError", isError},
    {"loadDecompressDictionary", loadDecompressDictionary},
    {"loadCompressDictionary", loadCompressDictionary},
    {"createDecompressStream", createDecompressStream},
    {"createCompressStream", createCompressStream},
    {NULL, NULL}};

LUALIB_API int luaopen_zstd(lua_State* L)
{


  LUA_REGISTER(L, "zstd", zstd_functions);
  return 1;
}

