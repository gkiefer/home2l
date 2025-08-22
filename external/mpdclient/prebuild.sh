#!/bin/bash


# Notes
# -----
#
#
#
# Obtaining sources
# -----------------
#
#   $ mkdir -p src
#   $ cd src
#   $ git clone -b v2.22 https://github.com/MusicPlayerDaemon/libmpdclient.git libmpdclient
#
#
#
# Configuration
# -------------

# Android settings ...
ANDROID_NDK=/opt/android/sdk/ndk/25.2.9519653


set -e  # stop this script on error
shopt -s nullglob


MAINDIR=`realpath ${0%/*}`
BUILD=/tmp/home2l-build

NDK=/opt/android-ndk




######################################################################
##### Part 1: Build on the local host for the local architecture #####
######################################################################


build_linux () {
  # Args: [<arch>] (Default: local architecture)

  # Determine local and target architecture...
  LOCAL_ARCH=`dpkg --print-architecture`
  ARCH="$1"
  if [[ "$ARCH" == "" ]]; then
    ARCH="$LOCAL_ARCH"
  fi

  if [[ "$LOCAL_ARCH" != "amd64" ]]; then
    echo "ERROR: This script only works on the 'amd64' architecture (but not '$LOCAL_ARCH')!"
    exit 3
  fi

  # Determine configure flags for (cross-)compiling...
  MESON_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""
  if [[ "$ARCH" != "$LOCAL_ARCH" ]]; then
    case "$ARCH" in
      armhf)
        MESON_FLAGS="--cross-file meson-cross-armhf.txt"
        ;;
      *)
        echo "ERROR: Unable to compile for target '$ARCH' on a '$LOCAL_ARCH' host!"
        exit 3
    esac
  fi

  echo
  echo
  echo "#####################################################################"
  echo "#####                 Pre-building for $ARCH"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  rm -fr $BUILD usr.new/$ARCH
  mkdir $BUILD
  mkdir -p usr.new/$ARCH

  echo
  echo "##########################"
  echo "#     Configuring ...    #"
  echo "##########################"
  echo

  export CFLAGS
  export LDFLAGS
  meson setup --prefix $MAINDIR/usr.new/$ARCH --default-library static --buildtype debugoptimized $MESON_FLAGS src/libmpdclient $BUILD

  echo
  echo "#####################################"
  echo "#     Building & installing ...     #"
  echo "#####################################"
  echo

  ninja -C $BUILD install

  cd usr.new/$ARCH
  if [[ "$ARCH" != "armhf" ]]; then
    mv -v lib/*-linux-gnu/*.a lib/
  fi
  rm -fr lib/*-linux-gnu share pkgconfig
}





########################################
#####   Part 2: Build for Android  #####
########################################


build_android () {
  echo
  echo
  echo "#####################################################################"
  echo "#####                 Pre-building for Android"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  rm -fr $BUILD usr.new/android
  mkdir $BUILD
  mkdir -p usr.new/android

  echo
  echo "#################################"
  echo "#     Building NDK toolchain    #"
  echo "#################################"
  echo

  # Determine configure flags for (cross-)compiling...
  TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/
  CC=${TOOLCHAIN}/armv7a-linux-androideabi19-clang
  AR=${TOOLCHAIN}/llvm-ar
  MESON_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""

  # One meson run to obtain 'version.h' and 'config.h'...
  #   Note: both appear to be arch-independent (as of 2.13.0 / 2018-01-14).
  meson setup --prefix $MAINDIR/usr.new/android --default-library static --buildtype debugoptimized $MESON_FLAGS src/libmpdclient $BUILD

  echo
  echo "######################################"
  echo "#     Building static library ...    #"
  echo "######################################"
  echo

  # Compile...
  cd $MAINDIR/src/libmpdclient/src
  rm -fr $BUILD/obj
  mkdir -p $BUILD/obj
  for SRC in *.c; do
    OBJ=${SRC%%.c}.o
    echo "CC $OBJ"
    ${CC} -c $SRC -o $BUILD/obj/$OBJ $CFLAGS -std=c99 -I../include -I. -I$BUILD -I$BUILD/include
    # The following two files are expected in $BUILD from a previous Linux compilation:
    #   version.h - mpdclient version (=> arch-independent)
    #   config.h  - presently (2018-01-14) only arch-independent data
  done

  # Make library...
  cd $BUILD/obj
  LIB=libmpdclient.a
  echo "AR $LIB"
  ${AR} rcs $LIB *.o

  echo
  echo "##################################"
  echo "#     Installing ...             #"
  echo "##################################"
  echo

  cd $MAINDIR/usr.new/android
  ln -vs ../armhf/include include    # borrow include dir from other architecture
  mkdir -p lib
  mv -v $BUILD/obj/*.a lib/
}





########################################
#####   Part 3: Build everything   #####
########################################


##### Clean everything #####

rm -fr include usr.new


##### Build #####

#~ build_android
#~ exit

build_linux amd64
build_linux armhf

build_android
