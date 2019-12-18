#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_json.hpp>

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
        static constexpr icy::string_view file_path = "File Path"_s;
        static constexpr icy::string_view file_size = "File Size"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const dcl_client_config& src, dcl_client_config& dst) noexcept;
public:
    icy::string hostname;
    uint64_t username = 0;
    icy::string password;
    icy::string file_path;
    size_t file_size = 0;
};

namespace dcl_event_type
{
    static constexpr auto config = icy::event_user(0x00);
    using msg_config = dcl_client_config;

    static constexpr auto network_connect = icy::event_user(0x01);
    struct msg_network_connect
    {
        enum class state : uint32_t
        {
            none,
            request,    //  gui -> main (button click)
            cancel,     //  gui -> main (window close)
            append_log, //  main -> gui
        };
        state state = state::none;
        icy::string text;
    };

    static constexpr auto network_update = icy::event_user(0x02);
    struct msg_network_update
    {
        enum class state : uint32_t
        {
            none,
            request,    //  gui -> main (button click)
            cancel,     //  gui -> main (window close)
            append_log, //  main -> gui
        };
        state state = state::none;
        uint64_t version_client = 0;
        uint64_t version_server = 0;
        uint64_t progress_total = 0;
        uint64_t progress_citem = 0;
        icy::string text;
    };

    static constexpr auto network_upload = icy::event_user(0x03);
    struct msg_network_upload
    {
        enum class state : uint32_t
        {
            none,
            request,    //  gui -> main (button click)
            cancel,     //  gui -> main (window close)
            append_log, //  main -> gui
        };
        state state = state::none;
        icy::string desc;
    };
    
    static constexpr auto project_open = icy::event_user(0x04);
    struct msg_project_open 
    {
        icy::string file;
    };

    static constexpr auto project_close = icy::event_user(0x05);
    using msg_project_close = std::integral_constant<uint64_t, project_close>;

    static constexpr auto project_create = icy::event_user(0x06);
    struct msg_project_create
    {
        icy::string file;
    };

    static constexpr auto project_save = icy::event_user(0x07);
    using msg_project_save = std::integral_constant<uint64_t, project_save>;
}