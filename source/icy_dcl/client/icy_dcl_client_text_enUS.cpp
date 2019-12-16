#include "icy_dcl_client_text.hpp"

using namespace icy;

icy::string_view to_string_enUS(const text text) noexcept
{
    switch (text)
    {
    case text::config: return "Config"_s;
    case text::connect: return "Connect"_s;
    case text::disconnect: return "Disconnect"_s;
    case text::hostname: return "Hostname"_s;
    case text::password: return "Password"_s;
    case text::update: return "Update"_s;
    case text::user: return "User"_s;
    case text::username: return "Username"_s;
    default:
        break;
    }
    return {};
}