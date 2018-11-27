LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := tutorial-1
LOCAL_SRC_FILES := tutorial-1.c
LOCAL_SHARED_LIBRARIES := gstreamer_android
LOCAL_LDLIBS := -llog -landroid
include $(BUILD_SHARED_LIBRARY)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm
else ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif



##### Adapt GStreamer configuration below ######

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/

include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk

PLUGINS_CORE       := coreelements coretracers adder app audioconvert audiorate audioresample audiotestsrc gio rawparse typefindfunctions volume autodetect
PLUGINS_PLAYBACK   := $(GSTREAMER_PLUGINS_PLAYBACK)   # playback
PLUGINS_CODECS     := ogg vorbis ivorbisdec alaw audioparsers auparse flac icydemux id3demux mulaw speex wavpack wavparse midi androidmedia
PLUGINS_NET        := $(GSTREAMER_PLUGINS_NET)   # tcp rtsp rtp rtpmanager soup udp sdpelem srtp rtspclientsink
PLUGINS_SYS        := opensles

PLUGINS_EXTRA	     := level

GSTREAMER_PLUGINS  := $(PLUGINS_CORE) $(PLUGINS_PLAYBACK) $(PLUGINS_CODECS) $(PLUGINS_NET) $(PLUGINS_SYS) $(PLUGINS_EXTRA)

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
