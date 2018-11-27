##### SDL2 #####

include ../../src/SDL2/Android.mk



##### SDL2_ttf ####

include ../../src/SDL2_ttf/Android.mk

# Extensions for static library...
LOCAL_MODULE := SDL2_ttf_static
LOCAL_MODULE_FILENAME := libSDL2_ttf
LOCAL_STATIC_LIBRARIES := SDL2_static

include $(BUILD_STATIC_LIBRARY)
