set -e

PWD=$(dirname -- "$0")
cd $PWD

# 创建新的构建目录
rm -rf $PWD/build
mkdir -p $PWD/build

rm -rf $THEOS/lib/RootBridge.framework

# 构建主项目(无根权限版本)
make clean &&
THEOS_PACKAGE_SCHEME=rootless ARCHS="arm64 arm64e" TARGET=iphone:clang:16.4:14.0 make package FINALPACKAGE=1 &&
cp -p "`ls -dtr1 packages/* | tail -1`" $PWD/build/

# 构建主项目(根版本)
make clean &&
make package FINALPACKAGE=1 &&
cp -p "`ls -dtr1 packages/* | tail -1`" $PWD/build/
