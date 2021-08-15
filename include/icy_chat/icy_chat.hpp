#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include "../icy_auth/icy_auth.hpp"

namespace icy
{
    static const string_view chat_public_hostname = "127.0.0.1:42890"_s;
    static const string_view chat_auth_module = "IcyChat"_s;
    extern const error_source error_source_chat;
    
    static constexpr size_t chat_message_max_size = 512;

    enum class chat_error_code : uint32_t
    {
        none,
        invalid_format,
        invalid_type,
        invalid_version,
        invalid_utf_string,
        invalid_json,
    };

    enum class chat_message_type : uint32_t
    {
        none,

        user_connect,
        user_send,
        user_recv,

        system_connect,
        system_create_room,
        system_invite_user,
        system_remove_user,

        _total,
    };
    struct chat_message
    {
        uint64_t version = 0;
        crypto_msg<auth_client_connect_module> encrypted_module_connect;   //  with module password
        chat_message_type type = chat_message_type::none;
        guid user;
        guid room;
        auth_clock::time_point time = {};
        string text;
    };

    error_type to_json(const chat_message& input, icy::json& output) noexcept;
    error_type from_json(const icy::json& input, chat_message& output) noexcept;

    struct chat_event
    {
        error_type error;
        icy::guid user;
        icy::guid room;
        auth_clock::time_point time = {};
        string text;
    };
    struct chat_system : public icy::event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const chat_system*>(this)->thread());
        }
        virtual error_type connect() noexcept = 0;
        virtual error_type send_to_user(const string_view text, const icy::guid& user) noexcept = 0;
        virtual error_type send_to_room(const string_view text, const icy::guid& user) noexcept = 0;
    };
    error_type create_chat_system(shared_ptr<chat_system>& system, const string_view auth_hostname, const uint64_t username, const crypto_key& password) noexcept;

    extern const event_type chat_event_type;
}