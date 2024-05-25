#!/bin/bash


##### Obtaining sources #####

# 1. Download sources [2024]:
#     install> VERSION=1.20.7    # last version supporting API level 19 (see https://gstreamer.freedesktop.org/download/#android)
#     install> wget https://gstreamer.freedesktop.org/data/pkg/android/$VERSION/gstreamer-1.0-android-universal-$VERSION.tar.xz.asc \
#                   https://gstreamer.freedesktop.org/data/pkg/android/$VERSION/gstreamer-1.0-android-universal-$VERSION.tar.xz.sha256sum \
#                   https://gstreamer.freedesktop.org/data/pkg/android/$VERSION/gstreamer-1.0-android-universal-$VERSION.tar.xz
#     install> sha256sum -c gstreamer-1.0-android-universal-$VERSION.tar.xz.sha256sum
#
#     src> mkdir android-universal-$VERSION
#     src> rm android-universal; ln -s android-universal-$VERSION android-universal
#     src/android-universal-$VERSION> tar xJf ../../install/gstreamer-1.0-android-universal-$VERSION.tar.xz armv7
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



##### Configuration #####

# Set Android NDK to use ...
ANDROID_NDK=/opt/android/sdk/ndk/21.4.7075529   # last known version working with GStreamer 1.20.7
#~ ANDROID_NDK=/opt/android-ndk-r12b               # last known version working with GStreamer 1.12.4





######################################################################
##### Preamble                                                   #####
######################################################################


set -e  # stop this script on error

MAINDIR=`realpath ${0%/*}`

export GSTREAMER_ROOT_ANDROID=$MAINDIR/src/android-universal





################################################################################
#####   Build Android SDK                                                  #####
################################################################################


echo "#####################################################################"
echo "##### Pre-building for Android SDK ...                          #####"
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

$ANDROID_NDK/ndk-build

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
$ANDROID_NDK/ndk-build clean
rm -fr gst-android-build src   # remove generated artefacts in tutorial/dummy project
