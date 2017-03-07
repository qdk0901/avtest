LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	avplayer.cpp \
	main.cpp

LOCAL_CLANG := true
LOCAL_CPPFLAGS := 

LOCAL_MODULE := avtest

LOCAL_MODULE_TAGS := eng optional

LOCAL_SHARED_LIBRARIES := \
	libc \
	libstlport \
	libstagefright liblog libutils libbinder libstagefright_foundation \
    libmedia libgui libcutils libui libtinyalsa libusbhost libz
	
LOCAL_C_INCLUDES := \
	external/stlport/stlport \
    bionic/libstdc++/include \
    bionic \
	frameworks/av/media/libstagefright \
	$(TOP)/frameworks/native/include/media/openmax \
	
LOCAL_CFLAGS := 

LOCAL_LDFLAGS := -ldl

include $(BUILD_EXECUTABLE)

