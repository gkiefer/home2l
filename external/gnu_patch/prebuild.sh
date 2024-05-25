#!/bin/bash


########## Obtaining sources ##########

# Origin:
#   $ git clone https://github.com/gitGNU/gnu_patch.git src
#   $ cd src; ./bootstrap

# See also:
# - https://developer.android.com/ndk/guides/other_build_systems



set -e  # stop this script on error
shopt -s nullglob


MAINDIR=`realpath ${0%/*}`
BUILD=/tmp/home2l-build

NDK=/opt/android-ndk



########################################
#####   Part 1: Build for Android  #####
########################################

echo
echo "##################################################"
echo "#   Copying sources ...                          #"
echo "##################################################"
echo

# Fresh copy of sources ...
mkdir -p $BUILD
rm -fr $BUILD/gnu_patch
cp -a $MAINDIR/src/ $BUILD/gnu_patch
cd $BUILD/gnu_patch

# Toolchain options ...
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64

export TARGET=armv7a-linux-androideabi

# Set this to your minSdkVersion.
export API=19

export AR=$TOOLCHAIN/bin/llvm-ar
export CC=$TOOLCHAIN/bin/$TARGET$API-clang
export AS=$CC
export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
export LD=$TOOLCHAIN/bin/ld
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib
export STRIP=$TOOLCHAIN/bin/llvm-strip



echo
echo "##################################################"
echo "#   Configuring ...                              #"
echo "##################################################"
echo

# Configure ...
./configure --host $TARGET


# Build ...
echo
echo "##################################################"
echo "#   Building ...                                 #"
echo "##################################################"
echo

make

mkdir -p $MAINDIR/usr/android/bin
cp -a src/patch $MAINDIR/usr/android/bin
$STRIP $MAINDIR/usr/android/bin/patch
