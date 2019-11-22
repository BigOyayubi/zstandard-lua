# zstandard-lua

A zstandard module for Lua, based on the zstandard c library.

it made for use with Unity and xLua.

facebook製可逆圧縮アルゴリズムzstandardのlua bindingです。<br>
Unity + xLua環境向けに組んでいます。英語は苦手です。

# Dependencies

* cmake
    * xLua build needs it
* go
    * I use make2help to show Makefile descriptions
* busted
    * lua unit test framework
        * `luarocks intall busted`
    * `busted test.lua`

# install

## osx

```
$ cd zstandard-lua
$ make install
$ lua
> zstd = require('zstd')
> result = zstd.decompress( decompressBuffer, decompressBufferSize, compressBuffer, compressBufferSize )
> print( zstd.isError( result ) -- false
$ make uninstall #uninstall
```

## without make

```
$ cd zstandard-lua
$ cp CMakeLists.txt xLua/build/CMakeLists.txt
$ cd xLua/build
$ sh make_linux64_luajit.sh
```

## Unity

```
$ cd zstandard-lua
$ make ios-luajit && make osx-luajit
$ cp -ap xLua/build/plugin_luajit/Plugins/* ~/UnityProj/Assets/Plugins/
```

add below C# codes to use in unity

```
namespace XLua.LuaDLL
{
    using System.Runtime.InteropServices;
    public partial class Lua
    {
        [DllImport(LUADLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int luaopen_zstd(System.IntPtr L);
        [MonoPInvokeCallback(typeof(LuaDLL.lua_CSFunction))]
        public static int LoadZstd(System.IntPtr L)
        {
            return luaopen_zstd(L);
        }
    }
}
```

now you can call zstd functions in UnityC# like this

```
namespace Hoge
{
    public class FugaBehavior : MonoBehavior
    {
        void Start()
        {
            using(var env = new xLua.LuaEnv())
            {
                env.AddBuildin("zstd", XLua.LuaDLL.Lua.LoadZstd);
                env.DoString(@"
local zstd = require('zstd')
local src = CS.UnityEngine.Resources.Load("compressed").bytes
local srcdSize = string.len(src)
local dstSize = 12345
local dst = (' '):rep(dstSize)
local result = zstd.decompress( dst, dstSize, src, srcSize )
print( zstd.isError(result) )
local str = ("aaa"):rep(10)
local compressed = zstd.compress( str, string.len(str) )
                ");
            }        
        }
    }
}
```
# APIs

## setup

```
local zstd = require('zstd')
```

## compress

```
local data = ("abc"):rep(100)
local compressed = zstd.compress( data, string.len(data) )
```

## streaming compress

```
local stream = zstd.createCompressStream()
local text = ("aaa"):rep(100)
stream:set( text, #text )
local t = {}
repeat
  local comp, done, read_in, read_out = stream:compress()
  table.insert(t, comp)
until done
local compressed = table.concat(t)
```

## compress with dictionary

```
local f = io.open('path/to/dict')
local dictdata = f:read("*a")
f:close()

local level = 3 -- option
local dict = zstd.loadCompressDictionary( dictdata, string.len(dictdata), level )
local data = ("abc"):rep(100)
local compressed = dict:compress( data, string.len(data) )
```

## decompress

```
local dst = (" "):rep(12345)
local decompressedSize = zstd.decompress( dst, string.len(dst), src, string.len(src) )
local isError = zstd.isError(decompressedSize) -- true or false
```

## streaming decompress

```
local stream = zstd.createDecompressStream()
local text = ("aaa"):rep(100)
local compressed = zstd.compress( text, #text )
stream:set( compressed, string.len(compressed) )
local t = {}
repeat
  local decomp, done, read_in, read_out = stream:decompress()
  table.insert(t, decomp)
until done
local decompressed = table.concat(t)
```

## decompress with dictionary

```
local dict = zstd.loadDecompressDictionary( dictdata, string.len(dictdata) )
local decompressedSize = dict:decompress( dst, string.len(dst), src, string.len(src) )
```

# todo

* (done)decompress
* (done)decompress with dictionary
* (done)stream decompress
* stream decompress with dictionary
* (done)compress
* (done)compress with dictionary
* (done)stream compress
* stream compress with dictionary
* (done)lua test code
* (done)osx build
* (done)ios build
* windows build
* android build

# motivation

zstandard is modern and has good performance.<br>
But pure C# implemantation of zstd, like zlib, does not exist.<br>
We have to make native plugin for each OS and binding C# to use zstd.

近年の優秀な可逆圧縮アルゴリズムにzstandardがあります。<br>
ただzlibのようなPureC#実装はまだないようで、一般的な手法でUnityで使うには

* Windows
* MacOS
* iOS
* Android
* Linux

など各環境でのcビルドとネイティブプラグインとC#をつなぐコードが必要でした。

I recently read xLua documents  "Add third-party Lua Libraries on xLua".<br>
lua-rapidjson, described in xLua's documents, similer to zstandard. <br>
and I started to implement zstandard binding.

xLuaのlua-rapidjsonの組み込みを調べていたところ、<br>
同様にzstd cコードとwrapper cコードを組み込めば<br>
xLuaの各環境でのcビルドとネイティブプラグインとC#をつなぐコードを活かせると思い至り書き始めました。

# memo

zstd java implementation and j2objc may be good to use zstd in unity.

zstandardのjava実装があるため、Android側はjava、iOSはj2objcを使えばlua非経由でUnityにzstandardを導入できる気がします。

