#pragma once

#include "icy_chat_server.hpp"

using namespace icy;

error_type chat_server_application::exec(const guid& author, const chat_message& request) noexcept
{
    return error_type();
}