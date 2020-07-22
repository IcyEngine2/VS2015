#pragma once

#include <icy_engine/core/icy_string_view.hpp>

namespace mbox
{
    using icy::operator""_s;
    static const auto multicast_addr = "230.4.4.2"_s;
    static const auto multicast_port = "1844"_s;

    static const auto protocol_version = 0x2020'01'02;

    struct ping
    {
        int version = protocol_version;
        char address[32] = {};
    };

    struct key
    {
        rel_ops(key);
        friend int compare(const key& lhs, const key& rhs) noexcept
        {
            return icy::compare(lhs.value, rhs.value);
        }
        union
        {
            struct
            {
                uint32_t pid;
                uint32_t hash;
            };
            uint64_t value;
        };
    };
    struct info
    {
        int version = protocol_version;
        key key;
        char computer_name[32];
        char window_name[64];
        char process_name[32];
        icy::guid profile;
    };

    enum class command_type : uint32_t
    {
        none,
        exit,
        input,
        image,
        profile,
    };
    struct command
    {
        int version = protocol_version;
        command_type type = command_type::none;
        uint32_t size = 0;
    };
}