#ifndef __UPIL_DEBUG_H__
#define __UPIL_DEBUG_H__

enum {
	MSG_EXCESSIVE, MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR
};

#ifdef ANDROID_LOG

void upil_hexdump(int level, const char *title, const uchar *data, size_t len);
void upil_printf(int level, char *format, ...);

#define upil_dump(t,d,l)            upil_hexdump(MSG_DEBUG, t,d,l)

#define upil_dbg(args...)           upil_printf(MSG_DEBUG, args)
#define upil_info(args...)          upil_printf(MSG_INFO, args)
#define upil_warn(args...)          upil_printf(MSG_WARNING, args)
#define upil_error(args...)         upil_printf(MSG_ERROR, args)

#endif

extern int upil_debug_level;
extern int upil_debug_ts;
extern int g_dbg_count;

#define ENTER_LEAVE_LOG(is_enter)     do { \
    char space_enter[2*32]; \
    int i_enter=0; \
    is_enter?0:--g_dbg_count; \
    for (;i_enter<g_dbg_count;i_enter++) \
        memcpy(space_enter+i_enter*2, "  ", 2); \
    space_enter[i_enter*2] = '\0'; \
    upil_printf(MSG_MSGDUMP, "%s(%d) [%s] %s", space_enter, \
        is_enter?g_dbg_count++:g_dbg_count, \
        __func__, \
        is_enter?"ENTER":"LEAVE"); \
}while(0)

#define ENTER_LOG()     ENTER_LEAVE_LOG(1)
#define LEAVE_LOG()     ENTER_LEAVE_LOG(0)
//#define ENTER_LOG()     upil_printf(MSG_DEBUG,"(%d) [%s] ENTER", g_dbg_count++, __func__)
//#define LEAVE_LOG()     upil_printf(MSG_DEBUG,"(%d) [%s] LEAVE", --g_dbg_count, __func__)

#endif

