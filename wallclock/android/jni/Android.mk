# This file is part of the Home2L project.
#
# (C) 2015-2021 Gundolf Kiefer
#
# Home2L is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Home2L is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Home2L. If not, see <https://www.gnu.org/licenses/>.


LOCAL_PATH := $(DIR_SRC)
#~ LOCAL_PATH := $(call my-dir)/../..



#################### Define static and shared libs #############################

EXT_STATIC_LIBS := SDL2_static SDL2_ttf_static
EXT_SHARED_LIBS :=

ifeq ($(WITH_PHONE),1)
	ifeq ($(PHONE_LIB),linphone)
		EXT_SHARED_LIBS += linphone-armeabi-v7a mediastreamer_base-armeabi-v7a mediastreamer_voip-armeabi-v7a ortp-armeabi-v7a
		#~ EXT_SHARED_LIBS := ffmpeg_shared linphone_shared			# 2.5.1
	endif
endif

ifeq ($(WITH_MUSIC),1)
	EXT_STATIC_LIBS += mpdclient
endif

ifeq ($(WITH_GSTREAMER),1)
	EXT_SHARED_LIBS += gstreamer_android
endif



########## Make lists of libs available to the Home2l build system #############


# Print shared libraries...
home2l_print_shlibs:
	@echo '$(EXT_SHARED_LIBS)'



#################### Main module ###############################################

include $(CLEAR_VARS)

LOCAL_MODULE := home2l-wallclock
LOCAL_CPP_EXTENSION := .C

LOCAL_SRC_FILES := $(shell make -s --no-print-directory -C $(LOCAL_PATH) print-sources| grep -v ^make)
LOCAL_CFLAGS := $(shell make -s --no-print-directory -C $(LOCAL_PATH) print-config | grep -v ^make)
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/../common $(LOCAL_PATH)/../resources

LOCAL_STATIC_LIBRARIES := $(EXT_STATIC_LIBS)
LOCAL_SHARED_LIBRARIES := $(EXT_SHARED_LIBS)

LOCAL_LDLIBS := -llog


include $(BUILD_SHARED_LIBRARY)



#################### Imported modules ##########################################

EXT_PATH := $(LOCAL_PATH)/../external
	# 'LOCAL_PATH' will be overwritten below

include $(EXT_PATH)/sdl2/Android.mk

ifeq ($(WITH_MUSIC),1)
	include $(EXT_PATH)/mpdclient/Android.mk
endif

ifeq ($(WITH_GSTREAMER),1)
	include $(EXT_PATH)/gstreamer/Android.mk
endif

ifeq ($(WITH_PHONE),1)
	include $(EXT_PATH)/$(PHONE_LIB)/Android.mk
endif
