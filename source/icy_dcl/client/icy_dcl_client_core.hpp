#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_qtgui/icy_qtgui.hpp>

using icy::operator""_s;

static constexpr auto dcl_client_max_progress = 10'000u;

class dcl_client_config
{
public:
    struct key
    {
        static constexpr icy::string_view hostname = "Hostname"_s;
        static constexpr icy::string_view username = "Username"_s;
        static constexpr icy::string_view password = "Password"_s;  
        static constexpr icy::string_view filepath = "Database Path"_s;
        static constexpr icy::string_view filesize = "Database Size"_s;
        static constexpr icy::string_view locale = "Locale"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const dcl_client_config& src, dcl_client_config& dst) noexcept;
public:
    icy::string hostname;
    uint64_t username = 0;
    icy::string password;
    icy::string filepath;
    size_t filesize = 0;
    icy::guid locale;
};
namespace dcl_event_type
{ 
    static constexpr auto network_connect_begin     =   icy::event_user(0x01);
    static constexpr auto network_connect_cancel    =   icy::event_user(0x02);
    static constexpr auto network_update_begin      =   icy::event_user(0x03);
    static constexpr auto network_update_changed    =   icy::event_user(0x04);
    static constexpr auto network_update_cancel     =   icy::event_user(0x05);
    static constexpr auto network_upload_begin      =   icy::event_user(0x06);
    static constexpr auto network_upload_changed    =   icy::event_user(0x07);
    static constexpr auto network_upload_cancel     =   icy::event_user(0x08);

    //  arg: dcl_index
    static constexpr auto show_context_menu         =   icy::event_user(0x09);
    //  arg: dcl_index
    static constexpr auto explorer_select           =   icy::event_user(0x0A);
}