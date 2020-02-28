#include "icy_network_socket.hpp"

using namespace icy;
using namespace detail;

error_type detail::network_setopt(SOCKET sock, const int opt, int value, const int level) noexcept
{
    return network_func_setsockopt(sock, level, opt,
        reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR ? last_system_error() : error_type{};
}

decltype(&::setsockopt) detail::network_func_setsockopt;
decltype(&::shutdown) detail::network_func_shutdown;
decltype(&::closesocket) detail::network_func_closesocket;
decltype(&::WSASocketW) detail::network_func_socket;

void network_socket::shutdown() noexcept
{
    if (m_value != INVALID_SOCKET)
    {
        if (network_func_shutdown)
            network_func_shutdown(m_value, SD_BOTH);

        if (network_func_closesocket)
            network_func_closesocket(m_value);

        m_value = INVALID_SOCKET;
    }
}
error_type network_socket::initialize(const network_socket::type sock_type, const network_address_type addr_type) noexcept
{
    auto success = false;
    auto value = network_func_socket(addr_type == network_address_type::ip_v6 ? AF_INET6 : AF_INET,
        sock_type == type::tcp ? SOCK_STREAM : SOCK_DGRAM,
        sock_type == type::tcp ? IPPROTO_TCP : IPPROTO_UDP,
        nullptr, 0, WSA_FLAG_OVERLAPPED);

    if (value == INVALID_SOCKET)
        return last_system_error();

    ICY_SCOPE_EXIT{ if (!success) network_func_closesocket(value); };

    if (sock_type == type::tcp)
    {
        ICY_ERROR(network_setopt(value, TCP_NODELAY, TRUE, IPPROTO_TCP));
        ICY_ERROR(network_setopt(value, SO_KEEPALIVE, FALSE));
    }
    ICY_ERROR(network_setopt(value, SO_REUSEADDR, TRUE));
    ICY_ERROR(network_setopt(value, SO_RCVBUF, 0));
    ICY_ERROR(network_setopt(value, SO_SNDBUF, 0));

    timeval tv = {};

    if (network_func_setsockopt(value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
        return last_system_error();
    if (network_func_setsockopt(value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
        return last_system_error();

    shutdown();
    success = true;
    m_value = value;

    return {};
}