#include "icy_network_address.hpp"

using namespace icy;
using namespace detail;

error_type detail::network_address_query::create(network_address& addr, const network_address_type type) noexcept
{
    auto af = AF_INET;
    addr = {};
    addr.m_addr_type = type;
    if (type == network_address_type::ip_v6)
    {
        addr.m_addr = static_cast<sockaddr*>(realloc(nullptr, addr.m_addr_len = sizeof(sockaddr_in6)));
        af = AF_INET6;
    }
    else
    {
        addr.m_addr = static_cast<sockaddr*>(realloc(nullptr, addr.m_addr_len = sizeof(sockaddr_in)));
    }
    if (!addr.m_addr)
        return make_stdlib_error(std::errc::not_enough_memory);
    memset(addr.m_addr, 0, addr.m_addr_len);
    addr.m_addr->sa_family = af;
    return {};
}
error_type detail::network_address_query::create(network_address& addr, const size_t addr_len, const sockaddr& addr_ptr) noexcept
{
    const auto new_addr = static_cast<sockaddr*>(icy::realloc(nullptr, addr_len));
    if (!new_addr)
        return make_stdlib_error(std::errc::not_enough_memory);

    addr.m_addr_len = addr_len;
    if (addr_ptr.sa_family == AF_INET)
        addr.m_addr_type = network_address_type::ip_v4;
    else if (addr_ptr.sa_family == AF_INET6)
        addr.m_addr_type = network_address_type::ip_v6;

    icy::realloc(addr.m_addr, 0);
    addr.m_addr = new_addr;
    memcpy(addr.m_addr, &addr_ptr, addr_len);
    return {};
}
void __stdcall detail::network_address_query::callback(DWORD, DWORD, OVERLAPPED* overlapped) noexcept
{
    const auto query = reinterpret_cast<network_address_query*>(overlapped);
    auto next = query->info;
    while (next)
    {
        network_address new_addr;
        if (const auto error = create(new_addr, next->ai_addrlen, *next->ai_addr))
        {
            query->error = error;
            break;
        }
        if (next->ai_canonname)
        {
            const auto name = const_array_view<wchar_t>(next->ai_canonname, wcslen(next->ai_canonname));
            if (const auto error = to_string(name, new_addr.m_name))
            {
                query->error = error;
                break;
            }
        }
        if (const auto error = query->buffer->push_back(std::move(new_addr)))
        {
            query->error = error;
            break;
        }
        next = next->ai_next;
    }
    SetEvent(query->event);
}


