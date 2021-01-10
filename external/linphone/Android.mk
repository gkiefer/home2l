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


LOCAL_PATH := $(call my-dir)



##### linphone #####

include $(CLEAR_VARS)

#~ LOCAL_MODULE := linphone_static
#~ LOCAL_SRC_FILES := $(LOCAL_PATH)/lib-android/liblinphone.a
#~
#~ LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/
#~ LOCAL_EXPORT_LDLIBS := -lffmpeg-linphone-arm
#~ #LOCAL_EXPORT_LDLIBS := -Wl,--undefined=Java_org_libsdl_app_SDLActivity_nativeInit -ldl -lGLESv1_CM -lGLESv2 -llog -landroid
#~
#~ include $(PREBUILT_STATIC_LIBRARY)


LOCAL_MODULE := linphone-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/liblinphone-armeabi-v7a.so
# $(LOCAL_PATH)/lib-android/libffmpeg-linphone-arm.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/usr/android/include/

include $(PREBUILT_SHARED_LIBRARY)



##### Submodules to be included as shared libs, too #####

include $(CLEAR_VARS)
LOCAL_MODULE := mediastreamer_base-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := mediastreamer_voip-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ortp-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

# Indirect dependencies...

include $(CLEAR_VARS)
LOCAL_MODULE := bctoolbox-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ffmpeg-linphone-armeabi-v7a
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := gnustl_shared
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
include $(PREBUILT_SHARED_LIBRARY)

#~ include $(CLEAR_VARS)
#~ LOCAL_MODULE := mssilk
#~ LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/lib$(LOCAL_MODULE).so
#~ include $(PREBUILT_SHARED_LIBRARY)





##### ffmpeg (2.5.1) #####

#~ include $(CLEAR_VARS)
#~ 
#~ LOCAL_MODULE := ffmpeg_shared
#~ LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libffmpeg-linphone-arm.so
#~ 
#~ include $(PREBUILT_SHARED_LIBRARY)

