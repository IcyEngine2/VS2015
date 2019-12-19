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
        static constexpr icy::string_view system_path = "System Project Path"_s;
        static constexpr icy::string_view system_size = "System Project Size"_s;
        static constexpr icy::string_view user_path = "User Project Path"_s;
        static constexpr icy::string_view user_size = "User Project Size"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const dcl_client_config& src, dcl_client_config& dst) noexcept;
public:
    icy::string hostname;
    uint64_t username = 0;
    icy::string password;
    icy::string system_path;
    icy::string user_path;
    size_t system_size = 0;
    size_t user_size = 0;
};
class dcl_client_network_model : public icy::gui_model
{
public:
    struct col
    {
        enum : uint32_t
        {
            time,
            message,
            error,
        };
    };
public:
    uint32_t data(const icy::gui_node node, const icy::gui_role role, icy::gui_variant& value) const noexcept override;
    icy::error_type data(const icy::gui_node node, const icy::gui_role role, icy::string& str) const noexcept;
private:
    struct data_type
    {
        icy::clock_type::time_point time;
        icy::string message;
        icy::string error;
    };
private:
    icy::array<data_type> m_data;
};
class dcl_client_upload_model : public icy::gui_model
{
public:
    uint32_t data(const icy::gui_node node, const icy::gui_role role, icy::gui_variant& value) const noexcept;
private:
    struct data_type
    {
        icy::gui_check_state check = icy::gui_check_state::unchecked;
        icy::guid guid;
        icy::string name;
    };
private:
    icy::array<data_type> m_data;
};
struct dcl_client_state : dcl_client_config
{
    uint64_t version_client = 0;
    uint64_t version_server = 0;
    uint64_t bytesize_current = 0;
    uint64_t bytesize_maximum = 0;
    dcl_client_network_model connect_network_model;
    dcl_client_network_model update_network_model;
    dcl_client_network_model upload_network_model;
    dcl_client_upload_model upload_data_model;
    icy::string upload_message;
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
    
    static constexpr auto project_request_open      =   icy::event_user(0x0A);
    static constexpr auto project_request_close     =   icy::event_user(0x0B);
    static constexpr auto project_request_create    =   icy::event_user(0x0C);
    static constexpr auto project_request_save      =   icy::event_user(0x0D);
    static constexpr auto project_opened            =   icy::event_user(0x0E);
    static constexpr auto project_closed            =   icy::event_user(0x0F);

    struct msg_project_request_open     { icy::string filename; };
}