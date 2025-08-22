#!/bin/bash

# NOTE: Linphone is currently unsupported!
#       Building last working version on Android requires NDK r10d !!



##### Download source (upgrade trial without success) #####

#    git clone git://git.linphone.org/linphone-android.git --recursive src
#      from http://www.linphone.org/technical-corner/linphone/downloads
#
#   git checkout 3.2.7; git submodule update --recursive

#~ > git describe --long
#~ 3.2.7-0-gf74d504f
#~ > git submodule

#~ 20f7bdd8118cc518015a500033de45d39702f0ed submodules/bcg729 (1.0.3-9-g20f7bdd)
#~ 0cda8686829e23387c4468f33c71425bcc36b9d7 submodules/bctoolbox (0.5.1-77-g0cda868)
#~ cf1aaa36c5738c25e59c8fafbade388a0081cd53 submodules/bcunit (3.0-13-gcf1aaa3)
#~ 8077adc76cf7a99c8908c487c7fe2f20a181ce7b submodules/belcard (1.0.1-14-g8077adc)
#~ 05a77dbf24f06bd58cffd610dbee2c0abf3fdc73 submodules/belle-sip (1.6.1-53-g05a77db)
#~ 0e237fca52851df28f2401702fca5d34ed56bd5b submodules/belr (0.1.2-10-g0e237fc)
#~ 556335fee2bbb28e6cbe40510499ef15abef85fb submodules/bzrtp (1.0.5-12-g556335f)
#~ 7b77d1eab60e2bb88f765d679f8b4f86a1bfbd93 submodules/cmake-builder (desktop-3.10.0-137-g7b77d1e)
#~ 1db66897fa0f2f4b397bb961b3e5a747c170ed40 submodules/externals/antlr3 (antlr-3.4-34-g1db6689)
#~ 6899f2759c7b19d5402335d3a937c53020abfeca submodules/externals/bv16-floatingpoint (1.2)
#~ 4e154e6bbbe92cc76e333a0b4acb365b5c042ec6 submodules/externals/codec2 (heads/linphone)
#~ 644296e736ee219cd02f7b7d7b7b4c7c5a464217 submodules/externals/ffmpeg (n2.8.3)
#~ 0f8822b5326c76bb9dc4c6b552631f51792c3982 submodules/externals/gsm (heads/master-10-g0f8822b)
#~ 0e99446a237bef6704d45f74d504ae8aafa45f9a submodules/externals/libjpeg-turbo (heads/master)
#~ 743cc896d3995bdf87d64e780831d42dbd12c738 submodules/externals/libmatroska-c (0.22.1-124-g743cc89)
#~ 3fc0f9ad1dc124bdd7fc2aa5aeb6a684f79191f2 submodules/externals/libupnp (release-1.6.19)
#~ 22aa947d577adbdd9cbfa7bb92da599254bcfb8f submodules/externals/libvpx (v1.6.1-2-g22aa947d5)
#~ c943f708f1853de4eb15e5a94cf0b35d108da87a submodules/externals/libxml2 (v2.8.0)
#~ d9385339a5c2979786cfc844c0527593c14662c5 submodules/externals/mbedtls (mbedtls-2.4.1-146-gd9385339)
#~ 3b67218fb8efb776bcd79e7445774e02d778321d submodules/externals/opencore-amr (v0.1.3-10-g3b67218)
#~ 9e75838c8638c48a32b15c73c9da7b1fe942fd5f submodules/externals/openh264 (v1.1-1592-g9e75838c)
#~ 35b371a85bf2cf21ab4b12b5475c76a2775b25d1 submodules/externals/opus (v1.1.4-10-g35b371a8)
#~ 0aafdb5941457e8aa96cc835f9f221f8cd7afb10 submodules/externals/speex (remotes/origin/HEAD)
#~ a752d47da1a80e634f81a57bb975b6a80fc14ffa submodules/externals/srtp (remotes/origin/cvs-20120427-19-ga752d47)
#~ 60b925e621fd8d2bd3000207ceb5596a0fa20fb3 submodules/externals/vo-amrwbenc (v0.1.3-10-g60b925e)
#~ adc99d17d8c1fbc164fae8319b40d7c45f30314e submodules/externals/x264 (adc99d17)
#~ 9782bd388b8c5e0411530ecba473f3498fdad5c6 submodules/linphone (3.11.1-242-g9782bd388)
#~ -43539d210063c7a6be360758090f4890c10973d9 submodules/mediastreamer2
#~ 207ab85b81fc3d4dda75ee460f4a3107da7e14bc submodules/msamr (1.1.2-5-g207ab85)
#~ 789c8a4adf83d719b0dc1ea58c7b77fefe39f5ac submodules/mscodec2 (heads/master)
#~ 3a398b4000f29c67e010057f718c136ca245a9a8 submodules/msopenh264 (1.2.0-5-g3a398b4)
#~ fef0c397c1d2dfc3b8b0951e3db0b9b5cb5d34b9 submodules/mssilk (1.1.0-5-gfef0c39)
#~ 265c3bf1bfae31ed43766afb473782ec7e782898 submodules/mswebrtc (1.1.0-7-g265c3bf)
#~ 3a22b8d31c43a3a4e1e4985b5a6bbec7b03972bf submodules/msx264 (1.5.3-8-g3a22b8d)
#~ -67c0672e2680baa85a74d8966b813fac259649df submodules/oRTP




