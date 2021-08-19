#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include "../icy_auth/icy_auth.hpp"

namespace icy
{
    static const string_view chat_public_hostname = "127.0.0.1:42890"_s;
    static const string_view chat_auth_module = "IcyChat"_s;
    extern const error_source error_source_chat;
    
    using chat_clock = auth_clock;
    using chat_time = chat_clock::time_point;

    static constexpr size_t chat_message_max_size = 512;

    enum class chat_error_code : uint32_t
    {
        none,
        invalid_type,
        invalid_version,
        invalid_utf_string,
        invalid_json,
        invalid_hostname,
        invalid_message_size,
        invalid_user,
        invalid_room,
        access_denied,
    };

    enum class chat_request_type : uint32_t
    {
        none,

        user_send_text,
        user_update,

        system_create_room,
        system_invite_user,
        system_remove_user,

        _total,
    };
    struct chat_request
    {
        error_type to_json(icy::json& output) const noexcept;
        error_type from_json(const icy::json& input) noexcept;
        uint64_t version = 0;
        crypto_msg<auth_client_connect_module> encrypted_module_connect;   //  with module password
        chat_request_type type = chat_request_type::none;
        icy::guid guid;
        icy::guid user;
        icy::guid room;
        chat_time time = {};
        string text;
    };
    struct chat_response
    {
        error_type to_json(icy::json& output) const noexcept;
        error_type from_json(const icy::json& input) noexcept;
        uint64_t version = 0;
        chat_error_code error = chat_error_code::none;
        icy::guid guid;
        icy::guid user;
        icy::guid room;
        chat_time time = {};
        string text;
    };
    
    struct chat_event
    {
        error_type error;
        icy::guid user;
        icy::guid room;
        chat_time time = {};
        string text;
    };
    struct chat_system : public icy::event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const chat_system*>(this)->thread());
        }
        virtual error_type connect(const auth_client_connect_client& client, const crypto_msg<auth_client_connect_module>& module) noexcept = 0;
        virtual error_type update() noexcept = 0;
        virtual error_type send_to_user(const string_view text, const icy::guid& user) noexcept = 0;
        virtual error_type send_to_room(const string_view text, const icy::guid& room) noexcept = 0;
    };
    error_type create_chat_system(shared_ptr<chat_system>& system) noexcept;

    extern const event_type chat_event_type;
}