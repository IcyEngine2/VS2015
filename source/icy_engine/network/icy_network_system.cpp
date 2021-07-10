#include "icy_network_system.hpp"
#include "icy_network_address.hpp"

using namespace icy;
using namespace detail;

static const auto http_str_endl01 = "\r\n"_s;
static const auto http_str_endl02 = "\r\n\r\n"_s;
static const auto http_str_header = "Content-Length: "_s;

static decltype(&::WSASend) network_func_send = nullptr;
static decltype(&::WSARecv) network_func_recv = nullptr;
static decltype(&::WSASendTo) network_func_send_to = nullptr;
static decltype(&::WSARecvFrom) network_func_recv_from = nullptr;
static decltype(&::WSAGetLastError) network_get_last_error = nullptr;
static LPFN_ACCEPTEX network_func_accept_ex = nullptr;
static LPFN_GETACCEPTEXSOCKADDRS network_func_get_accept_ex_sockaddrs = nullptr;
static LPFN_DISCONNECTEX network_func_disconnect_ex = nullptr;

error_type detail::network_func_init(library& lib) noexcept
{
    network_func_send = ICY_FIND_FUNC(lib, WSASend);
    network_func_recv = ICY_FIND_FUNC(lib, WSARecv);
    network_func_shutdown = reinterpret_cast<decltype(&::shutdown)>(lib.find("shutdown"));
    network_func_closesocket = ICY_FIND_FUNC(lib, closesocket);
    network_func_socket = ICY_FIND_FUNC(lib, WSASocketW);
    network_func_setsockopt = ICY_FIND_FUNC(lib, setsockopt);
    network_func_getsockopt = ICY_FIND_FUNC(lib, getsockopt);
    network_func_recv_from = ICY_FIND_FUNC(lib, WSARecvFrom);
    network_func_send_to = ICY_FIND_FUNC(lib, WSASendTo);
    network_get_last_error = ICY_FIND_FUNC(lib, WSAGetLastError);

    if (false
        || !network_func_send
        || !network_func_recv
        || !network_func_shutdown
        || !network_func_closesocket
        || !network_func_socket
        || !network_func_setsockopt
        || !network_func_getsockopt
        || !network_func_recv_from
        || !network_func_send_to
        || !network_get_last_error)
        return make_stdlib_error(std::errc::function_not_supported);

    return error_type();
}

