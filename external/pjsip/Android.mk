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



include $(CLEAR_VARS)

LOCAL_MODULE := pjsip

LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libpjproject.a

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/usr/android/include/

LOCAL_EXPORT_CFLAGS := -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1
LOCAL_EXPORT_LDLIBS := -lOpenSLES -llog -lGLESv2 -lEGL -landroid

include $(PREBUILT_STATIC_LIBRARY)



#~ # ***** VPX *****

#~ include $(CLEAR_VARS)

#~ LOCAL_MODULE := vpx

#~ LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libvpx.a

#~ LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/usr/android/include/

#~ include $(PREBUILT_STATIC_LIBRARY)
