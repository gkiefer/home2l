#!/bin/bash


##### Obtaining sources #####

# 1. Download sources:
#     install> wget https://gstreamer.freedesktop.org/data/pkg/android/1.12.4/gstreamer-1.0-android-universal-1.12.4.tar.bz2.asc \
#                   https://gstreamer.freedesktop.org/data/pkg/android/1.12.4/gstreamer-1.0-android-universal-1.12.4.tar.bz2.sha256sum \
#                   https://gstreamer.freedesktop.org/data/pkg/android/1.12.4/gstreamer-1.0-android-universal-1.12.4.tar.bz2
#
#     src> tar xjf ../install/gstreamer-1.0-android-universal-1.12.4.tar.bz2 android-universal/armv7
#
# 2. Documentation:
#     https://gstreamer.freedesktop.org/documentation/installing/for-android-development.html
#
# 3. Configuration (included plugins etc.):
#     see dummy_app/jni/Android.mk
#
# 4. Download docs/tutorial:
#     git clone git://anongit.freedesktop.org/gstreamer/gst-docs
#
# The files in 'dummy_app' are derived from tutorials at https://gstreamer.freedesktop.org/.





set -e  # stop this script on error

MAKEFLAGS="-j 8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`





################################################################################
#####   Build Android SDK                                                  #####
################################################################################

export GSTREAMER_ROOT_ANDROID=$MAINDIR/src/android-universal

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

ndk-build -j 1
  # sometimes 'ndk-build' stops with a strange error; seems to happen less likely without "-j 8" option
  # NDK r12d appears to contain a buggy '/opt/android-ndk/prebuilt/linux-x86/bin/make'!!
  # Workaround: Replace that with '/usr/bin/make' (Debian 9.1, 2018-01-15)

echo
echo "#############################################################"
echo "# Extracting results to 'android/lib' and 'include'...      #"
echo "#############################################################"
echo

cd $MAINDIR
rm -fr usr.new/android
mkdir -p usr.new/android

echo "Copying many headers (please be patient)..."
mkdir -p usr.new/android/include
cp -a src/android-universal/armv7/include/gstreamer-1.0 usr.new/android/include
cp -a src/android-universal/armv7/include/glib-2.0 usr.new/android/include
cp -a src/android-universal/armv7/lib/glib-2.0/include/glibconfig.h usr.new/android/include/glib-2.0/

mkdir -p usr.new/android/lib
cp -va dummy_app/libs/armeabi-v7a/libgstreamer_android.so usr.new/android/lib

echo
echo "#############################################################"
echo "# Final Android cleanup ...                                 #"
echo "#############################################################"
echo

cd $MAINDIR/dummy_app/jni
ndk-build clean
