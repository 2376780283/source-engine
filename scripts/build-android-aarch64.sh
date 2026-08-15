#!/bin/sh
git submodule init && git submodule update

sudo apt-get update
sudo apt-get install -f -y libopenal-dev g++-multilib gcc-multilib libpng-dev libjpeg-dev libfreetype6-dev libfontconfig1-dev libcurl4-gnutls-dev libsdl2-dev zlib1g-dev libbz2-dev libedit-dev

wget https://dl.google.com/android/repository/android-ndk-r28c-linux-x86_64.zip -nc -q    

unzip -q android-ndk-r28c-linux-x86_64.zip

export ANDROID_NDK_HOME=$PWD/android-ndk-r28c/

python3 ./waf configure -T release --prefix=../android_build --android=aarch64,llvm,29 --disable-warns --togles

python3 ./waf install
