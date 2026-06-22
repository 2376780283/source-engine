export ICON=nh2.png
export PACKAGE=nh
export APP_NAME="Nightmare House 2"
cd srceng-mod-launcher
git clone https://gitlab.com/LostGamer/android-sdk
export ANDROID_SDK_HOME=$PWD/android-sdk
git pull
chmod +x android/script.sh
./android/scripts/script.sh
chmod +x waf
./waf configure -T release &&
./waf build
