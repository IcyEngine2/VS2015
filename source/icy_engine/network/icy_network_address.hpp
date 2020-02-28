#pragma once

#include "icy_network_socket.hpp"
#include <icy_engine/core/icy_array.hpp>

class icy::detail::network_address_query
{
public:
    network_address_query() noexcept = default;
    network_address_query(const network_address_query&) = delete;
    ~network_address_query() noexcept
    {
        if (info && free) free(info);
        if (event) CloseHandle(event);
    }
    static icy::error_type create(icy::network_address& addr, const icy::network_address_type type) noexcept;
    static icy::error_type create(icy::network_address& addr, const size_t addr_len, const sockaddr& addr_ptr) noexcept;
    static void __stdcall callback(DWORD, DWORD, OVERLAPPED* overlapped) noexcept;
    OVERLAPPED overlapped = {};
    PADDRINFOEX info = nullptr;
    HANDLE event = nullptr;
    icy::error_type error;
    icy::array<icy::network_address>* buffer = nullptr;
    decltype(&FreeAddrInfoExW) free = nullptr;
};
