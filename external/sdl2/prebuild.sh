#!/bin/bash


##### Obtaining sources #####

# Origin:
#   SDL2-2.0.3.tar.gz       from https://www.libsdl.org/release/SDL2-2.0.3.tar.gz
#     -> src/SDL2
#   SDL2_ttf-2.0.12.tar.gz  from https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.12.tar.gz
#     -> src/SDL2_ttf


set -e  # stop this script on error

MAKEFLAGS="-j 8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`





######################################################################
##### Part 1: Build on the local host for the local architecture #####
######################################################################

# Note: Include files will be taken from the Android part.
#   By visual inspection, the include files generated here and from the
#   Android build system differ only in the 'config.h' file, where the
#   one from Android seems to be more generic.


if [[ "$1" == "-l" ]]; then
  ARCH=`dpkg --print-architecture`

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
  echo "####################"
  echo "#     SDL2 ...     #"
  echo "####################"
  echo

  cd $MAINDIR/src/SDL2
  ./configure --prefix=$MAINDIR/build --disable-shared --enable-static
  make clean
  make $MAKEFLAGS install
  make clean
  rm `grep "creating" config.log | sed 's#.*creating ##'`   # remove configure output

  echo
  echo "########################"
  echo "#     SDL2-TTF ...     #"
  echo "########################"
  echo

  cd $MAINDIR/src/SDL2_ttf
  ./configure --prefix=$MAINDIR/build --with-sdl-prefix=$MAINDIR/build --disable-shared --enable-static
  make clean
  make $MAKEFLAGS install
  make clean

  echo
  echo "###################################"
  echo "#     Move files in place ...     #"
  echo "###################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/$ARCH
  mv build/include/SDL2 usr.new/$ARCH/include
  mv build/lib usr.new/$ARCH/lib
  rm -fr build





########################################
#####   Part 2: Build everything   #####
########################################


else # if [[ "$1" == "-l" ]];


  ##### Clean everything #####

  rm -fr include usr.new


  ##### Build on all supported Debian architectures  #####

  ssh lilienthal  `realpath $0` -l    # i386
  ssh obelix      `realpath $0` -l    # amd64
  ssh lennon      `realpath $0` -l    # armhf


  ##### Extract Debian packages that cannot be installed by foreign architecture #####

  echo
  echo
  echo "#####################################################################"
  echo "##### Extracting existing Debian packages ...                   #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  mkdir build
  shopt -s nullglob   # for empty directories in 'mv ...' commands
  for ARCH in amd64 armhf; do
    for P in debs/*$ARCH.deb; do
      echo $P
      dpkg -x $P build
    done
    if [[ "$ARCH" == "amd64" ]]; then
      GNU_ARCH=x86_64-linux-gnu
    fi
    if [[ "$ARCH" == "armhf" ]]; then
      GNU_ARCH=arm-linux-gnueabihf
    fi
    # TBD: install & compile with correct includes
    #~ mkdir -p usr.new/$ARCH/include
    #~ mv build/usr/include/* usr.new/$ARCH/include
    mkdir -p usr.new/$ARCH/lib
    mv build/usr/lib/*.a build/usr/lib/$GNU_ARCH/*.a usr.new/$ARCH/lib
    rm -fr build
  done
  shopt -u nullglob


  ##### Build Android SDK #####

  echo "#####################################################################"
  echo "##### Pre-building for Android SDK ...                          #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR/dummy_app/jni

  echo "#############################################################"
  echo "# Cleaning Android SDK ...                                  #"
  echo "#############################################################"
  echo

  ndk-build clean

  echo
  echo "#############################################################"
  echo "# Building Android SDK ...                                  #"
  echo "#############################################################"
  echo

  cp $MAINDIR/src/SDL2/include/SDL_config_android.h $MAINDIR/src/SDL2/include/SDL_config.h
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


fi # if [[ "$1" == "-l" ]];
