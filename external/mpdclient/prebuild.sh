#!/bin/bash


########## Obtaining sources ##########

# Origin:
#   wget https://www.musicpd.org/download/libmpdclient/2/libmpdclient-2.13.tar.xz
#   tar Jxf libmpdclient-2.13.tar.xz


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

  if [[ "$LOCAL_ARCH" != "i386" && "$LOCAL_ARCH" != "amd64" ]]; then
    echo "ERROR: This script only works on a 'i386' or 'amd64' architecture (but not '$LOCAL_ARCH')!"
    exit 3
  fi

  # Determine configure flags for (cross-)compiling...
  MESON_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""
  if [[ "$ARCH" != "$LOCAL_ARCH" ]]; then
    case "$ARCH" in
      i386)   # untested!
        CFLAGS="$CFLAGS -m32"
        LDFLAGS="$LDLAGS -m32"
        ;;
      amd64)
        CFLAGS="$CFLAGS -m64"
        LDFLAGS="$LDLAGS -m64"
        ;;
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
  echo "#######################"
  echo "#     Configure...    #"
  echo "#######################"
  echo

  export CFLAGS
  export LDFLAGS
  meson --prefix $MAINDIR/usr.new/$ARCH --default-library static --buildtype debugoptimized $MESON_FLAGS src/libmpdclient $BUILD

  echo
  echo "##################################"
  echo "#     Build & install ...        #"
  echo "##################################"
  echo

  ninja -C $BUILD install

  cd usr.new/$ARCH
  mv lib/i386-linux-gnu/*.a lib/
  rm -fr lib/i386-linux-gnu share
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
  echo "##############################"
  echo "#     Build NDK toolchain    #"
  echo "##############################"
  echo

  $NDK/build/tools/make-standalone-toolchain.sh \
    --arch=arm --abis=armeabi-v7a --platform=android-19 --install-dir=$BUILD/android-toolchain

  # Determine configure flags for (cross-)compiling...
  MESON_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""
  TOOLS=$BUILD/android-toolchain/arm-linux-androideabi/bin

  # One meson run to obtain 'version.h' and 'config.h'...
  #   Note: both appear to be arch-independent (as of 2.13.0 / 2018-01-14).
  meson --prefix $MAINDIR/usr.new/android --default-library static --buildtype debugoptimized src/libmpdclient $BUILD

  echo
  echo "##############################################"
  echo "#     Compile and make static library ...    #"
  echo "##############################################"
  echo

  # Compile...
  cd $MAINDIR/src/libmpdclient/src
  rm -fr $BUILD/obj
  mkdir -p $BUILD/obj
  for SRC in *.c; do
    OBJ=${SRC%%.c}.o
    echo "CC $OBJ"
    $TOOLS/gcc -c $SRC -o $BUILD/obj/$OBJ $CFLAGS -std=c99 -I../include -I. -I$BUILD
    # The following two files are expected in $BUILD from a previous Linux compilation:
    #   version.h - mpdclient version (=> arch-independent)
    #   config.h  - presently (2018-01-14) only arch-independent data
  done

  # Make library...
  cd $BUILD/obj
  LIB=libmpdclient.a
  echo "AR $LIB"
  $TOOLS/ar rcs $LIB *.o

  echo
  echo "##################################"
  echo "#     Install ...                #"
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

build_linux i386
build_linux armhf
build_linux amd64

build_android
