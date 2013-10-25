# Copyright 2012, albuer@gmail.cim

LOCAL_PATH:= $(call my-dir)

############# upil ###############
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
    ../upil_event/upil_event.c \
    ../upil_event/upil_event_wrapper.c \
    ../state_machine/handler.c \
    ../upil_debug.c \
    ../state_machine/state_machine.c \
    handler_test.c

#    ../state_machine/state_machine.c
LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_C_INCLUDES := \
        $(LOCAL_PATH)/../include \
        $(LOCAL_PATH)/..

LOCAL_CFLAGS :=

LOCAL_MODULE:= upil_test

LOCAL_LDLIBS += -lpthread

include $(BUILD_EXECUTABLE)


