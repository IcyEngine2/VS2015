#pragma once

#include <icy_engine/core/icy_string_view.hpp>

namespace mbox
{
    using icy::operator""_s;
    static const auto multicast_addr = "230.4.4.2"_s;
    static const auto multicast_port = "1844"_s;

    static const auto protocol_version = 0x2019'12'31;
    enum class command_type : uint32_t
    {
        none,
        exit,
        info,
        screen,
    };
    enum class command_arg_type : uint32_t
    {
        none,
        rectangle,
        string,
        image,
        info,
    };
    struct command_info
    {
        uint32_t process_id;
        char computer_name[32];
        char window_name[32];
        char process_name[32];
        char profile[32];
    };
    struct command
    {
        command() noexcept
        {
            memset(this, 0, sizeof(*this));
            version = protocol_version;
        }
        command(const command&) = delete;
        void* _unused;
        int version;
        icy::guid proc_uid;
        command_type cmd_type;
        icy::guid cmd_uid;
        command_arg_type arg_type;
        union
        {
            icy::rectangle rectangle;
            struct
            {
                size_t size;
                char bytes[sizeof(command_info) - sizeof(size_t)];
            } binary;
            command_info info;
        };
    };
}