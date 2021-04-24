#include "xproto.h"
#include "xc_misc.h"
#include "xcbext.h"

char* xcb_setup_failed_reason(const xcb_setup_failed_t* arg0)
{
    return "";
}
int xcb_setup_failed_reason_length(const xcb_setup_failed_t* R)
{
    return 0;
}
xcb_query_extension_cookie_t xcb_query_extension(xcb_connection_t* arg0, uint16_t arg1, const char* arg2)
{
    xcb_query_extension_cookie_t c;
    c.sequence = 0;
    return c;
}
xcb_query_extension_reply_t* xcb_query_extension_reply(xcb_connection_t* arg0, xcb_query_extension_cookie_t arg1, xcb_generic_error_t** arg2)
{
    return 0;
}
char* xcb_setup_authenticate_reason(const xcb_setup_authenticate_t* arg0)
{
    return "";
}
int xcb_setup_authenticate_reason_length(const xcb_setup_authenticate_t* R)
{
    return 0;
}
xcb_xc_misc_get_xid_range_cookie_t xcb_xc_misc_get_xid_range(xcb_connection_t* arg0)
{
    xcb_xc_misc_get_xid_range_cookie_t c;
    c.sequence = 0;
    return c;
}
xcb_xc_misc_get_xid_range_reply_t* 
xcb_xc_misc_get_xid_range_reply(xcb_connection_t* c,
    xcb_xc_misc_get_xid_range_cookie_t   cookie  /**< */,
    xcb_generic_error_t** e)
{
    return 0;
}

xcb_extension_t xcb_xc_misc_id;