network_address::network_address(network_address&& rhs) noexcept
{
    memcpy(this, &rhs, sizeof(rhs));
    new (&m_name) string(std::move(rhs.m_name));
    rhs.m_addr = nullptr;
}
network_address::~network_address() noexcept
{
    if (m_addr)
        icy::realloc(m_addr, 0);
}
error_type network_address::query(array<network_address>& array, const string_view host, const string_view port, const duration_type timeout) noexcept
{
    auto lib = "ws2_32"_lib;
    ICY_ERROR(lib.initialize());

    auto done = false;
    auto wsaData = WSADATA{};

    const auto func_wsa_startup = ICY_FIND_FUNC(lib, WSAStartup);
    const auto func_wsa_cleanup = ICY_FIND_FUNC(lib, WSACleanup);
    const auto func_get_addr_info = ICY_FIND_FUNC(lib, GetAddrInfoExW);
    const auto func_cancel_addr_info = ICY_FIND_FUNC(lib, GetAddrInfoExCancel);
    const auto func_free_addr_info = ICY_FIND_FUNC(lib, FreeAddrInfoExW);

    if (false
        || !func_wsa_startup
        || !func_wsa_cleanup
        || !func_get_addr_info
        || !func_cancel_addr_info
        || !func_free_addr_info)
        return make_stdlib_error(std::errc::function_not_supported);

    if (func_wsa_startup(WINSOCK_VERSION, &wsaData) == SOCKET_ERROR)
        return last_system_error();

    ICY_SCOPE_EXIT{ func_wsa_cleanup(); };
    if (MAKEWORD(wsaData.wHighVersion, wsaData.wVersion) != WINSOCK_VERSION)
        return make_stdlib_error(std::errc::network_down);

    wchar_t* buf_addr = nullptr;
    wchar_t* buf_port = nullptr;
    size_t len_addr = 0;
    size_t len_port = 0;
    ICY_ERROR(host.to_utf16(buf_addr, &len_addr));
    ICY_ERROR(port.to_utf16(buf_port, &len_port));
    buf_addr = static_cast<wchar_t*>(icy::realloc(nullptr, (len_addr + 1) * sizeof(wchar_t)));
    buf_port = static_cast<wchar_t*>(icy::realloc(nullptr, (len_port + 1) * sizeof(wchar_t)));
    ICY_SCOPE_EXIT{ icy::realloc(buf_port, 0); };
    ICY_SCOPE_EXIT{ icy::realloc(buf_addr, 0); };
    if (!buf_addr || !buf_port)
        return make_stdlib_error(std::errc::not_enough_memory);

    ICY_ERROR(host.to_utf16(buf_addr, &len_addr));
    ICY_ERROR(port.to_utf16(buf_port, &len_port));
    buf_addr[len_addr] = 0;
    buf_port[len_port] = 0;

    auto hints = addrinfoexW{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;

    detail::network_address_query query;
    query.buffer = &array;
    query.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    query.free = func_free_addr_info;
    if (!query.event)
        return last_system_error();

    HANDLE cancel = nullptr;
    const auto error = func_get_addr_info(buf_addr, buf_port, NS_ALL, nullptr, &hints, &query.info, nullptr,
        &query.overlapped, &detail::network_address_query::callback, &cancel);

    if (error == 0)
    {
        detail::network_address_query::callback(uint32_t(error), 0, &query.overlapped);
    }
    else if (error != WSA_IO_PENDING)
    {
        return make_system_error(make_system_error_code(uint32_t(error)));
    }
    else if (WaitForSingleObject(query.event, ms_timeout(timeout)) == WAIT_TIMEOUT)
    {
        func_cancel_addr_info(&cancel);
        WaitForSingleObject(query.event, INFINITE);
        return make_stdlib_error(std::errc::timed_out);
    }
    return query.error;
}
error_type icy::copy(const network_address& src, network_address& dst) noexcept
{
    string str;
    ICY_ERROR(copy(src.m_name, str));
    auto new_addr = static_cast<sockaddr*>(icy::realloc(nullptr, src.m_addr_len));
    if (!new_addr && src.m_addr_len) return make_stdlib_error(std::errc::not_enough_memory);
    if (dst.m_addr) icy::realloc(dst.m_addr, 0);

    dst.m_addr_type = src.m_addr_type;
    dst.m_addr_len = src.m_addr_len;
    dst.m_addr = new_addr;
    dst.m_name = std::move(str);
    memcpy(new_addr, src.m_addr, src.m_addr_len);
    return {};
}
error_type icy::to_string(const network_address& address, string& str) noexcept
{
    if (!address.size())
    {
        str.clear();
        return {};
    }

    library lib = "ws2_32"_lib;
    ICY_ERROR(lib.initialize());

    const auto func = ICY_FIND_FUNC(lib, WSAAddressToStringW);
    if (!func)
        return make_stdlib_error(std::errc::function_not_supported);

    wchar_t buffer[64] = {};
    auto size = DWORD(sizeof(buffer) / sizeof(*buffer));
    if (func(address.data(), uint32_t(address.size()), nullptr, buffer, &size) == SOCKET_ERROR)
        return last_system_error();
    return to_string(const_array_view<wchar_t>(buffer, size), str);
}