##### Download source (last working version, under Jessie) #####

#    git clone git://git.linphone.org/linphone-android.git --recursive src
#      from http://www.linphone.org/technical-corner/linphone/downloads
#
#   git checkout 3.2.1-0-gcb10aa4; git submodule update --recursive    # Version number may be out-dated

#~ > git describe --long
#~ 3.2.1-0-gcb10aa4
#~ > git submodule
#~ 20614c1ede14e02adbd6518a278334e9388602f3 submodules/bcg729 (1.0.1-21-g20614c1)
#~ 6c9c968e550b8f1106e97a99b391c5eda6bf6a6b submodules/bctoolbox (0.4.0-6-g6c9c968)
#~ 29c556fa8ac1ab21fba1291231ffa8dea43cf32a submodules/bcunit (3.0-8-g29c556f)
#~ 63e61b0ae0f20e6d9f790335184fa4a0fc2a90ab submodules/belcard (1.0.0-23-g63e61b0)
#~ f082be37ebbdf600e6115563ce53e85e2374b635 submodules/belle-sip (1.5.0-110-gf082be3)
#~ 8a7f0868a7d35f86ff5fa422c7333f113d935e0f submodules/belr (0.1.1-9-g8a7f086)
#~ 2b234b08e6c85babbcd76651ab432b91c76aaff4 submodules/bzrtp (1.0.4-2-g2b234b0)
#~ 8762d5e50463c773cae56bb965661d53ad2395af submodules/cmake-builder (desktop-3.10.0-76-g8762d5e)
#~ 1db66897fa0f2f4b397bb961b3e5a747c170ed40 submodules/externals/antlr3 (antlr-3.4-34-g1db6689)
#~ 6899f2759c7b19d5402335d3a937c53020abfeca submodules/externals/bv16-floatingpoint (1.2)
#~ 4e154e6bbbe92cc76e333a0b4acb365b5c042ec6 submodules/externals/codec2 (heads/linphone)
#~ 644296e736ee219cd02f7b7d7b7b4c7c5a464217 submodules/externals/ffmpeg (n2.8.3)
#~ 0f8822b5326c76bb9dc4c6b552631f51792c3982 submodules/externals/gsm (heads/master-10-g0f8822b)
#~ 825b3fb90eb48e3cd3128bfa64cd66c07dd5ac6f submodules/externals/libmatroska-c (0.22.1-122-g825b3fb)
#~ 3fc0f9ad1dc124bdd7fc2aa5aeb6a684f79191f2 submodules/externals/libupnp (release-1.6.19)
#~ d2b4742a04da011adf05a4ea63d041f60e50195a submodules/externals/libvpx (v1.5.0-4-gd2b4742)
#~ c943f708f1853de4eb15e5a94cf0b35d108da87a submodules/externals/libxml2 (v2.8.0)
#~ 3b88f2749d59e5346de08e121fba1d797c55ddaa submodules/externals/mbedtls (mbedtls-2.2.1-30-g3b88f27)
#~ 3b67218fb8efb776bcd79e7445774e02d778321d submodules/externals/opencore-amr (v0.1.3-10-g3b67218)
#~ 9e75838c8638c48a32b15c73c9da7b1fe942fd5f submodules/externals/openh264 (v1.1-1592-g9e75838)
#~ 1f22ae379bbcdfa376775db2b9407529fb1fa781 submodules/externals/opus (v1.1.1)
#~ b9700a4515a89a493ed34dc0c72f768d4949d543 submodules/externals/speex (b9700a4)
#~ a752d47da1a80e634f81a57bb975b6a80fc14ffa submodules/externals/srtp (remotes/origin/cvs-20120427-19-ga752d47)
#~ 60b925e621fd8d2bd3000207ceb5596a0fa20fb3 submodules/externals/vo-amrwbenc (v0.1.3-10-g60b925e)
#~ adc99d17d8c1fbc164fae8319b40d7c45f30314e submodules/externals/x264 (adc99d1)
#~ a87a9ecd454616d04baa9e04e9e1ffa9ff36c375 submodules/linphone (3.10.2-463-ga87a9ec)
#~ 207ab85b81fc3d4dda75ee460f4a3107da7e14bc submodules/msamr (1.1.2-5-g207ab85)
#~ 789c8a4adf83d719b0dc1ea58c7b77fefe39f5ac submodules/mscodec2 (heads/master)
#~ 57dfcd9a0d9d0019c780d784682eaaa9763d4a33 submodules/msopenh264 (1.1.2-6-g57dfcd9)
#~ 48a8d1f5df85e4a784633584fda49a3202491a9a submodules/mssilk (1.0.2-7-g48a8d1f)
#~ 475eb67b5cb8d82f6636e69c3bde8b18daeb824e submodules/mswebrtc (1.1.0~13)
#~ 0a5c0a89fa05cab2445073c5ba5546f1511b2a78 submodules/msx264 (1.5.3-7-g0a5c0a8)



