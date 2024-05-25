#!/bin/bash


########## Obtaining sources ##########

# $ mkdir -p src
# $ cd src
# $ git clone -b 2.11 https://github.com/pjsip/pjproject.git pjsip
# $ git clone -b v1.10.0 https://chromium.googlesource.com/webm/libvpx


########## Information ##########

# Notes
# -----
#
# - Compilation is only supported on amd64 host,

# Configuration
# -------------
#
# Video codecs (Debian 10):
# - libvpx v.1.7 (packaged), requires 'libvpx-dev'; forced for all architectures (amd64, armhf, i386)
#
# Video codecs (Android):
# - libvpx v1.10.0
#
# OpenSSL:
# - relies on PJSIPs auto-detection: presently only on 'amd64' host (no cross-compilation),
#   requires 'libssl-dev'
#

# Increasing number of ALSA devices [2021-06-06]
# ----------------------------------------------
#
# The maximum number of ALSA devices is hard-coded to 32 and too low for a typical Debian system.
# The following patch increases it to PJMEDIA_AUD_MAX_DEVS, which is presently 64.
#
#
# .../home2l/src/external/pjsip/src/pjsip> git format-patch HEAD~1 --stdout
# From b3341c589a74ac093a74f1c5cb1329c90b5f163e Mon Sep 17 00:00:00 2001
# From: Gundolf Kiefer <gundolf.kiefer@web.de>
# Date: Sun, 6 Jun 2021 00:04:05 +0200
# Subject: [PATCH] Increase maximum number of ALSA devices
#
# ---
#  pjmedia/src/pjmedia-audiodev/alsa_dev.c | 2 +-
#  1 file changed, 1 insertion(+), 1 deletion(-)
#
# diff --git a/pjmedia/src/pjmedia-audiodev/alsa_dev.c b/pjmedia/src/pjmedia-audiodev/alsa_dev.c
# index eb729f3ac..1c19f0199 100644
# --- a/pjmedia/src/pjmedia-audiodev/alsa_dev.c
# +++ b/pjmedia/src/pjmedia-audiodev/alsa_dev.c
# @@ -43,7 +43,7 @@
#  #define ALSASOUND_CAPTURE              2
#  #define MAX_SOUND_CARDS                5
#  #define MAX_SOUND_DEVICES_PER_CARD     5
# -#define MAX_DEVICES                    32
# +#define MAX_DEVICES                    PJMEDIA_AUD_MAX_DEVS
#  #define MAX_MIX_NAME_LEN                64
#
#  /* Set to 1 to enable tracing */
# --
# 2.20.1




# Asterisk and Video/H263+ [2021-05-12]
# -------------------------------------
#
# The SDP/FMTP messages sent out by Asterisk depend on the codecs enabled in Asterisk,
# but do not reflect the capabilities of the peer! Hence, only codecs supported by all
# video-capable peers should be enabled.
#
# Special problem with H263+:
# - Asterisk sends:
#     'a=fmtp:96 SQCIF=0;QCIF=1;CIF=1;CIF4=0;CIF16=0;VGA=0;F=0;I=0;J=0;T=0;K=0;N=0;BPP=0;HRD=0'
# - This is not accepted by PJSIP:
#     'pjsua_media.c  ......Error updating media call00:1: Invalid SDP fmtp attribute (PJMEDIA_SDP_EINFMTP)'
# - According to the specification of H263 options [1], the value behind "SQCIF", "QCIF", ... is
#   the maximum allowed frame period time in 1/30 s. Possible values are 1...32.
#   "0" (meant to mean "disabled"?) is not allowed!
#
# The issue is discussed in [2], but is not yet fixed in Asterisk.
# A workaround for PJSIP has been posted in [3].
#
# References:
#   [1] https://datatracker.ietf.org/doc/html/draft-even-avt-h263-h261-options-00#page-4
#   [2] https://community.asterisk.org/t/h263p-video-deactivated-when-using-with-asterisk/67852
#   [3] https://www.spinics.net/lists/pjsip/msg20173.html
#
#
# Patch for PJSIP 2.11:
#
# .../home2l/src/external/pjsip/src/pjsip> git format-patch HEAD~1 --stdout
# From 3ea3df00e8f199ca0052363ee60f59413fe6c530 Mon Sep 17 00:00:00 2001
# From: Gundolf Kiefer <...>
# Date: Wed, 12 May 2021 20:44:23 +0200
# Subject: [PATCH] Workaround for Asterisk sending incorrect H263 fmtp string in
#  SDP
#
# ---
#  pjmedia/src/pjmedia/vid_codec_util.c | 1 +
#  1 file changed, 1 insertion(+)
#
# diff --git a/pjmedia/src/pjmedia/vid_codec_util.c b/pjmedia/src/pjmedia/vid_codec_util.c
# index 4f513679e..03e5ffa7d 100644
# --- a/pjmedia/src/pjmedia/vid_codec_util.c
# +++ b/pjmedia/src/pjmedia/vid_codec_util.c
# @@ -128,6 +128,7 @@ PJ_DEF(pj_status_t) pjmedia_vid_codec_parse_h263_fmtp(
#                 unsigned mpi;
#
#                 mpi = pj_strtoul(&fmtp->param[i].val);
# +               if (mpi == 0) continue;   /* [GK 2021-05-12] Workaround: Asterisk may produce "<format>=0" entry in the SDP/fmtp attribute - we ignore them here. */
#                 if (mpi<1 || mpi>32)
#                     return PJMEDIA_SDP_EINFMTP;
#
# --
# 2.20.1



