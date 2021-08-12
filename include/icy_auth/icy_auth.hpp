#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/utility/icy_crypto.hpp>
#include <cstdint>

namespace icy
{
    static constexpr auto auth_addr_length = 64;
    enum class auth_error_code : uint32_t
    {
        none,

        invalid_json = 100,
        invalid_json_version,
        invalid_json_type,
        invalid_json_guid,
        invalid_json_time,
        invalid_json_message,
        invalid_json_username,
        invalid_json_password,
        invalid_json_error,
        
        client_access = 200,
        client_max_capacity,
        client_already_exists,
        client_not_found,
        client_invalid_password,
        client_invalid_ticket,
        client_expired_ticket,
        client_invalid_module,

        module_access = 300,
        module_max_capacity,
        module_already_exists,
        module_not_found,

        timeout = 400,
    };
    constexpr inline auth_error_code auth_error_category(const auth_error_code code) noexcept
    {
        return auth_error_code(uint32_t(code) / 100 * 100);
    }
    using auth_clock = std::chrono::system_clock;
    using auth_time = auth_clock::time_point;

    enum class auth_request_type : uint32_t
    {
        none,

        //  username; password -> error
        client_create,
        
        //  username; ticket*[encode with userpass] -> error; ticket*[encode with userpass]; ticket*[encode with serverpass]
        client_ticket,
        
        //  username; module; string(encoded_ticket)[with serverpass] -> error; ticket*[encode with userpass]; ticket*[encode with modulepass]
        client_connect,

        //  module -> error
        module_create,
        
        //  module; IP address; timeout; modulepass -> error
        module_update,

        _total,
    };

    extern const error_source error_source_auth;
    inline error_type make_auth_error(const auth_error_code code) noexcept
    {
        return error_type(static_cast<unsigned>(code), error_source_auth);
    }
    error_type to_string(const auth_request_type type, string& str) noexcept;
    
    struct auth_client_ticket_client
    {
        auth_time expire = {};
    };
    struct auth_client_ticket_server
    {
        auth_time expire = {};
    };
    struct auth_client_connect_client
    {
        auth_time expire = {};
        char address[auth_addr_length];
        crypto_key session;
    };
    struct auth_client_connect_module
    {
        auth_time expire = {};
        guid userguid;
        crypto_key session;
    };
    
    struct auth_request
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        guid guid;
        auth_request_type type = auth_request_type::none;
        auth_time time = {};
        uint64_t username = 0;
        json message;
    };
    struct auth_response
    {
    public:
        auth_response() noexcept
        {

        }
        auth_response(const auth_request& request) noexcept : guid(request.guid), type(request.type), time(request.time)
        {

        }
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        guid guid;
        auth_request_type type = auth_request_type::none;
        auth_time time = {};
        auth_error_code error = auth_error_code::none;
        json message;
    };

    struct auth_request_msg_client_create
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        crypto_key password;
    };
    struct auth_request_msg_client_ticket
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        crypto_msg<auth_time> encrypted_time;
    };
    struct auth_request_msg_client_connect
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        crypto_msg<auth_client_ticket_server> encrypted_server_ticket;
        uint64_t module = 0;    
    };
    struct auth_request_msg_module_create
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        uint64_t module = 0;
    };
    struct auth_request_msg_module_update
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        uint64_t module = 0;
        string address;
        crypto_key password;
        auth_clock::duration timeout = {};
    };    

    struct auth_response_msg_client_create
    {
        error_type from_json(const json&) noexcept
        {
            return {};
        }
        error_type to_json(json&) const noexcept
        {
            return {};
        }
    };
    struct auth_response_msg_client_ticket
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        crypto_msg<auth_client_ticket_client> encrypted_client_ticket;     //  with client password
        crypto_msg<auth_client_ticket_server> encrypted_server_ticket;     //  with server password
    };
    struct auth_response_msg_client_connect
    {
    public:
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
    public:
        crypto_msg<auth_client_connect_client> encrypted_client_connect;   //  with client password
        crypto_msg<auth_client_connect_module> encrypted_module_connect;   //  with module password
    };
    struct auth_response_msg_module_create
    {
    public:
        error_type from_json(const json&) noexcept
        {
            return {};
        }
        error_type to_json(json&) const noexcept
        {
            return {};
        }
    };
    struct auth_response_msg_module_update
    {
    public:
        error_type from_json(const json&) noexcept
        {
            return {};
        }
        error_type to_json(json&) const noexcept
        {
            return {};
        }
    };
}