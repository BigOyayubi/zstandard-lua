require 'busted.runner'()

local zstd = require("zstd")
local readfile = function( path )
  local f = io.open(path)
  local s = f:read("*a")
  f:close()
  return s
end

local read_testfile = function( decompressed_filename )
    local decompressed = readfile(decompressed_filename)
    local decompressed_size = string.len(decompressed)
    
    local compressed = readfile(decompressed_filename .. '.zstd')
    local compressed_size = string.len(compressed)

    return decompressed, decompressed_size, compressed, compressed_size
end

describe("decompress method test", function()
  it("return value check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1.txt' )
    local dst = (" "):rep(decompressed_size)
    local result = zstd.decompress( dst, decompressed_size, compressed, compressed_size )
    
    assert.are.equal( result, decompressed_size )
  end)

  it("decompressed data check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1.txt' )
    local dst = (" "):rep(decompressed_size)
    local result = zstd.decompress( dst, decompressed_size, compressed, compressed_size )

    assert.are.equal( dst, decompressed )
  end)

  it("return value isError check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1.txt' )
    local dst = (" "):rep(decompressed_size)
    local result = zstd.decompress( dst, decompressed_size, compressed, compressed_size )
    local isError = zstd.isError( result )
    
    assert.are.equal( isError, false )
  end)

  it("decompress fail test", function()
    local src = "aaa"
    local dst = (" "):rep(100)
    local result = zstd.decompress( dst, #dst, src, #src )
    local isError = zstd.isError( result )
    
    assert.are.equal( isError, true )
  end)
end)


