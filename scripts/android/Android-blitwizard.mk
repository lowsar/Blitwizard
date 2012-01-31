LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/../vorbis/include $(LOCAL_PATH)/../ogg/include $(LOCAL_PATH)/../imgloader/ $(LOCAL_PATH)/../box2d/ $(LOCAL_PATH)/../lua/

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.cpp \
    SOURCEFILELIST

LOCAL_SHARED_LIBRARIES := SDL zlib png imgloader ogg vorbis box2d lua

LOCAL_CFLAGS := -DVERSION="VERSIONINSERT"
LOCAL_LDLIBS := -lGLESv1_CM -llog

include $(BUILD_SHARED_LIBRARY)