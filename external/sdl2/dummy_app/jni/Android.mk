#~ LOCAL_PATH := $(call my-dir)

#~ HIDAPI_ROOT_ABS:= $(LOCAL_PATH)/../../src/SDL2/src/hidapi/android


##### SDL2 #####

include ../../src/SDL/Android.mk


#~ # Create a static version of the HID API (would be required by SDL2 >= 2.0.9) ...
#~ include $(CLEAR_VARS)

#~ LOCAL_CPPFLAGS += -std=c++11

#~ LOCAL_SRC_FILES := $(HIDAPI_ROOT_ABS)/hid.cpp

#~ LOCAL_MODULE := hidapi_static
#~ LOCAL_MODULE_FILENAME := libhidapi

#~ include $(BUILD_STATIC_LIBRARY)



##### SDL2_ttf (and its subprojects) ####

include ../../src/SDL_ttf/Android.mk

#~ include $(CLEAR_VARS)
#~ include ../../src/SDL_ttf/external/freetype/Android.mk

#~ include $(CLEAR_VARS)
#~ include ../../src/SDL_ttf/external/harfbuzz/Android.mk



#~ # Extensions for static library...
#~ LOCAL_MODULE := SDL2_ttf_static
#~ LOCAL_MODULE_FILENAME := libSDL2_ttf
#~ LOCAL_STATIC_LIBRARIES := SDL2_static

#~ include $(BUILD_STATIC_LIBRARY)
