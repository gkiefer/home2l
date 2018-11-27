#!/bin/bash


########## Obtaining sources ##########

# Origin:
#   wget http://www.pjsip.org/release/2.6/pjproject-2.6.tar.bz2
#     -> src/pjproject-2.6


set -e  # stop this script on error
shopt -s nullglob


MAKEFLAGS="-j 8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`





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
  TARGET_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""
  if [[ "$ARCH" != "$LOCAL_ARCH" ]]; then
    case "$ARCH" in
      i386)   # untested!
        TARGET_FLAGS="--host x86_64-pc-linux-gnu"   # just to enable cross-compile mode
        CFLAGS="$CFLAGS -m32"
        LDFLAGS="$LDLAGS -m32"
        ;;
      amd64)
        TARGET_FLAGS="--host i686-pc-linux-gnu"   # just to enable cross-compile mode
        CFLAGS="$CFLAGS -m64"
        LDFLAGS="$LDLAGS -m64"
        ;;
      armhf)
        TARGET_FLAGS="--host arm-linux-gnueabihf"
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
  rm -fr build usr.new/$ARCH
  mkdir build

  echo
  echo "#######################"
  echo "#     Configure...    #"
  echo "#######################"
  echo

  cd $MAINDIR/src/pjproject-2.6
  export CFLAGS
  export LDFLAGS
  ./configure $TARGET_FLAGS --prefix=$MAINDIR/build \
      --disable-ssl --disable-silk --disable-opus --disable-libwebrtc --disable-oss --disable-sdl --disable-openh264
  echo "#define PJMEDIA_HAS_VIDEO 1" > pjlib/include/pj/config_site.h

  echo
  echo "#######################"
  echo "#     Build...        #"
  echo "#######################"
  echo

  make $MAKEFLAGS dep
  make $MAKEFLAGS

  echo
  echo "############################"
  echo "#   Install & Cleanup...   #"
  echo "############################"
  echo

  make $MAKEFLAGS install
  make $MAKEFLAGS clean
  find . -name "lib*.a" | xargs rm
  rm `grep "creating" config.log | sed 's#.*creating ##'`   # remove configure output

  echo
  echo "###################################"
  echo "#     Move files in place ...     #"
  echo "###################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/$ARCH
  mv build/include build/lib usr.new/$ARCH
  rm -fr build
  cd $MAINDIR/usr.new/$ARCH/lib
  for MARK in "-i686-pc-linux-gnu" "-arm-unknown-linux-gnueabihf"; do
    for f in *$MARK.a; do mv $f ${f%%$MARK.a}.a; done
  done
  rm -fr pkgconfig
}





########################################
#####   Part 2: Build for Android  #####
########################################


build_android () {
  echo "NOT IMPLEMENTED YET: Building for Android"
  return

  ##### Build Android SDK #####

  # TBD:
  #  - to be tested!
  #  - add ffmpeg (see https://trac.pjsip.org/repos/wiki/Getting-Started/Android)

  ARCH=android

  echo "#####################################################################"
  echo "##### Pre-building for Android ...                              #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  rm -fr build usr.new/$ARCH
  mkdir build

  echo
  echo "#######################"
  echo "#     Configure...    #"
  echo "#######################"
  echo

  cd $MAINDIR/src/pjproject-2.6
  export ANDROID_NDK_ROOT=/opt/android-ndk-r10d/
  TARGET_ABI=armeabi-v7a ./configure-android --use-ndk-cflags \
    --prefix=$MAINDIR/build --disable-ssl --disable-silk --disable-opus --disable-libwebrtc --disable-oss
      #  --disable-openh264
  echo "#define PJ_CONFIG_ANDROID 1"          > pjlib/include/pj/config_site.h
  echo "#include <pj/config_site_sample.h>"   >> pjlib/include/pj/config_site.h
  echo "#define PJMEDIA_HAS_VIDEO 1"          >> pjlib/include/pj/config_site.h

  echo
  echo "#######################"
  echo "#     Build...        #"
  echo "#######################"
  echo

  make $MAKEFLAGS dep
  make $MAKEFLAGS

  echo
  echo "############################"
  echo "#   Install & Cleanup...   #"
  echo "############################"
  echo

  make $MAKEFLAGS install
  make $MAKEFLAGS clean
  find . -name "lib*.a" | xargs rm
  rm `grep "creating" config.log | sed 's#.*creating ##'`   # remove configure output

  echo
  echo "###################################"
  echo "#     Move files in place ...     #"
  echo "###################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/$ARCH
  mv build/include build/lib usr.new/$ARCH
  mv build/lib usr.new/$ARCH/lib
  rm -fr build
}





########################################
#####   Part 3: Build everything   #####
########################################


##### Clean everything #####

rm -fr include usr.new


##### Build on all supported Debian architectures  #####

# TBD: Try cross-compilation
#~ ssh lilienthal  `realpath $0` -l    # i386
#~ ssh obelix      `realpath $0` -l    # amd64
#~ ssh lennon      `realpath $0` -l    # armhf

build_linux i386
build_linux armhf
build_linux amd64
