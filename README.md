# zstandard-lua

Unity + xLua環境でzstandardを扱うため、<br>
xLuaビルドにzstandard wrapper c apiを組み込んだものです。

# motivation

近年の優秀な可逆圧縮アルゴリズムにzstandardがあります。<br>
ただzlibのようなPureC#実装はまだないようで、一般的な手法でUnityで使うには

* Windows
* MacOS
* iOS
* Android
* Linux

など各環境でのcビルドとネイティブプラグインとC#をつなぐコードが必要でした。

xLuaのlua-rapidjsonの組み込みを調べていたところ、<br>
同様にzstd cコードとwrapper cコードを組み込めば<br>
xLuaの各環境でのcビルドとネイティブプラグインとC#をつなぐコードを活かせると<br>
思い至り書き始めました。

# memo

zstandardのjava実装があるため、Android側はjava、iOSはj2objcを使えば<br>
lua非経由でUnityにzstandardを導入できる気がします。

