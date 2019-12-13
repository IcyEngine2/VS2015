#pragma once

#include <icy_module/icy_auth.hpp>
#include <icy_lib/icy_database.hpp>
#include <icy_lib/icy_crypto.hpp>
#include <icy_engine/icy_core.hpp>

static constexpr auto auth_date_length = sizeof("YYYY-MM-DD HH:MM:SS");
class auth_config_dbase;

struct auth_client_data
{
    icy::crypto_key password;
    icy::guid guid;
    char date[auth_date_length] = {};
};
struct auth_module_data
{
    icy::crypto_key password;
    icy::guid guid;
    char date[auth_date_length] = {};
    char addr[icy::auth_addr_length] = {};
    uint32_t timeout = 0;
};
enum class auth_query_flag : uint32_t
{
    none = 0x00,
    client_name = 0x01,
    client_guid = 0x02,
    client_date = 0x04,
    module_name = 0x08,
    module_guid = 0x10,
    module_date = 0x20,
    module_addr = 0x40,
    module_time = 0x80, //  timeout
};

inline auth_query_flag operator|(const auth_query_flag lhs, const auth_query_flag rhs) noexcept
{
    return auth_query_flag(uint32_t(lhs) | uint32_t(rhs));
}
inline bool operator&(const auth_query_flag lhs, const auth_query_flag rhs) noexcept
{
    return !!(uint32_t(lhs)& uint32_t(rhs));
}

struct auth_client_query_data
{
    uint64_t name = 0;
    icy::guid guid = {};
    icy::clock_type::time_point date = {};
};
struct auth_module_query_data
{
    uint64_t name = 0;
    icy::guid guid;
    icy::clock_type::time_point date = {};
    char addr[icy::auth_addr_length] = {};
    icy::duration_type timeout = {};
};
struct auth_query_callback
{
    virtual icy::error_type operator()(const auth_client_query_data&) noexcept
    {
        return {};
    }
    virtual icy::error_type operator()(const auth_module_query_data&) noexcept
    {
        return {};
    }
};

class auth_database
{
public:
    icy::error_type initialize(const auth_config_dbase& config) noexcept;
    icy::error_type exec(const icy::auth_request& request, icy::auth_response& response) noexcept;
    icy::error_type query(const auth_query_flag flags, auth_query_callback& callback) const noexcept;
    icy::error_type clear_clients() noexcept;
    icy::error_type clear_modules() noexcept;
private:
    icy::error_type exec(const icy::auth_request& request, const icy::auth_request_msg_client_create& input, icy::auth_response_msg_client_create& output) noexcept;
    icy::error_type exec(const icy::auth_request& request, const icy::auth_request_msg_client_ticket& input, icy::auth_response_msg_client_ticket& output) noexcept;
    icy::error_type exec(const icy::auth_request& request, const icy::auth_request_msg_client_connect& input, icy::auth_response_msg_client_connect& output) noexcept;
    icy::error_type exec(const icy::auth_request& request, const icy::auth_request_msg_module_create& input, icy::auth_response_msg_module_create& output) noexcept;
    icy::error_type exec(const icy::auth_request& request, const icy::auth_request_msg_module_update& input, icy::auth_response_msg_module_update& output) noexcept;
private:
    icy::database_system_write m_data;
    icy::database_dbi m_dbi_usr;
    icy::database_dbi m_dbi_mod;
    size_t m_max_clients = 0;
    size_t m_max_modules = 0x10;
    std::chrono::system_clock::duration m_timeout = {};
    icy::crypto_key m_password = icy::crypto_random;
};