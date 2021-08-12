#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_auth/icy_auth.hpp>

using icy::operator""_s;

class auth_config_network
{
public:
    struct key
    {
        static constexpr icy::string_view http = "http"_s;
        static constexpr icy::string_view file_path = "logPath"_s;
        static constexpr icy::string_view file_size = "logSize"_s;
        static constexpr icy::string_view errors = "errors"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const auth_config_network& src, auth_config_network& dst) noexcept;
public:
    icy::network_server_config http;
    icy::string file_path;
    size_t file_size = 0;
    icy::array<uint32_t> errors;  //  error codes
    bool any_error = false;
};
class auth_config_dbase
{
public:
    struct key
    {
        static constexpr icy::string_view file_path = "filePath"_s;
        static constexpr icy::string_view file_size = "fileSize"_s;  //  integer (mb)
        static constexpr icy::string_view timeout = "timeout"_s;
        static constexpr icy::string_view clients = "clients"_s;
        static constexpr icy::string_view modules = "modules"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const auth_config_dbase& src, auth_config_dbase& dst) noexcept;
public:
    icy::string file_path;
    size_t file_size = 0;
    icy::auth_clock::duration timeout = {};
    size_t clients = 0;
    size_t modules = 0;
};

class auth_config
{
public:
    struct default_values
    {
        static constexpr auto gheap_size = 64 << 20;
        static constexpr auto file_path = "auth_config.txt"_s;
    };
    struct key
    {
        static constexpr icy::string_view gheap_size = "heapSize"_s;
        static constexpr icy::string_view dbase = "database"_s;
        static constexpr icy::string_view client = "client"_s;
        static constexpr icy::string_view module = "module"_s;
        static constexpr icy::string_view admin = "admin"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept;
    icy::error_type to_json(icy::json& output) const noexcept;
    static icy::error_type copy(const auth_config& src, auth_config& dst) noexcept;
public:
    size_t gheap_size = 0;
    auth_config_dbase dbase;
    auth_config_network client;
    auth_config_network module;
    auth_config_network admin;
};

namespace auth_event_type
{
    static constexpr auto stop_http_client = icy::event_user(0x00);
    static constexpr auto stop_http_module = icy::event_user(0x01);
    static constexpr auto stop_http_admin = icy::event_user(0x02);
    static constexpr auto run_http_client = icy::event_user(0x03);
    static constexpr auto run_http_module = icy::event_user(0x04);
    static constexpr auto run_http_admin = icy::event_user(0x05);
    static constexpr auto print_config = icy::event_user(0x06);
    static constexpr auto print_clients = icy::event_user(0x07);
    static constexpr auto print_modules = icy::event_user(0x08);
    static constexpr auto print_status = icy::event_user(0x09);
    static constexpr auto console_write = icy::event_user(0x0A);
    static constexpr auto console_error = icy::event_user(0x0B);
    static constexpr auto clear_clients = icy::event_user(0x0C);
    static constexpr auto clear_modules = icy::event_user(0x0D);
    static constexpr auto global_exit = icy::event_user(0x0E);
}