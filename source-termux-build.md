# ===== Android NDK 环境配置 =====
export NDK=/home/zzh/android-ndk-r28c
export PATH=$NDK/toolchains/llvm/prebuilt/linux-arm64/bin:$PATH

# 指定编译器与工具链
export CC=aarch64-linux-android32-clang
export CXX=aarch64-linux-android32-clang++
export STRIP=$NDK/toolchains/llvm/prebuilt/linux-arm64/bin/llvm-strip

# 编译与链接参数
export CXXFLAGS="-DNO_STD_REGEX=1 -Wno-error"
export LDFLAGS="-lunwind -static-libstdc++"

# ===== Waf 配置阶段 =====
python3 ./waf configure -T release \
  --prefix=../android_build \
  --android=aarch64,host,32 \
  --target=../android_build/aarch64 \
  --disable-warns \
  --build-games=entropyzero2 \
  --togles \
  --enable-opus

# ===== 构建阶段 =====
# python3 ./waf build -v


python3 ./waf configure -T release --prefix=../android_build --android=aarch64,host,32 --target=../android_build/aarch64 --disable-warns --build-games=cstrike --togles --enable-opus