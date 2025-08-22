#!/bin/bash


##### Obtaining sources #####

# Initial:
#   $ md -p src && cd src
#   $ git clone https://github.com/libsdl-org/SDL.git
#   $ git clone https://github.com/libsdl-org/SDL_ttf.git
#
# Upgrade:
#   $ cd src/SDL
#   $ git pull
#   $ git checkout release-2.28.5
#   $ cd ../SDL_ttf
#   $ git checkout release-2.20.2



##### Configuration #####

# Set Android NDK to use ...
ANDROID_NDK=/opt/android/sdk/ndk/25.2.9519653





######################################################################
##### Preamble                                                   #####
######################################################################


set -e  # stop this script on error

MAKEFLAGS="-j 8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`
BUILDDIR=/tmp/home2l-build/sdl2





######################################################################
##### Part 1: Build on the local host for the local architecture #####
######################################################################

# Note: Include files will be taken from the Android part.
#   By visual inspection, the include files generated here and from the
#   Android build system differ only in the 'config.h' file, where the
#   one from Android seems to be more generic.


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
  TARGET_FLAGS=""
  CFLAGS="-g"
  LDFLAGS=""
  if [[ "$ARCH" != "$LOCAL_ARCH" ]]; then
    case "$ARCH" in
      armhf)
        TARGET_FLAGS="--host arm-linux-gnueabihf"
        ;;
      *)
        echo "ERROR: Unable to compile for target '$ARCH' on a '$LOCAL_ARCH' host!"
        exit 3
    esac
  fi
  export CFLAGS
  export LDFLAGS

  echo
  echo
  echo "#####################################################################"
  echo "#####                 Pre-building for $ARCH"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  rm -fr $BUILDDIR usr.new/$ARCH
  mkdir -p $BUILDDIR

  echo
  echo "####################"
  echo "#     SDL ...      #"
  echo "####################"
  echo

  cd $MAINDIR/src/SDL
  make maintainer-clean 2>/dev/null || true
  ./autogen.sh
  ./configure $TARGET_FLAGS --prefix=$BUILDDIR --disable-shared --enable-static
  make $MAKEFLAGS install
  make clean
  rm `grep "creating" config.log | sed 's#.*creating ##'`   # remove configure output

  echo
  echo "########################"
  echo "#     SDL-TTF ...      #"
  echo "########################"
  echo
  
  cd $MAINDIR/src/SDL_ttf
  make maintainer-clean 2>/dev/null || true
  ./autogen.sh
  ./configure $TARGET_FLAGS --prefix=$BUILDDIR --disable-shared --enable-static \
      --with-sdl-prefix=$BUILDDIR
  make $MAKEFLAGS install
  make clean

  echo
  echo "###################################"
  echo "#     Move files in place ...     #"
  echo "###################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/$ARCH
  mv $BUILDDIR/include/SDL2 usr.new/$ARCH/include
  mv $BUILDDIR/lib usr.new/$ARCH/lib
  rm -fr $BUILDDIR
}





##########################################
#####   Part 2: Build for Android    #####
##########################################


build_android () {
  echo "#####################################################################"
  echo "##### Pre-building for Android ...                              #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR/dummy_app/jni

  echo "#############################################################"
  echo "# Cleaning with Android NDK ...                             #"
  echo "#############################################################"
  echo

  $ANDROID_NDK/ndk-build clean

  echo
  echo "#############################################################"
  echo "# Building with Android NDK ...                             #"
  echo "#############################################################"
  echo

  # Set Android default options and disable some that are not required or may cause problems ...
  cp $MAINDIR/src/SDL/include/SDL_config_android.h $MAINDIR/src/SDL/include/SDL_config.h
  #~ for CFG in SDL_AUDIO_DRIVER_OPENSLES \
             #~ SDL_JOYSTICK_ANDROID SDL_JOYSTICK_HIDAPI SDL_HAPTIC_ANDROID \
             #~ SDL_SENSOR_ANDROID SDL_LOADSO_DLOPEN SDL_FILESYSTEM_ANDROID; do
    #~ sed -i 's#\('$CFG'\).*$#\1 0#' $MAINDIR/src/SDL/include/SDL_config.h
  #~ done
  $ANDROID_NDK/ndk-build $MAKEFLAGS SDL2_static SDL2_ttf_static freetype harfbuzz

  echo
  echo "#############################################################"
  echo "# Extracting results to 'android/lib' and 'include'...      #"
  echo "#############################################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/android/include
  cp -va src/SDL/include/*.h usr.new/android/include
  cp -va src/SDL_ttf/SDL_ttf.h usr.new/android/include
  mkdir -p usr.new/android/lib
  cp -va dummy_app/obj/local/armeabi-v7a/*.a usr.new/android/lib

  echo
  echo "#############################################################"
  echo "# Final Android cleanup ...                                 #"
  echo "#############################################################"
  echo

  cd $MAINDIR/dummy_app/jni
  $ANDROID_NDK/ndk-build clean
}





########################################
#####   Part 3: Build everything   #####
########################################


##### Clean everything #####

rm -fr include usr.new


##### Build on all supported Debian architectures  #####

build_linux amd64
build_linux armhf


##### Build for Android SDK #####

build_android