# General settings ...
MAKEFLAGS="-j8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`

# Android settings ...
export ANDROID_NDK_ROOT=/opt/android-ndk/
export TARGET_ABI=armeabi-v7a
export APP_PLATFORM=android-19



# Initialization ...
set -e  # stop this script on error
shopt -s nullglob





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
  TARGET_CFLAGS="-g"
  TARGET_LDFLAGS=""
  if [[ "$ARCH" != "$LOCAL_ARCH" ]]; then
    case "$ARCH" in
      i386)   # untested!
        TARGET_FLAGS="--host i686-pc-linux-gnu"     # just to enable cross-compile mode
        TARGET_CFLAGS="$CFLAGS -m32"
        TARGET_LDFLAGS="$LDLAGS -m32"
        ;;
      amd64)
        TARGET_FLAGS="--host x86_64-pc-linux-gnu"   # just to enable cross-compile mode
        TARGET_CFLAGS="$CFLAGS -m64"
        TARGET_LDFLAGS="$LDLAGS -m64"
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

  cd $MAINDIR/src/pjsip

  # Manually set CFLAGS for external codec(s) ...
  #   To date, the PJSIP configure script does not provide options to force enabling certain codecs.
  #   Instead, it tries to autodetect them. This often fails unnecessarily, since we only build a
  #   static library (no need for linking) or due to the cross-compilation setup.
  export CFLAGS="$TARGET_CFLAGS -DPJMEDIA_HAS_VPX_CODEC=1"
  export LDFLAGS="$TARGET_LDFLAGS -lvpx"

  # Run configure script ...
  find . -name '.*.depend' -exec rm \{\} \;    # Workaround [2021-05-18]
  ./configure $TARGET_FLAGS --prefix=$MAINDIR/build --disable-sdl --disable-pjsua2 \
      --disable-silk --disable-libwebrtc --disable-openh264 --disable-ffmpeg
    # Note [2021-05-18]: Adding '--enable-ssl', '--enable-opus', '--enable-vpx'
    #    appear to have no effect or even to DISABLE these codecs!

  # Create config file ...
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
  for MARK in "-i686-pc-linux-gnu" "-arm-unknown-linux-gnueabihf" "-x86_64-unknown-linux-gnu"; do
    for f in *$MARK.a; do mv $f ${f%%$MARK.a}.a; done
  done
  rm -fr pkgconfig
}





########################################
#####   Part 2: Build for Android  #####
########################################


