#!/bin/bash


##### Obtaining sources #####

# Origin:
#   SDL2-2.0.3.tar.gz       from https://www.libsdl.org/release/SDL2-2.0.3.tar.gz
#     -> src/SDL2
#   SDL2_ttf-2.0.12.tar.gz  from https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.12.tar.gz
#     -> src/SDL2_ttf
#
# [2021-04-20] SDL2-2.0.11  requires Android NDK r21e or higher
#
# [2021-04-21] The latest versions compliant with Android-19 are:
#    SDL2-2.0.8
#    SDL2_ttf-2.0.14


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
        TARGET_FLAGS="--host i686-pc-linux-gnu"     # just to enable cross-compile mode
        CFLAGS="$CFLAGS -m32"
        LDFLAGS="$LDLAGS -m32"
        ;;
      amd64)
        TARGET_FLAGS="--host x86_64-pc-linux-gnu"   # just to enable cross-compile mode
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
  echo "#     SDL2 ...     #"
  echo "####################"
  echo

  cd $MAINDIR/src/SDL2
  make maintainer-clean 2>/dev/null || true
  ./autogen.sh
  ./configure $TARGET_FLAGS --prefix=$BUILDDIR --disable-shared --enable-static
  make $MAKEFLAGS install
  make clean
  rm `grep "creating" config.log | sed 's#.*creating ##'`   # remove configure output

  echo
  echo "#################################"
  echo "#     SDL2-TTF/freetype ...     #"
  echo "#################################"
  echo

  cd $MAINDIR/src/SDL2_ttf/external/freetype-*  
  make distclean_project # 2>/dev/null || true
  ./autogen.sh
  ./configure $TARGET_FLAGS --prefix=$BUILDDIR --disable-shared --enable-static
  make && make $MAKEFLAGS install     # The first 'make' is required to generate 'builds/unix/freetype-config'.
  make clean

  echo
  echo "########################"
  echo "#     SDL2-TTF ...     #"
  echo "########################"
  echo
  
  cd $MAINDIR/src/SDL2_ttf
  make maintainer-clean 2>/dev/null || true
  ./autogen.sh
  ./configure $TARGET_FLAGS --prefix=$BUILDDIR --disable-shared --enable-static \
      --with-freetype-prefix=$BUILDDIR --with-sdl-prefix=$BUILDDIR
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
  echo "##### Pre-building for Android SDK ...                          #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR/dummy_app/jni

  echo "#############################################################"
  echo "# Cleaning with Android SDK ...                             #"
  echo "#############################################################"
  echo

  ndk-build clean

  echo
  echo "#############################################################"
  echo "# Building with Android SDK ...                             #"
  echo "#############################################################"
  echo

  # Set Android default options and disable some that are not required or may cause problems ...
  cp $MAINDIR/src/SDL2/include/SDL_config_android.h $MAINDIR/src/SDL2/include/SDL_config.h
  #~ for CFG in SDL_AUDIO_DRIVER_OPENSLES \
             #~ SDL_JOYSTICK_ANDROID SDL_JOYSTICK_HIDAPI SDL_HAPTIC_ANDROID \
             #~ SDL_SENSOR_ANDROID SDL_LOADSO_DLOPEN SDL_FILESYSTEM_ANDROID; do
    #~ sed -i 's#\('$CFG'\).*$#\1 0#' $MAINDIR/src/SDL2/include/SDL_config.h
  #~ done
  ndk-build $MAKEFLAGS SDL2_static SDL2_ttf_static

  echo
  echo "#############################################################"
  echo "# Extracting results to 'android/lib' and 'include'...      #"
  echo "#############################################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/android/include
  cp -va src/SDL2/include/*.h usr.new/android/include
  cp -va src/SDL2_ttf/SDL_ttf.h usr.new/android/include
  mkdir -p usr.new/android/lib
  cp -va dummy_app/obj/local/armeabi-v7a/*.a usr.new/android/lib

  echo
  echo "#############################################################"
  echo "# Final Android cleanup ...                                 #"
  echo "#############################################################"
  echo

  cd $MAINDIR/dummy_app/jni
  ndk-build clean
}





########################################
#####   Part 3: Build everything   #####
########################################


##### Clean everything #####

rm -fr include usr.new


##### Build on all supported Debian architectures  #####

build_linux amd64
build_linux armhf
build_linux i386


##### Build for Android SDK #####

build_android


##### Extract Debian packages that cannot be installed by foreign architecture #####

#~ echo
#~ echo
#~ echo "#####################################################################"
#~ echo "##### Extracting existing Debian packages ...                   #####"
#~ echo "#####################################################################"
#~ echo

#~ cd $MAINDIR
#~ mkdir build
#~ shopt -s nullglob   # for empty directories in 'mv ...' commands
#~ for ARCH in amd64 armhf; do
  #~ for P in debs/*$ARCH.deb; do
    #~ echo $P
    #~ dpkg -x $P build
  #~ done
  #~ if [[ "$ARCH" == "amd64" ]]; then
    #~ GNU_ARCH=x86_64-linux-gnu
  #~ fi
  #~ if [[ "$ARCH" == "armhf" ]]; then
    #~ GNU_ARCH=arm-linux-gnueabihf
  #~ fi
  #~ # TBD: install & compile with correct includes
  #~ # mkdir -p usr.new/$ARCH/include
  #~ # mv build/usr/include/* usr.new/$ARCH/include
  #~ mkdir -p usr.new/$ARCH/lib
  #~ mv build/usr/lib/*.a build/usr/lib/$GNU_ARCH/*.a usr.new/$ARCH/lib
  #~ rm -fr build
#~ done
#~ shopt -u nullglob
