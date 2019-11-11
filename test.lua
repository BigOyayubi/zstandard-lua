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

local read_testfile2 = function( decompressed_file_prefix )
    local decompressed = readfile(decompressed_file_prefix .. ".txt" )
    local decompressed_size = string.len(decompressed)

    local compressed = readfile(decompressed_file_prefix .. ".zstd" )
    local compressed_size = string.len(compressed)

    local dictionary = readfile(decompressed_file_prefix .. ".dict")
    local dictionary_size = string.len(dictionary)

    return decompressed, decompressed_size, compressed, compressed_size, dictionary, dictionary_size
end

describe("decompress method test", function()
  it("return value check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1/test1.txt' )
    local dst = (" "):rep(decompressed_size)
    local result = zstd.decompress( dst, decompressed_size, compressed, compressed_size )
    
    assert.are.equal( result, decompressed_size )
  end)

  it("decompressed data check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1/test1.txt' )
    local dst = (" "):rep(decompressed_size)
    local result = zstd.decompress( dst, decompressed_size, compressed, compressed_size )

    assert.are.equal( dst, decompressed )
  end)

  it("return value isError check", function()
    local decompressed, decompressed_size, compressed, compressed_size = read_testfile( 'testdata/test1/test1.txt' )
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

describe("decompress method with dictionary test", function()
  it("load dictionary test", function()
    local dst, dst_size, src, src_size, dict_data, dict_data_size = read_testfile2( 'testdata/test2/test2' )
    local dict = zstd.loadDictionary( dict_data, dict_data_size )
  end)

  it("decompress with dictionary test", function()
    local dst, dst_size, src, src_size, dict_data, dict_data_size = read_testfile2( 'testdata/test2/test2' )
    local dict = zstd.loadDictionary( dict_data, dict_data_size )
    local result = dict:decompressDict( dst, dst_size, src, src_size )
    local isError = zstd.isError( result )
    assert.are.equal( isError, false )
  end)
end)