build_android () {

  ##### Build with Android SDK #####

  ARCH=android

  echo "#####################################################################"
  echo "##### Pre-building for Android ...                              #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  rm -fr build usr.new/$ARCH
  mkdir -p build/lib

  #~ echo
  #~ echo "################################"
  #~ echo "#     Building OpenH264 ...    #"
  #~ echo "################################"
  #~ echo
  #~ make -C src/openh264 OS=android NDKROOT=/opt/android-ndk-r12b/ TARGET=android-19
  #~   # Note: NDK 21e (and probably later) do not work; other versions after r12b have not been tested.

  echo
  echo "############################"
  echo "#     Building VPX ...     #"
  echo "############################"
  echo

  # The following code creates an Android NDK project and compiles it as described
  # in the VPX source tree, file 'build/make/Android.mk'.
  mkdir -p $MAINDIR/build/android/jni
  cd $MAINDIR/build/android/jni
  ln -s $MAINDIR/src/libvpx libvpx
  ./libvpx/configure --target=armv7-android-gcc --disable-examples --enable-external-build
  echo 'LOCAL_PATH := $(call my-dir)'         > Android.mk
  echo 'include $(CLEAR_VARS)'                >> Android.mk
  echo 'include libvpx/build/make/Android.mk' >> Android.mk
  cp -a $MAINDIR/../../wallclock/android/jni/Application.mk .
  ndk-build $MAKEFLAGS
  mv ../obj/local/armeabi-v7a/libvpx.a $MAINDIR/build/lib
  VPX_INCLUDE=$MAINDIR/src/libvpx

  echo
  echo "########################"
  echo "#     Configure...     #"
  echo "########################"
  echo

  cd $MAINDIR/src/pjsip

  # Note: The environment variables ANDROID_NDK_ROOT, TARGET_ABI, and APP_PLATFORM are set in the
  #       header of this file.

  # Manually set CFLAGS for external codec(s) ...
  #   To date, the PJSIP configure script does not provide options to force enabling certain codecs.
  #   Instead, it tries to autodetect them by compiling and *linking* some test program.
  #   This often fails unnecessarily, since we only build a static library (no need for linking)
  #   or due to the cross-compilation setup.
  export CFLAGS="-I$VPX_INCLUDE -DPJMEDIA_HAS_VPX_CODEC=1"

  # Run configure script ...
  find . -name '.*.depend' -exec rm \{\} \;    # Workaround [2021-05-18]
  ./configure-android --use-ndk-cflags --prefix=$MAINDIR/build --disable-pjsua2 \
    --disable-silk --disable-libwebrtc --disable-openh264 \
    --disable-ssl
    # Note [2021-05-18]: Adding '--enable-opus --enable-vpx' would causes these codecs to be DISABLED!

  # Create config file ...
  echo "#define PJ_CONFIG_ANDROID 1"          > pjlib/include/pj/config_site.h
  echo "#include <pj/config_site_sample.h>"   >> pjlib/include/pj/config_site.h
  echo "#define PJMEDIA_HAS_VIDEO 1"          >> pjlib/include/pj/config_site.h
  #   disable 'JNI_OnLoad()' function, see comment in 'phone-pjsip.c' ...
  echo "#define PJ_JNI_HAS_JNI_ONLOAD 0"      >> pjlib/include/pj/config_site.h
  #   set audio device latency to realistic values Android ...
  #     [2021-06-07] 1. It is not clear if this has any effect - perhaps, these settings must go into CFLAGS.
  #                  2. To date, Speex AEC is the most appropriate AEC, and it does not use any latency argument.
  echo "#define PJMEDIA_SND_DEFAULT_REC_LATENCY  400" >> pjlib/include/pj/config_site.h
  echo "#define PJMEDIA_SND_DEFAULT_PLAY_LATENCY 400" >> pjlib/include/pj/config_site.h
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

  # Move includes ...
  mv build/include usr.new/$ARCH

  # Merge all generated .a files into a single one at the destination ...
  mkdir -p usr.new/$ARCH/lib
  `ndk-which ar` -rcT build/libpjproject-thin.a build/lib/*.a
  cd usr.new/$ARCH/lib
  echo -e "create libpjproject.a\naddlib ../../../build/libpjproject-thin.a\nsave\nend" | `ndk-which ar` -M
  `ndk-which ranlib` libpjproject.a

  # Cleanup ...
  rm -fr $MAINDIR/build
}





########################################
#####   Part 3: Build everything   #####
########################################


##### Clean everything #####

rm -fr usr.new


##### Build on all supported Debian architectures  #####

build_android
#~ exit
build_linux amd64
build_linux armhf
build_linux i386
