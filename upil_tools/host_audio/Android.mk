LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := host_play

LOCAL_SRC_FILES := \
        hplay.cpp 

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libmedia

LOCAL_C_INCLUDES += frameworks/base/include

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := host_cap
LOCAL_SRC_FILES := \
        hcap.cpp
LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libmedia
LOCAL_C_INCLUDES += frameworks/base/include

include $(BUILD_EXECUTABLE)