##### Version information (older versions) #####

#~ > git describe --long
#~ 2.5.1-0-g9dc02ee
#~ > git submodule
#~ 31a89d7d951200f6c86f800ca017184700bd0917 submodules/bcg729 (0.2~37)
#~ 91ae7c164d9d20fd36657943a0aba807b4dfeb4f submodules/belle-sip (1.4.1-82-g91ae7c1)
#~ 4a4f757f66b02cf8834fd8d7a939bf54b245ad7e submodules/bzrtp (1.0.0-11-g4a4f757)
#~ 1db66897fa0f2f4b397bb961b3e5a747c170ed40 submodules/externals/antlr3 (antlr-3.4-34-g1db6689)
#~ c47eaa453fb75d55d32304413672c16706af85e0 submodules/externals/axmlrpc (heads/master)
#~ dc9283c76517b1f093d132d1ee0aa9671b558e56 submodules/externals/cunit (heads/master-11-gdc9283c)
#~ 26617b47faccaf3646b767ed3062affd8252499d submodules/externals/ffmpeg (n1.2.3)
#~ 405fb3856f0b9e902dcb159ec6a3409ba6e78476 submodules/externals/gsm (heads/master)
#~ 3414f292bb5b1ac5ee2fd99976b7274fd81e48ee submodules/externals/libmatroska (remotes/origin/dev-3-g3414f29)
#~ d0b16d056e0f681a2bc6bd70859303b4bba521dc submodules/externals/libupnp (last_svn_1.6.x-322-gd0b16d0)
#~ c74bf6d889992c3cabe017ec353ca85c323107cd submodules/externals/libvpx (v1.4.0)
#~ c943f708f1853de4eb15e5a94cf0b35d108da87a submodules/externals/libxml2 (v2.8.0)
#~ 3b67218fb8efb776bcd79e7445774e02d778321d submodules/externals/opencore-amr (v0.1.3-10-g3b67218)
#~ 3a75956fb2584cca84a95ba1fcbc72fa2c91f98d submodules/externals/openh264 (v1.1-1158-g3a75956)
#~ 4e8acd5ca305f385715a2c36642fdbc91503134f submodules/externals/opus (draft-ietf-codec-oggopus-03)
#~ ab2f403a3e0ec91257f0e943129c0eec272f34e8 submodules/externals/polarssl (remotes/origin/polarssl-1.4-31-gab2f403)
#~ 0aa712946869bc6faba5ada29d0d4a2b4a702b46 submodules/externals/speex (0aa7129)
#~ a752d47da1a80e634f81a57bb975b6a80fc14ffa submodules/externals/srtp (remotes/origin/cvs-20120427-19-ga752d47)
#~ e96f4b5d3fe38b3bee5d71fefdaa1af8b842dde9 submodules/externals/vo-amrwbenc (v0.1.1-8-ge96f4b5)
#~ -2117f353f82da43f648b10dc6bd99f55e0d44c3f submodules/externals/webrtc
#~ adc99d17d8c1fbc164fae8319b40d7c45f30314e submodules/externals/x264 (adc99d1)
#~ b33fd88ba1e59dba05c0a3d8d839d523c2fbc1af submodules/libilbc-rfc3951 (debian/0.6-2-25-gb33fd88)
#~ e7dd35efa0f0d250db66fadb11994b4f48e088b1 submodules/linphone (3.8.5-395-ge7dd35e)
#~ 61ab5cb11a63a5f8e10cced9d481979718199954 submodules/msamr (1.0.1~29)
#~ 64df6cb53ceacdea74a021e83f664e9aaa140ef8 submodules/mscodec2 (heads/master)
#~ e027979b567d13b221dfd4312149cd2820a496ff submodules/msilbc (debian/2.0.3-1-28-ge027979)
#~ 9be271700b377a251b95e9d8dbe0e8b878a403ac submodules/msopenh264 (1.1.1~5)
#~ a687fa9608d0257273adac6d10f2a8c8a8d50589 submodules/mssilk (1.0.1~4)
#~ 5a55409ddce8bc35163662231dae475488dfce75 submodules/mswebrtc (5a55409)
#~ 58941c15dcf23d6b47cdc557b399dc6e7ce2b0b5 submodules/msx264 (1.5.1~3)


