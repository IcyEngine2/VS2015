#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_input.hpp>

#define MBOX_VERSION_MAJOR  4
#define MBOX_VERSION_MINOR  0
#define MBOX_MULTICAST_ADDR "236.22.83.171"_s
#define MBOX_MULTICAST_PORT "7608"_s
#define MBOX_MULTICAST_PING icy::guid(0xF99B3FEC4B7A4696, 0xA220924FC4175C22)

enum class mbox_request_type : uint32_t
{
    none,
    ping,
    exec,
    input,
};
enum class mbox_source_type
{
    none,
    client,
    launch,
    server,
};

struct mbox_udp_request
{
    mbox_udp_request() noexcept : uid(MBOX_MULTICAST_PING)
    {

    }
    icy::guid uid;
    mbox_request_type type = mbox_request_type::none;
    mbox_source_type source = mbox_source_type::none;
    union
    {
        icy::input_message input[4];
        char text[64];
    };
};