error_type detail::network_connection::accept(network_system_data& system) noexcept
{
    ICY_ERROR(shutdown(std::chrono::seconds(0)));

    ICY_ERROR(ovl_recv.bytes.resize(system.addr_size() * 2));
    ICY_ERROR(socket.initialize(network_socket::type::tcp, system.m_config.addr_type));
    const auto iocp = CreateIoCompletionPort(reinterpret_cast<HANDLE>(SOCKET(socket)), system.m_iocp, 0, 0);
    if (iocp != system.m_iocp)
        return last_system_error();

    ovl_recv.conn = this;
    ovl_recv.type = event_type::network_connect;
    auto bytes = 0ul;
    if (!network_func_accept_ex(system.m_socket, socket, ovl_recv.bytes.data(), 
        0, system.addr_size(), system.addr_size(), &bytes, &ovl_recv.overlapped))
    {
        const auto error = network_get_last_error();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(error));
    }
    return error_type();
}
error_type detail::network_connection::recv() noexcept
{
    ICY_ASSERT(ovl_recv.offset < ovl_recv.bytes.size(), "TCP CONNECTION CORRUPTED");
    auto flags = 0ul;
    auto bytes = 0ul;
    WSABUF buf = {};
    buf.buf = reinterpret_cast<char*>(ovl_recv.bytes.data()) + ovl_recv.offset;
    buf.len = ULONG(ovl_recv.bytes.size()) - ovl_recv.offset;
    const auto ret = network_func_recv(socket, &buf, 1, &bytes, &flags, &ovl_recv.overlapped, nullptr);
    if (ret == -1)
    {
        const auto error = network_get_last_error();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(error));
    }
    return error_type();
};
error_type detail::network_connection::recv(array<uint8_t>&& bytes) noexcept
{
    if (bytes.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    network_tcp_overlapped new_ovl;
    network_tcp_overlapped* ptr = &new_ovl;
    if (ovl_recv.type == event_type::none)
        ptr = &ovl_recv;

    ptr->bytes = std::move(bytes);
    ptr->type = event_type::network_recv;
    ptr->conn = this;
    ptr->offset = 0;

    if (ptr != &new_ovl)
    {
        ICY_ERROR(recv());
    }
    else
    {
        ICY_ERROR(rqueue.push(std::move(new_ovl)));
    }
    return error_type();
}
error_type detail::network_connection::send() noexcept
{
    ICY_ASSERT(ovl_send.offset < ovl_send.bytes.size(), "TCP CONNECTION CORRUPTED");
    auto flags = 0ul;
    auto bytes = 0ul;
    WSABUF buf = {};
    buf.buf = reinterpret_cast<char*>(ovl_send.bytes.data()) + ovl_send.offset;
    buf.len = ULONG(ovl_send.bytes.size()) - ovl_send.offset;
    const auto ret = network_func_send(socket, &buf, 1, &bytes, 0, &ovl_send.overlapped, nullptr);
    if (ret == -1)
    {
        const auto error = network_get_last_error();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(error));
    }
    return error_type();
};
error_type detail::network_connection::send(array<uint8_t>&& bytes) noexcept
{
    if (bytes.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    network_tcp_overlapped new_ovl;
    network_tcp_overlapped* ptr = &new_ovl;
    if (ovl_send.type == event_type::none)
        ptr = &ovl_send;

    ptr->bytes = std::move(bytes);
    ptr->type = event_type::network_send;
    ptr->conn = this;
    ptr->offset = 0;

    if (ptr != &new_ovl)
    {
        ICY_ERROR(send());
    }
    else
    {
        ICY_ERROR(squeue.push(std::move(new_ovl)));
    }
    return error_type();
}
error_type detail::network_connection::disc() noexcept
{
    network_tcp_overlapped new_ovl;
    network_tcp_overlapped* ptr = &new_ovl;
    if (ovl_send.type == event_type::none)
        ptr = &ovl_send;

    ptr->type = event_type::network_disconnect;
    ptr->conn = this;
    if (ptr != &new_ovl)
    {
        if (!network_func_disconnect_ex(socket, &ovl_send.overlapped, 0, 0))
        {
            const auto error = network_get_last_error();
            if (error != WSA_IO_PENDING)
                return make_system_error(make_system_error_code(error));
        }
    }
    else
    {
        ICY_ERROR(squeue.push(std::move(new_ovl)));
    }
    return error_type();
};
error_type detail::network_connection::shutdown(const duration_type timeout) noexcept
{
    rqueue.clear();
    squeue.clear();

    if (socket == INVALID_SOCKET)
        return {};

    const auto handle = HANDLE(SOCKET(socket));
    auto error_send = error_type();
    auto error_recv = error_type();
    if (ovl_send.type)
    {
        if (!CancelIoEx(handle, &ovl_send.overlapped))
            error_send = last_system_error();
        if (error_send.code == 0 || error_send.code == ERROR_NOT_FOUND)
        {
            auto bytes = 0ul;
            if (!GetOverlappedResultEx(handle, &ovl_send.overlapped, &bytes, ms_timeout(timeout), TRUE))
                error_send = last_system_error();
        }
        ovl_send = {};
    }
    if (ovl_recv.type)
    {
        if (!CancelIoEx(handle, &ovl_recv.overlapped))
            error_recv = last_system_error();
        if (error_recv.code == 0 || error_recv.code == ERROR_NOT_FOUND)
        {
            auto bytes = 0ul;
            if (!GetOverlappedResultEx(handle, &ovl_recv.overlapped, &bytes, ms_timeout(timeout), TRUE))
                error_recv = last_system_error();
        }
        ovl_recv = {};
    }
    ICY_ERROR(error_send);
    ICY_ERROR(error_recv);
    return error_type();
}

void network_system_data::destroy(network_system_data*& ptr) noexcept
{
    if (ptr)
    {
        allocator_type::destroy(ptr);
        allocator_type::deallocate(ptr);
        ptr = nullptr;
    }
}
void network_system_data::shutdown() noexcept
{
    m_cmds.clear();
    m_conn.clear();

    m_socket.shutdown();
    if (m_iocp)
    {
        CloseHandle(m_iocp);
        m_iocp = nullptr;
    }
    if (m_wsa)
    {
        m_wsa();
        m_wsa = nullptr;
    }
    m_library.shutdown();
}
error_type network_system_data::create(network_system_data*& ptr, const bool http) noexcept
{
    auto new_ptr = allocator_type::allocate<network_system_data>(1);
    if (!new_ptr) return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_ptr);
    ICY_SCOPE_EXIT{ destroy(new_ptr); };
    ICY_ERROR(new_ptr->initialize(http));
    std::swap(ptr, new_ptr);
    return {};
}
error_type network_system_data::initialize(const bool http) noexcept
{
    shutdown();

    ICY_ERROR(m_library.initialize());

    auto done = false;
    auto wsaData = WSADATA{};

    const auto wsa_startup = ICY_FIND_FUNC(m_library, WSAStartup);
    const auto wsa_cleanup = ICY_FIND_FUNC(m_library, WSACleanup);

    if (!wsa_startup || !wsa_cleanup)
        return make_stdlib_error(std::errc::function_not_supported);

    if (wsa_startup(WINSOCK_VERSION, &wsaData) == SOCKET_ERROR)
        return last_system_error();

    ICY_SCOPE_EXIT{ if (!done) wsa_cleanup(); };
    if (MAKEWORD(wsaData.wHighVersion, wsaData.wVersion) != WINSOCK_VERSION)
        return make_stdlib_error(std::errc::network_down);

    auto iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocp)
        return last_system_error();
    ICY_SCOPE_EXIT{ if (!done) CloseHandle(iocp); };

    m_bind = reinterpret_cast<decltype(&::bind)>(m_library.find("bind"));
    m_htons = ICY_FIND_FUNC(m_library, htons);

    ICY_ERROR(network_func_init(m_library));

    if (!m_bind || !m_htons)
        return make_stdlib_error(std::errc::function_not_supported);

    m_iocp = iocp;
    m_wsa = wsa_cleanup;
    m_http = http;
    done = true;
    return {};
}
error_type detail::network_system_data::launch(const network_server_config& args, const network_socket::type sock_type) noexcept
{
    decltype(&::listen) network_func_listen = nullptr;
    if (sock_type == network_socket::type::tcp)
    {
        network_func_listen = ICY_FIND_FUNC(m_library, listen);
        if (!network_func_listen)
            return make_stdlib_error(std::errc::function_not_supported);
    }

    ICY_ERROR(m_socket.initialize(sock_type, args.addr_type));
    if (args.addr_type == network_address_type::ip_v6)
    {
        sockaddr_in6 addr = {};
        addr.sin6_port = m_htons(args.port);
        addr.sin6_family = AF_INET6;
        if (m_bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
            return last_system_error();
    }
    else
    {
        sockaddr_in addr = {};
        addr.sin_port = m_htons(args.port);
        addr.sin_family = AF_INET;
        if (m_bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
            return last_system_error();
    }
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(SOCKET(m_socket)), m_iocp, 0, 0))
        return last_system_error();

    ICY_ERROR(copy(args, m_config));
    if (network_func_listen)
    {
        if (network_func_listen(m_socket, int(args.queue)) == SOCKET_ERROR)
            return last_system_error();
        ICY_ERROR(m_conn.resize(args.capacity));
        ICY_ERROR(query_socket_func(m_library, m_socket, WSAID_ACCEPTEX, network_func_accept_ex));
        ICY_ERROR(query_socket_func(m_library, m_socket, WSAID_GETACCEPTEXSOCKADDRS, network_func_get_accept_ex_sockaddrs));
        ICY_ERROR(query_socket_func(m_library, m_socket, WSAID_DISCONNECTEX, network_func_disconnect_ex));
    }
    if (m_http)
    {
        for (auto&& conn : m_conn)
            ICY_ERROR(conn.accept(*this));
    }
    if (sock_type == detail::network_socket::type::udp)
    {
        ICY_ERROR(m_udp_recv.resize(args.capacity));

        for (auto&& udp : m_udp_recv)
        {
            udp.type = event_type::network_recv;
            ICY_ERROR(udp.bytes.resize(args.buffer));            
            WSABUF buf = { ULONG(udp.bytes.size()), reinterpret_cast<char*>(udp.bytes.data()) };
            auto bytes = 0ul;
            auto flags = 0ul;
            if (network_func_recv_from(m_socket, &buf, 1, &bytes, &flags, reinterpret_cast<sockaddr*>(udp.addr_buf),
                &udp.addr_len, &udp.overlapped, nullptr) == SOCKET_ERROR)
            {
                const auto error = last_system_error();
                if (error.code != make_system_error_code(ERROR_IO_PENDING))
                    return error;
            }
        }
    }
    return error_type();
}
error_type detail::network_system_data::connect(const network_address& address, 
    const const_array_view<uint8_t> bytes, const duration_type timeout, array<uint8_t>&& recv_buffer) noexcept
{
    network_address tmp;
    ICY_ERROR(network_address_query::create(tmp, address.addr_type()));

    LPFN_CONNECTEX network_func_connect = nullptr;
    ICY_ERROR(m_socket.initialize(network_socket::type::tcp, address.addr_type()));
    ICY_ERROR(query_socket_func(m_library, m_socket, WSAID_CONNECTEX, network_func_connect));

    if (m_bind(m_socket, tmp.data(), tmp.size()) == INVALID_SOCKET)
        return last_system_error();

    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(SOCKET(m_socket)), m_iocp, 0, 0))
        return last_system_error();

    OVERLAPPED ovl = {};
    auto count = 0ul;
    if (!network_func_connect(m_socket, address.data(), address.size(),
        const_cast<uint8_t*>(bytes.data()), DWORD(bytes.size()), &count, &ovl))
    {
        const auto error = network_get_last_error();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    OVERLAPPED_ENTRY entry = {};
    if (!GetQueuedCompletionStatusEx(m_iocp, &entry, 1, &count, ms_timeout(timeout), TRUE))
    {
        const auto error = last_system_error();
        if (error == make_system_error(make_system_error_code(ERROR_TIMEOUT)) ||
            error == make_system_error(make_system_error_code(WAIT_TIMEOUT)))
            return make_stdlib_error(std::errc::timed_out);
        else
            return error;
    }
    if (entry.lpOverlapped == &ovl)
    {
        if (network_func_setsockopt(m_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR)
            return last_system_error();
        ICY_ERROR(m_conn.resize(1));
        m_conn[0].socket = std::move(m_socket);
        if (m_http)
            ICY_ERROR(m_conn[0].recv(std::move(recv_buffer)));
        return {};
    }
    return make_stdlib_error(std::errc::not_connected);
}
error_type detail::network_system_data::cancel() noexcept
{
    if (!PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr))
        return last_system_error();
    return {};
}
error_type detail::network_system_data::post(const uint32_t conn, const event_type type, array<uint8_t>&& bytes, const network_address* const addr) noexcept
{
    if (m_config.port) // is server
    {
        if (m_socket.get_type() == network_socket::type::tcp)
        {
            if (conn == 0 || conn > m_conn.size())
                return make_stdlib_error(std::errc::invalid_argument);
        }
    }
    else
    {
        if (conn != 0)
            return make_stdlib_error(std::errc::invalid_argument);
    }
    if (type == event_type::network_connect || type == event_type::network_disconnect)
    {
        if (!bytes.empty())
            return make_stdlib_error(std::errc::invalid_argument);
    }
    else if (type == event_type::network_send || type == event_type::network_recv)
    {
        if (bytes.empty())
            return make_stdlib_error(std::errc::invalid_argument);
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }

    network_command cmd;
    cmd.conn = m_config.port ? conn : 1;
    cmd.bytes = std::move(bytes);
    cmd.type = type;
    if (addr)
        ICY_ERROR(copy(*addr, cmd.address));

    ICY_ERROR(m_cmds.push(std::move(cmd)));
    if (!PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr))
        return last_system_error();

    return error_type();
}
error_type detail::network_system_data::post(const uint32_t conn, unique_ptr<http_response>&& response) noexcept
{
    if (!response)
        return make_stdlib_error(std::errc::invalid_argument);

    if (m_config.port)
    {
        if (conn == 0 || conn > m_conn.size())
            return make_stdlib_error(std::errc::invalid_argument);
    }
    else if (conn)
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    
    string str;
    ICY_ERROR(to_string(*response, str));

    network_command cmd;
    cmd.conn = m_config.port ? conn : 1;
    cmd.type = event_type::network_send;
    ICY_ERROR(cmd.bytes.append(str.ubytes()));
    ICY_ERROR(cmd.bytes.append(response->body));
    cmd.http.response = std::move(response);

    ICY_ERROR(m_cmds.push(std::move(cmd)));
    if (!PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr))
        return last_system_error();

    return error_type();
}
error_type detail::network_system_data::post(const uint32_t conn, unique_ptr<http_request>&& request) noexcept
{
    if (!request || (m_config.port == 0 && conn == 0 || conn > m_conn.size()))
        return make_stdlib_error(std::errc::invalid_argument);

    string str;
    ICY_ERROR(to_string(*request, str));

    network_command cmd;
    cmd.conn = m_config.port ? conn : 1;
    cmd.type = event_type::network_send;
    ICY_ERROR(cmd.bytes.append(str.ubytes()));
    ICY_ERROR(cmd.bytes.append(request->body));
    cmd.http.request = std::move(request);

    ICY_ERROR(m_cmds.push(std::move(cmd)));
    if (!PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr))
        return last_system_error();

    return error_type();
}
error_type detail::network_system_data::loop_tcp(event_system& system) noexcept
{
    network_command cmd;
    while (m_cmds.pop(cmd))
    {
        auto conn = &m_conn[cmd.conn - 1];
        network_event event;
        switch (cmd.type)
        {
        case event_type::network_connect:
        {
            event.error = conn->accept(*this);
            break;
        }
        case event_type::network_disconnect:
        {
            event.error = conn->disc();
            break;
        }
        case event_type::network_recv:
        {
            event.error = conn->recv(std::move(cmd.bytes));
            break;
        }
        case event_type::network_send:
        {
            event.error = conn->send(std::move(cmd.bytes));
            conn->http.request = std::move(cmd.http.request);
            conn->http.response = std::move(cmd.http.response);
            break;
        }
        default:
            return make_stdlib_error(std::errc::function_not_supported);
        };
        event.conn = cmd.conn;

        if (event.error)
        {
            ICY_ERROR(event::post(&system, cmd.type, std::move(event)));
            event.error = conn->shutdown(m_config.timeout);
            ICY_ERROR(event::post(&system, event_type::network_disconnect, std::move(event)));
            if (m_http && m_config.port)
                ICY_ERROR(conn->accept(*this));
        }
    }

    OVERLAPPED_ENTRY entry = {};
    auto count = 0ul;

    SetLastError(ERROR_SUCCESS);
    GetQueuedCompletionStatus(m_iocp, &entry.dwNumberOfBytesTransferred,
        &entry.lpCompletionKey, &entry.lpOverlapped, INFINITE);
    
    auto ovl = reinterpret_cast<network_tcp_overlapped*>(entry.lpOverlapped);
    if (!ovl)
        return last_system_error();

    auto& conn = *ovl->conn;
    ovl->offset += entry.dwNumberOfBytesTransferred;    
    network_event event;

    if (m_config.port)
        event.conn = uint32_t(std::distance(m_conn.data(), &conn)) + 1;
    
    if (const auto error = last_system_error())
    {
        if (error == make_system_error(make_system_error_code(ERROR_IO_PENDING)))
            ;
        else
            event.error = error;
    }
    
    const auto type = ovl->type;
    auto update = ovl->offset == ovl->bytes.size();
    const auto is_disconnected = entry.dwNumberOfBytesTransferred == 0 && (
        ovl->type == event_type::network_recv || ovl->type == event_type::network_send);

    if (event.error || is_disconnected)
    {
        ICY_ERROR(event::post(&system, type, std::move(event)));
    }
    else
    {
        switch (type)
        {
        case event_type::network_disconnect:
        {
            update = true;
            break;
        }
        case event_type::network_connect:
        {
            if (network_func_setsockopt(conn.socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                reinterpret_cast<const char*>(&m_socket), sizeof(SOCKET)) == SOCKET_ERROR)
            {
                event.error = last_system_error();
            }
            else
            {
                sockaddr* client_addr = nullptr;
                sockaddr* server_addr = nullptr;
                auto client_size = 0;
                auto server_size = 0;
                network_func_get_accept_ex_sockaddrs(ovl->bytes.data(), uint32_t(ovl->bytes.size() - 2 * addr_size()),
                    addr_size(), addr_size(), &server_addr, &server_size, &client_addr, &client_size);
                event.error = network_address_query::create(event.address, size_t(client_size), *client_addr);
            }
            update = true;
            ICY_ERROR(event::post(&system, type, std::move(event)));
            break;
           /* if (m_http && !event.error)
            {
                ICY_ERROR(event::post(&system, type, std::move(event)));
                ovl->type = event_type::network_recv;
                if (!event.error) event.error = ovl->bytes.resize(m_config.buffer);
                if (!event.error) conn.addr = std::move(event.address);
                //  fall through
            }
            else
            {
                update = true;
                ICY_ERROR(event::post(&system, type, std::move(event)));
                break;
            }*/
        }
        case event_type::network_recv:
        {
            if (m_http)
            {
                const auto find_str = [ovl](const string_view str) -> uint32_t
                {
                    const auto input_beg = ovl->bytes.data() + ovl->http_header;
                    const auto input_end = ovl->bytes.data() + ovl->offset;
                    const auto find = std::search(input_beg, input_end, str.bytes().begin(), str.bytes().end());
                    return find != input_end ? uint32_t(find - ovl->bytes.data()) : 0;
                };
                if (ovl->http_header == 0 && !event.error)
                {
                    if (ovl->http_header = find_str(http_str_header))
                    {
                        if (ovl->http_header >= http_max_header || ovl->http_header >= m_config.buffer)
                            event.error = make_stdlib_error(std::errc::value_too_large);
                    }
                }
                if (ovl->http_length == 0 && !event.error)
                {
                    if (ovl->http_header)
                    {
                        if (const auto find_endl = find_str(http_str_endl01))
                        {
                            const auto length_beg = ovl->bytes.data() + ovl->http_header + http_str_header.bytes().size();
                            const auto length_beg_ptr = reinterpret_cast<const char*>(length_beg);
                            const auto length_end_ptr = reinterpret_cast<const char*>(ovl->bytes.data() + find_endl);
                            event.error = to_value(string_view(length_beg_ptr, length_end_ptr), ovl->http_length);
                            if (ovl->http_header + ovl->http_length + 4 > m_config.buffer)
                                event.error = make_stdlib_error(std::errc::no_buffer_space);

                        }
                    }
                }
                if (ovl->http_offset == 0 && !event.error)
                    ovl->http_offset = find_str(http_str_endl02);
                
                if (ovl->http_offset > 0 && !event.error)
                {
                    if (ovl->http_length + ovl->http_offset + 4 == ovl->offset)
                    {
                        auto http_str = string_view(reinterpret_cast<const char*>(ovl->bytes.data()), ovl->offset);
                        
                        if (http_str.bytes().size() > 4 && string_view(http_str.begin(), http_str.begin() + 4) == "HTTP"_s)
                        {
                            auto response = make_unique<http_response>();
                            if (!response)
                                event.error = make_stdlib_error(std::errc::not_enough_memory);
                            if (response)
                                event.error = to_value(http_str, *response);
                            if (!event.error)
                                event.http.response = std::move(response);
                        }
                        else
                        {
                            auto request = make_unique<http_request>();
                            if (!request)
                                event.error = make_stdlib_error(std::errc::not_enough_memory);
                            if (request)
                                event.error = to_value(http_str, *request);
                            if (!event.error)
                                event.http.request = std::move(request);
                        }

                        if (!event.error)
                        {
                            update = true;
                            event.address = std::move(conn.addr);
                        }
                    }
                }
            }            
            if (!event.error)
            {
                if (update || (event.error = conn.recv()))
                {
                    event.bytes = std::move(ovl->bytes);
                    event.bytes.resize(ovl->offset);
                    ICY_ERROR(event::post(&system, type, std::move(event)));
                }
            }
            break;
        }
        case event_type::network_send:
        {
            if (update || (event.error = conn.send()))
            {
                event.bytes = std::move(ovl->bytes);
                event.bytes.resize(ovl->offset);
                ICY_ERROR(event::post(&system, type, std::move(event)));
            }
            if (update && m_http)
            {
                event.http.response = std::move(conn.http.response);
                event.http.request = std::move(conn.http.request);
            }
            break;
        }
        default:
            return make_stdlib_error(std::errc::invalid_argument);
        }
    }

    if (event.error || is_disconnected)
    {
        const auto new_error = conn.shutdown(m_config.timeout);
        if (!event.error)
            event.error = new_error;
        ICY_ERROR(event::post(&system, event_type::network_disconnect, std::move(event)));
        if (m_http && m_config.port)
            ICY_ERROR(conn.accept(*this));
        return error_type();
    }
    if (update)
    {
        *ovl = {};
        auto next_type = event_type::none;

        if (m_http && m_config.port)
        {
            //if (type == event_type::network_send)
            //{
            //    //event.error = conn.disc();
            //}
            //else 
            if (type == event_type::network_disconnect)
            {
                ICY_ERROR(conn.accept(*this));
            }
        }
        if (!conn.ovl_recv.type)
        {
            network_tcp_overlapped next;
            if (conn.rqueue.pop(next))
            {
                next_type = next.type;
                conn.ovl_recv = std::move(next);
                event.error = conn.recv();
            }
        }
        if (!conn.ovl_send.type)
        {
            network_tcp_overlapped next;
            if (conn.squeue.pop(next))
            {
                next_type = next.type;
                conn.ovl_send = std::move(next);
                if (next.type == event_type::network_send)
                    event.error = conn.send();
                else
                    event.error = conn.disc();
            }
        }
        if (event.error)
        {
            ICY_ERROR(event::post(&system, next_type, std::move(event)));
            event.error = conn.shutdown(m_config.timeout);
            ICY_ERROR(event::post(&system, event_type::network_disconnect, std::move(event)));
            if (m_http && m_config.port)
                ICY_ERROR(conn.accept(*this));
        }
    }

    return error_type();
}
error_type detail::network_system_data::loop_udp(event_system& system) noexcept
{
    const auto recv_from = [this](detail::network_udp_overlapped& udp, array<uint8_t>&& bytes)
    {
        udp.bytes = std::move(bytes);
        udp.addr_len = sizeof(udp.addr_buf);

        WSABUF buf = { ULONG(udp.bytes.size()), reinterpret_cast<char*>(udp.bytes.data()) };
        auto rbytes = 0ul;
        auto flags = 0ul;
        if (network_func_recv_from(m_socket, &buf, 1, &rbytes, &flags, reinterpret_cast<sockaddr*>(udp.addr_buf),
            &udp.addr_len, &udp.overlapped, nullptr) == SOCKET_ERROR)
        {
            const auto error = last_system_error();
            if (error.code != make_system_error_code(ERROR_IO_PENDING))
                return error;
        }
        return error_type();
    };
    const auto send_to = [this](array<uint8_t>&& bytes, const sockaddr* const addr_buf, const int addr_len)
    {
        if (m_udp_send.ovl.type == event_type::network_send)
        {
            network_udp_overlapped ovl;
            memcpy(ovl.addr_buf, addr_buf, ovl.addr_len = addr_len);
            ovl.bytes = std::move(bytes);
            ovl.type = event_type::network_send;
            ICY_ERROR(m_udp_send.queue.push(std::move(ovl)));
        }
        else
        {
            m_udp_send.ovl.type = event_type::network_send;
            m_udp_send.ovl.bytes = std::move(bytes);

            WSABUF buf = { ULONG(m_udp_send.ovl.bytes.size()), reinterpret_cast<char*>(m_udp_send.ovl.bytes.data()) };
            auto sbytes = 0ul;
            if (network_func_send_to(m_socket, &buf, 1, &sbytes, 0, addr_buf, addr_len, &m_udp_send.ovl.overlapped, nullptr) == SOCKET_ERROR)
            {
                const auto error = last_system_error();
                if (error.code != make_system_error_code(ERROR_IO_PENDING))
                    return error;
            }
        }
        return error_type();
    };

    network_command cmd;
    while (m_cmds.pop(cmd))
    {
        network_event event;
        switch (cmd.type)
        {
        case event_type::network_send:
        {
            event.error = send_to(std::move(cmd.bytes), cmd.address.data(), cmd.address.size());
            break;
        }
        default:
            return make_stdlib_error(std::errc::function_not_supported);
        };
        if (event.error)
        {
            ICY_ERROR(event::post(&system, cmd.type, std::move(event)));
        }
    }

    OVERLAPPED_ENTRY entry = {};
    auto count = 0ul;
    SetLastError(ERROR_SUCCESS);
    GetQueuedCompletionStatus(m_iocp, &entry.dwNumberOfBytesTransferred,
        &entry.lpCompletionKey, &entry.lpOverlapped, INFINITE);
    auto ovl = reinterpret_cast<network_udp_overlapped*>(entry.lpOverlapped);
    if (!ovl)
        return last_system_error();

    network_event event;
    event.bytes = std::move(ovl->bytes);
    event.bytes.resize(entry.dwNumberOfBytesTransferred);

    if (ovl->type == event_type::network_recv)
    {
        event.error = network_address_query::create(event.address, ovl->addr_len, *reinterpret_cast<const sockaddr*>(ovl->addr_buf));
        ICY_ERROR(event::post(&system, event_type::network_recv, std::move(event)));
        
        array<uint8_t> bytes;
        ICY_ERROR(bytes.resize(m_config.buffer));
        ICY_ERROR(recv_from(*ovl, std::move(bytes)));
    }
    else if (ovl->type == event_type::network_send)
    {
        ICY_ERROR(event::post(&system, event_type::network_send, std::move(event)));

        ovl->type = event_type::none;
        network_udp_overlapped new_ovl;
        if (m_udp_send.queue.pop(new_ovl))
            ICY_ERROR(send_to(std::move(new_ovl.bytes), reinterpret_cast<const sockaddr*>(new_ovl.addr_buf), new_ovl.addr_len));
    }
    return error_type();
}
error_type detail::network_system_data::join(const network_address& addr, const bool join) noexcept
{
    if (addr.size() == sizeof(sockaddr_in6))
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    else if (addr.size() == sizeof(sockaddr_in))
    {
        struct ip_mreq mreq = {};
        mreq.imr_multiaddr = reinterpret_cast<sockaddr_in*>(addr.data())->sin_addr;
        if (network_func_setsockopt(m_socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
            return last_system_error();
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}