set -e  # stop this script on error

MAKEFLAGS="-j 8"     # use parallel threads when building
MAINDIR=`realpath ${0%/*}`





######################################################################
##### Part 1: Build on the local host for the local architecture #####
######################################################################


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
  echo "#############################################################"
  echo "# Building 'bcunit' for $ARCH (build-dep for belle-sip) $ARCH..."
  echo "#############################################################"
  echo

  # Get 'src-deps/bcunit': git clone https://github.com/BelledonneCommunications/bcunit.git
  cd $MAINDIR/src-deps/bcunit
  ./autogen.sh
  ./configure  --prefix=$MAINDIR/build --enable-static #  --disable-shared
  make $MAKEFLAGS -w install

  echo
  echo "#############################################################"
  echo "# Building 'bctoolbox' (build-dep for belle-sip) for $ARCH..."
  echo "#############################################################"
  echo

  # Get 'src-deps/bctoolbox': git clone https://github.com/BelledonneCommunications/bctoolbox.git
  cd $MAINDIR/src-deps/bctoolbox
  rm -f CMakeCache.txt
  cmake . -DCMAKE_INSTALL_PREFIX=$MAINDIR/build -DBUILD_SHARED_LIBS=OFF -DENABLE_POLARSSL=ON -DENABLE_MBEDTLS=OFF
  make clean
  make $MAKEFLAGS -w install
  
  echo
  echo "#############################################################"
  echo "# Configuring belle-sip for $ARCH ..."
  echo "#############################################################"
  echo

  cd $MAINDIR/src/submodules/belle-sip
  ./autogen.sh

  echo

  PKG_CONFIG_PATH=$MAINDIR/build/lib/pkgconfig \
  ./configure --prefix=$MAINDIR/build --disable-shared --enable-static --disable-tests

  echo
  echo "#############################################################"
  echo "# Cleaning belle-sip ..."
  echo "#############################################################"
  echo

  make clean

  echo

  echo "#############################################################"
  echo "# Building belle-sip for $ARCH ..."
  echo "#############################################################"
  echo

  make $MAKEFLAGS -w install # DESTDIR=$MAINDIR/build
  


  echo
  echo "#############################################################"
  echo "# Configuring speex for $ARCH ..."
  echo "#############################################################"
  echo

  cd $MAINDIR/src/submodules/externals/speex
  ./autogen.sh

  echo

  PKG_CONFIG_PATH=$MAINDIR/build/lib/pkgconfig \
  ./configure --prefix=$MAINDIR/build --disable-shared --enable-static

  echo
  echo "#############################################################"
  echo "# Cleaning speex ..."
  echo "#############################################################"
  echo

  make clean

  echo

  echo "#############################################################"
  echo "# Building speex for $ARCH ..."
  echo "#############################################################"
  echo

  make $MAKEFLAGS -w install # DESTDIR=$MAINDIR/build



  echo
  echo "#############################################################"
  echo "# Configuring and building 'ffmpeg' for $ARCH ..."
  echo "#############################################################"
  echo
    # Note: Configure options as issued by the Android build system:
    # --target-os=linux --enable-cross-compile --enable-runtime-cpudetect --disable-everything --disable-doc --disable-ffplay --disable-ffmpeg --disable-ffprobe --disable-ffserver --disable-avdevice --disable-avfilter --disable-avformat --disable-swresample --disable-network --enable-decoder=mjpeg --enable-encoder=mjpeg --enable-decoder=mpeg4 --enable-encoder=mpeg4 --enable-decoder=h264 --enable-decoder=h263p --enable-encoder=h263p --enable-decoder=h263 --enable-encoder=h263 --extra-cflags="-w" --disable-static --enable-shared --disable-symver --build-suffix=-linphone-arm --arch=arm --sysroot=/opt/android-ndk/platforms/android-14/arch-arm --cross-prefix=/arm-linux-androideabi- --enable-pic

  cd $MAINDIR/src/submodules/externals/ffmpeg/
  ./configure --prefix=$MAINDIR/build \
      --disable-everything --disable-doc --disable-ffplay --disable-ffmpeg \
      --disable-ffprobe --disable-ffserver --disable-avdevice --disable-avfilter \
      --disable-avformat --disable-swresample --disable-network \
      --enable-decoder=mjpeg --enable-encoder=mjpeg --enable-decoder=mpeg4 \
      --enable-encoder=mpeg4 --enable-decoder=h264 --enable-decoder=h263p \
      --enable-encoder=h263p --enable-decoder=h263 --enable-encoder=h263

  #~ ./configure --prefix=$MAINDIR/build --enable-gpl
  make clean
  make $MAKEFLAGS install
  make clean
  rm -f config.h config.asm config.fate config.mak    # cleanup config files (Android build will fail otherwise)

  #~ echo
  #~ echo "#############################################################"
  #~ echo "# Configuring and building 'polarssl' for $ARCH ..."
  #~ echo "#############################################################"
  #~ echo
  #~
  #~ cd $MAINDIR/src/submodules/polarssl
  #~ ./configure --prefix=$MAINDIR/build --enable-static --disable-shared
  #~ make $MAKEFLAGS install

  echo
  echo "#############################################################"
  echo "# Configuring linphone with oRTP and mediastreamer2 for $ARCH ..."
  echo "#############################################################"
  echo

  cd $MAINDIR/src/submodules/linphone
  ./autogen.sh

  echo

  PKG_CONFIG_PATH="$MAINDIR/build/lib/pkgconfig" \
  CFLAGS="-I$MAINDIR/build/include" \
  LDFLAGS="-L$MAINDIR/build/lib -lm -lXext -lstdc++" \
  ./configure --prefix=$MAINDIR/build \
          --with-ffmpeg=$MAINDIR/build \
          --disable-tutorials --disable-tests --disable-documentation --disable-gtk_ui \
          --disable-shared --enable-static --disable-strict \
          --disable-nls --disable-oss --disable-pulseaudio --disable-gsm --disable-matroska --disable-spandsp --disable-upnp --disable-opus \
          --enable-x11 --disable-xv --disable-glx --disable-sdl --disable-theora --enable-vp8 --disable-g729bCN --with-srtp=none --disable-zlib --disable-notify \
          --enable-date --enable-alsa --disable-lime --disable-msg-storage
    # WORKAROUND: '-lstdc++' is only added due to a building bug in 3.2.1 (2016-12-09)
  echo

  echo "#############################################################"
  echo "# Cleaning linphone ..."
  echo "#############################################################"
  echo

  make clean

  echo
  echo "#############################################################"
  echo "# Building linphone for $ARCH ..."
  echo "#############################################################"
  echo

  # test -f linphone/coreapi/liblinphone_gitversion.h \
  #   || echo "#define LIBLINPHONE_GIT_VERSION \""${VER_LINPHONE}"\"" > linphone/coreapi/liblinphone_gitversion.h
  make $MAKEFLAGS -w install

  echo
  echo "#############################################################"
  echo "# Extracting results to 'usr.new/$ARCH' ..."
  echo "#############################################################"
  echo

  cd $MAINDIR
  mkdir -p usr.new/$ARCH

  # Fast, destructive version...
  rm build/lib/*.so*      # remove (optional) shared libraries to avoid confusion  
  mv build/include usr.new/$ARCH/include
  mv build/lib usr.new/$ARCH/lib
  rm -fr build

  # Non-destructive version (for debugging libs)...
  #~ cp -a build/include usr.new/$ARCH
  #~ cp -a build/lib usr.new/$ARCH
  #~ rm usr.new/$ARCH/lib/*.so*      # remove (optional) shared libraries to avoid confusion  





########################################
#####   Part 2: Build everything   #####
########################################


else # if [[ "$1" == "-l" ]];

  ##### Cleanup everything #####

  rm -fr build include usr.new


  ##### Build on all supported Debian architectures  #####

  ssh lilienthal  `realpath $0` -l    # i386
  ssh obelix      `realpath $0` -l    # amd64
  ssh lennon      `realpath $0` -l    # armhf


  ##### Extract Debian packages that cannot be installed by foreign architecture #####

  echo
  echo
  echo "#####################################################################"
  echo "##### Extracting from existing Debian packages ...              #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR
  mkdir build
  shopt -s nullglob   # for empty directories in 'mv ...' commands
  for ARCH in amd64 armhf; do
    for P in src-deps/*$ARCH.deb; do
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
    mkdir -p usr/$ARCH/lib
    mv build/usr/lib/*.a build/usr/lib/$GNU_ARCH/*.a usr/$ARCH/lib
    rm -fr build
  done
  shopt -u nullglob


  ##### Build Android SDK #####

  export PATH=/opt/android-ndk-r10d/:$PATH
    # linphone 2.5.1 needs NDK 10d(!) and does not accept symlinks in the path

  echo
  echo
  echo "#####################################################################"
  echo "##### Pre-building for Android APK ...                          #####"
  echo "#####################################################################"
  echo

  cd $MAINDIR/src

  echo "#############################################################"
  echo "# Cleaning Android SDK ...                                  #"
  echo "#############################################################"
  echo

  ./prepare.py -c

  echo "#############################################################"
  echo "# Preparing Android SDK ...                                  #"
  echo "#############################################################"
  echo

  ./prepare.py -DENABLE_NON_FREE_CODECS=ON -DENABLE_H263P=ON -DENABLE_H263=ON -DENABLE_SILK=OFF armv7
  echo

  echo "#############################################################"
  echo "# Building Android SDK ...                                  #"
  echo "#############################################################"
  echo

  make $MAKEFLAGS generate-sdk BUILD_WEBRTC_ISAC=0 BUILD_WEBRTC_AECM=0 BUILD_OPENH264=0 BUILD_SILK=0 BUILD_G729=0 BUILD_FOR_X86=0 BUILD_SQLITE=0 BUILD_OPUS=0 BUILD_UPNP=0

  echo

  echo "#############################################################"
  echo "# Extracting results to 'usr.new/android' ...               #"
  echo "#############################################################"
  echo

  mkdir -p $MAINDIR/usr.new/android/lib
  ln -s ../i386/include $MAINDIR/usr.new/android/include    # "borrow" the include dir from 'i386'
  cp -va libs/armeabi-v7a/*.so $MAINDIR/usr.new/android/lib
  cp -va bin/*.jar libs/*.jar $MAINDIR/usr.new/android/lib

  echo


  ##### Final cleanup #####

  echo "#############################################################"
  echo "# Final cleanup ...                                         #"
  echo "#############################################################"
  echo

  cd $MAINDIR/src
  make clean
  rm *sdk*.zip


fi # if [[ "$1" == "-l" ]];
