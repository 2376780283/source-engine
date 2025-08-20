#!/bin/sh

git submodule init && git submodule update
git clone https://github.com/ItzVladik/srceng-mod-launcher
#sudo apt-get update
#sudo apt-get install -f -y openjdk-17-jdk zip apksigner imagemagick lib32z1

set -e

if [ ! -e "android-ndk-r10e-linux-x86_64.zip"  ]; then
	wget https://dl.google.com/android/repository/android-ndk-r10e-linux-x86_64.zip -o /dev/null
	unzip android-ndk-r10e-linux-x86_64.zip
fi

export ANDROID_NDK_HOME=$PWD/android-ndk-r10e/

if [ ! -e "clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz" ]; then
	wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.1.0/clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz -o /dev/null
	tar -xf clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
fi

export PATH="$PWD/clang+llvm-11.1.0-x86_64-linux-gnu-ubuntu-16.04/bin:$PATH"

#python3 waf configure -T release --build-game=entropyzero2 --prefix=srceng-mod-launcher/android --android=armeabi-v7a-hard,host,21 --target=../armeabi-v7a --32bits --disable-warns &&
#python3 waf install --target=client,server,gamepadui,game_shader_dx9 --strip

python3 waf configure -T release --build-game=entropyzero2 --prefix=srceng-mod-launcher/android --android=aarch64,host,21 --target=../aarch64 --disable-warns &&
python3 waf install --target=client,server,gamepadui,game_shader_dx9 --strip

if [ -e "srceng-mod-launcher/android/lib/arm64-v8a/README.md" ]; then
	rm srceng-mod-launcher/android/lib/arm64-v8a/README.md
fi

export ICON=ez2.png
export PACKAGE=entropyzero2
export APP_NAME="Entropy: Zero 2"
cd srceng-mod-launcher

# rm android/lib/armeabi-v7a/libvstdlib.so android/lib/armeabi-v7a/libsteam_api.so android/lib/armeabi-v7a/libtier0.so
rm android/lib/arm64-v8a/libvstdlib.so android/lib/arm64-v8a/libsteam_api.so android/lib/arm64-v8a/libtier0.so


if [ ! -e "android-sdk" ]; then 
	git clone https://github.com/ItzVladik/android-sdk
fi

export ANDROID_SDK_HOME=$PWD/android-sdk
git pull
chmod +x android/scripts/script.sh
./android/scripts/script.sh
chmod +x waf
python3 waf configure -T release --javac-source-target=8 &&
python3 waf build
