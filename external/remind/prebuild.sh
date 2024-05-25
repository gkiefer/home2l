#!/bin/bash


########## Obtaining sources ##########

# Origin:
#   $ git clone https://salsa.debian.org/dskoll/remind.git src
#
# Update:
#   $ git pull

# See also:
# - https://developer.android.com/ndk/guides/other_build_systems
# - https://salsa.debian.org/dskoll/remind



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
rm -fr $BUILD/remind
cp -a $MAINDIR/src/ $BUILD/remind
cd $BUILD/remind

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
./configure --host $TARGET REM_USE_WCHAR=0

# Patch configuration file ...
#
# Note (2023-08-15): 'nl_langinfo()' is only used once:
#   src/calendar.c:789:    char const *encoding = nl_langinfo(CODESET);

cat >> src/custom.h << EOF

/* Avoid use of 'wctomb()' and 'nl_langinfo()', which are not present
   on Android API-level 19 ... */

#undef REM_USE_WCHAR
#define nl_langinfo(X) "utf-8"

EOF


# Build ...
echo
echo "##################################################"
echo "#   Building ...                                 #"
echo "##################################################"
echo

make

mkdir -p $MAINDIR/usr/android/bin
cp -a src/remind $MAINDIR/usr/android/bin
$STRIP $MAINDIR/usr/android/bin/remind
