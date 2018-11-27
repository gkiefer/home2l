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



include $(CLEAR_VARS)

LOCAL_MODULE := gstreamer_android
LOCAL_SRC_FILES := $(LOCAL_PATH)/usr/android/lib/libgstreamer_android.so
LOCAL_EXPORT_C_INCLUDES := \
		$(LOCAL_PATH)/usr/android/include/gstreamer-1.0 \
		$(LOCAL_PATH)/usr/android/include/glib-2.0

include $(PREBUILT_SHARED_LIBRARY)
