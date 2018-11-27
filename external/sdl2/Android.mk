# This file is part of the Home2L project.
#
# (C) 2015-2018 Gundolf Kiefer
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



##### SDL2 #####

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_static

LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libSDL2.a

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/usr/android/include/
LOCAL_EXPORT_LDLIBS := -Wl,--undefined=Java_org_libsdl_app_SDLActivity_nativeInit -ldl -lGLESv1_CM -lGLESv2 -llog -landroid

include $(PREBUILT_STATIC_LIBRARY)



##### SDL2_ttf #####

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_ttf_static
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libSDL2_ttf.a

include $(PREBUILT_STATIC_LIBRARY)
