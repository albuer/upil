# Copyright 2012, albuer@gmail.cim

LOCAL_PATH:= $(call my-dir)

############# hiddump ###############
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
    ../upil_event/upil_event.c \
    ../upil_event/upil_event_wrapper.c \
    ../state_machine/handler.c \
    ../state_machine/state_machine.c \
    hiddump.c \
    ../upil_debug.c \
    ../tp_protocol.c

#    ../state_machine/state_machine.c
LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/..

LOCAL_CFLAGS :=

LOCAL_MODULE:= hiddump

LOCAL_LDLIBS += -lpthread

include $(BUILD_EXECUTABLE)

############# hiddump ###############
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
    ../upil_event/upil_event.c \
    ../upil_event/upil_event_wrapper.c \
    ../state_machine/handler.c \
    ../state_machine/state_machine.c \
    ../upil_debug.c \
    ../tp_protocol.c \
    hidtool.c

#    ../state_machine/state_machine.c
LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/..

LOCAL_CFLAGS :=

LOCAL_MODULE:= hidtool

LOCAL_LDLIBS += -lpthread

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
