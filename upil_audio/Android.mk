LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/usb_audio/include \
                system/media/audio_utils/include \
		frameworks/base/include
LOCAL_SRC_FILES:= usb_audio_pcm.c mixer.c upil_audio.cpp
LOCAL_MODULE := libupil_audio
LOCAL_SHARED_LIBRARIES:= libcutils libutils libmedia libaudioutils
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_C_INCLUDES:= external/usb_audio/include \
		system/media/audio_utils/include 
#LOCAL_SRC_FILES:= usb_audio_pcm.c mixer.c
#LOCAL_MODULE := libusb_audio
#LOCAL_SHARED_LIBRARIES:= libcutils libutils libaudioutils
#LOCAL_MODULE_TAGS := optional
#LOCAL_PRELINK_MODULE := false
#include $(BUILD_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES:= uacap.c
#LOCAL_MODULE := uacap
#LOCAL_SHARED_LIBRARIES:= libcutils libutils libusb_audio
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_EXECUTABLE)

#include $(CLEAR_VARS)
#LOCAL_SRC_FILES:= uaplay.c
#LOCAL_MODULE := uaplay
#LOCAL_SHARED_LIBRARIES:= libcutils libutils libusb_audio
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_EXECUTABLE)


#include $(CLEAR_VARS)
#LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE := aplay
#LOCAL_SRC_FILES := \
        aplay.cpp 
#LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libmedia \
	libusb_audio
#LOCAL_C_INCLUDES += frameworks/base/include
#include $(BUILD_EXECUTABLE)

#include $(CLEAR_VARS)
#LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE := acap
#LOCAL_SRC_FILES := \
        acap.cpp
#LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libmedia \
	libusb_audio

#LOCAL_C_INCLUDES += frameworks/base/include

#include $(BUILD_EXECUTABLE)

#include $(CLEAR_VARS)
#LOCAL_MODULE_TAGS := optional
#LOCAL_MODULE := upil_audio
#LOCAL_SRC_FILES := upil_audio.cpp
#LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libutils \
        libmedia \
        libusb_audio
#LOCAL_C_INCLUDES += frameworks/base/include \
	system/media/audio_effects/include
#include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))

