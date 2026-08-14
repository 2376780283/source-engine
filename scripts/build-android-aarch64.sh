#!/bin/sh
export ANDROID_NDK_HOME=$PWD/android-ndk-r28c/
python3 ./waf configure -T release --prefix=../android_build --android=aarch64,llvm,29 --target=android_build --disable-warns --togles