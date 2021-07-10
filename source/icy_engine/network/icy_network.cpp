#include "icy_network_system.hpp"
#include "icy_network_address.hpp"
#include <icy_engine/core/icy_thread.hpp>

using namespace icy;

network_system_tcp_client::~network_system_tcp_client() noexcept
{
    if (m_thread) m_thread->wait();
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_tcp_client::send(const const_array_view<uint8_t> buffer) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(copy(buffer, bytes));
    return m_data->post(0, event_type::network_send, std::move(bytes));
}
error_type network_system_tcp_client::recv(const size_t capacity) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(bytes.resize(capacity));
    return m_data->post(0, event_type::network_recv, std::move(bytes));
}
error_type network_system_tcp_client::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        ICY_ERROR(m_data->loop_tcp(*this));
    }
    return error_type();
}
error_type network_system_tcp_client::exec_once() noexcept
{
    return m_data->loop_tcp(*this);
}
error_type network_system_tcp_client::signal(const event_data* event) noexcept
{
    return m_data->cancel();
}

network_system_udp_client::~network_system_udp_client() noexcept
{
    if (m_thread) m_thread->wait();
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_udp_client::join(const network_address& multicast) noexcept
{
    if (!m_data)
        return make_stdlib_error(std::errc::invalid_argument);
    return m_data->join(multicast, true);
}
error_type network_system_udp_client::leave(const network_address& multicast) noexcept
{
    if (!m_data)
        return make_stdlib_error(std::errc::invalid_argument);
    return m_data->join(multicast, false);
}
error_type network_system_udp_client::send(const network_address& address, const const_array_view<uint8_t> buffer) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(copy(buffer, bytes));
    return m_data->post(0, event_type::network_send, std::move(bytes), &address);
}
error_type network_system_udp_client::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        ICY_ERROR(m_data->loop_udp(*this));
    }
    return error_type();
}
error_type network_system_udp_client::signal(const event_data* event) noexcept
{
    return m_data->cancel();
}

network_system_http_client::~network_system_http_client() noexcept
{
    if (m_thread) m_thread->wait();
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_http_client::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        if (const auto error = m_data->loop_tcp(*this))
            ICY_ERROR(error);
    }
    return error_type();
}
error_type network_system_http_client::signal(const event_data* event) noexcept
{
    return m_data->cancel();
}
error_type network_system_http_client::send(const http_response& response) noexcept
{
    unique_ptr<http_response> new_response;
    ICY_ERROR(make_unique(response, new_response));
    return m_data->post(0, std::move(new_response));
}
error_type network_system_http_client::send(const http_request& request) noexcept
{
    unique_ptr<http_request> new_request;
    ICY_ERROR(make_unique(request, new_request));
    return m_data->post(0, std::move(new_request));
}
error_type network_system_http_client::recv() noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(bytes.resize(m_data->config().buffer));
    return m_data->post(0, event_type::network_recv, std::move(bytes));
}

network_system_tcp_server::~network_system_tcp_server() noexcept
{
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_tcp_server::open(const network_tcp_connection conn) noexcept
{
    return m_data->post(conn.index, event_type::network_connect, array<uint8_t>());
}
error_type network_system_tcp_server::close(const network_tcp_connection conn) noexcept
{
    array<uint8_t> bytes;
    return m_data->post(conn.index, event_type::network_disconnect, array<uint8_t>());
}
error_type network_system_tcp_server::send(const network_tcp_connection conn, const const_array_view<uint8_t> buffer) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(copy(buffer, bytes));
    return m_data->post(conn.index, event_type::network_send, std::move(bytes));
}
error_type network_system_tcp_server::recv(const network_tcp_connection conn, const size_t capacity) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(bytes.resize(capacity));
    return m_data->post(conn.index, event_type::network_recv, std::move(bytes));
}
error_type network_system_tcp_server::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        ICY_ERROR(m_data->loop_tcp(*this));
    }
    return error_type();
}
error_type network_system_tcp_server::signal(const event_data*) noexcept
{
    return m_data->cancel();
}

network_system_udp_server::~network_system_udp_server() noexcept
{
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_udp_server::send(const network_address& addr, const const_array_view<uint8_t> buffer) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(copy(buffer, bytes));
    return m_data->post(0u, event_type::network_send, std::move(bytes), &addr);
}
error_type network_system_udp_server::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        ICY_ERROR(m_data->loop_udp(*this));
    }
    return error_type();
}
error_type network_system_udp_server::signal(const event_data* event) noexcept
{
    return m_data->cancel();
}

network_system_http_server::~network_system_http_server() noexcept
{
    filter(0);
    detail::network_system_data::destroy(m_data);
}
error_type network_system_http_server::send(const network_tcp_connection conn, const http_response& response) noexcept
{
    unique_ptr<http_response> new_response;
    ICY_ERROR(make_unique(response, new_response));
    return m_data->post(conn.index, std::move(new_response));
}
error_type network_system_http_server::send(const network_tcp_connection conn, const http_request& request) noexcept
{
    unique_ptr<http_request> new_request;
    ICY_ERROR(make_unique(request, new_request));
    return m_data->post(conn.index, std::move(new_request));
}
error_type network_system_http_server::recv(const network_tcp_connection conn) noexcept
{
    array<uint8_t> bytes;
    ICY_ERROR(bytes.resize(m_data->config().buffer));
    return m_data->post(conn.index, event_type::network_recv, std::move(bytes));
}
error_type network_system_http_server::exec() noexcept
{
    while (*this)
    {
        if (auto event = pop())
        {

        }
        ICY_ERROR(m_data->loop_tcp(*this));
    }
    return error_type();
}
error_type network_system_http_server::signal(const event_data*) noexcept
{
    return m_data->cancel();
}

error_type icy::create_network_tcp_client(shared_ptr<network_system_tcp_client>& system, const network_address& address, const const_array_view<uint8_t> bytes, const duration_type timeout) noexcept
{
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, false));
    new_system->filter(event_type::system_internal);
    ICY_ERROR(new_system->m_data->connect(address, bytes, timeout));
    shared_ptr<event_thread> thread;
    ICY_ERROR(make_shared(thread, event_thread()));
    thread->system = new_system.get();
    new_system->m_thread = std::move(thread);
    system = std::move(new_system);
    return error_type();
}
error_type icy::create_network_tcp_client(shared_ptr<network_system_tcp_client>& system, const network_address& address, const http_request& request, const duration_type timeout) noexcept
{
    string str;
    ICY_ERROR(to_string(request, str));
    array<uint8_t> bytes;
    ICY_ERROR(bytes.append(str.ubytes()));
    ICY_ERROR(bytes.append(request.body));

    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, true));
    new_system->filter(event_type::system_internal);
    ICY_ERROR(new_system->m_data->connect(address, bytes, timeout));
    shared_ptr<event_thread> thread;
    ICY_ERROR(make_shared(thread, event_thread()));
    thread->system = new_system.get();
    new_system->m_thread = std::move(thread);
    system = std::move(new_system);
    return error_type();
}
error_type icy::create_network_udp_client(shared_ptr<network_system_udp_client>& system, const network_server_config& args) noexcept
{
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, false));
    ICY_ERROR(new_system->m_data->launch(args, detail::network_socket::type::udp));
    shared_ptr<event_thread> thread;
    ICY_ERROR(make_shared(thread, event_thread()));
    thread->system = new_system.get();
    new_system->m_thread = std::move(thread);
    new_system->filter(event_type::system_internal);
    system = std::move(new_system);
    return error_type();
}
error_type icy::create_network_http_client(shared_ptr<network_system_http_client>& system, const network_address& address, const http_request& request, const duration_type timeout, const size_t buffer) noexcept
{
    string str;
    ICY_ERROR(to_string(request, str));
    array<uint8_t> bytes;
    ICY_ERROR(bytes.append(str.ubytes()));
    ICY_ERROR(bytes.append(request.body));

    array<uint8_t> recv_buffer;
    ICY_ERROR(recv_buffer.resize(buffer));
    
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, true));
    new_system->filter(event_type::system_internal);
    ICY_ERROR(new_system->m_data->connect(address, bytes, timeout, std::move(recv_buffer)));
    shared_ptr<event_thread> thread;
    ICY_ERROR(make_shared(thread, event_thread()));
    thread->system = new_system.get();
    new_system->m_thread = std::move(thread);
    system = std::move(new_system);
    return error_type();
}
error_type icy::create_network_tcp_server(shared_ptr<network_system_tcp_server>& system, const network_server_config& args) noexcept
{
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, false));
    ICY_ERROR(new_system->m_data->launch(args, detail::network_socket::type::tcp));
    system = std::move(new_system);
    system->filter(event_type::system_internal);
    return error_type();
}
error_type icy::create_network_udp_server(shared_ptr<network_system_udp_server>& system, const network_server_config& args) noexcept
{
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, false));
    ICY_ERROR(new_system->m_data->launch(args, detail::network_socket::type::udp));
    system = std::move(new_system);
    system->filter(event_type::system_internal);
    return error_type();
}
error_type icy::create_network_http_server(shared_ptr<network_system_http_server>& system, const network_server_config& args) noexcept
{
    std::remove_reference_t<decltype(system)> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(detail::network_system_data::create(new_system->m_data, true));
    ICY_ERROR(new_system->m_data->launch(args, detail::network_socket::type::tcp));
    system = std::move(new_system);
    system->filter(event_type::system_internal);
    return error_type();
}

class network_udp_socket::data_type
{
public:
    ~data_type() noexcept
    {
        socket = detail::network_socket();
        if (wsa_cleanup) wsa_cleanup();
    }
    error_type recv() noexcept;

    std::atomic<uint32_t> ref = 1;
    library lib = "ws2_32"_lib;
    decltype(&WSACleanup) wsa_cleanup = nullptr;
    decltype(&WSASendTo) func_send = nullptr;
    decltype(&WSARecvFrom) func_recv = nullptr;
    detail::network_socket socket;
    detail::network_udp_overlapped udp;
    mpsc_queue<detail::network_udp_overlapped> rqueue;
};

error_type network_udp_socket::data_type::recv() noexcept
{
    ICY_ASSERT(udp.type == event_type::network_recv || udp.bytes.empty(), "INVALID STATE");
    udp.addr_len = sizeof(udp.addr_buf);
    auto func = [](DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
    {
        const auto ovl = reinterpret_cast<detail::network_udp_overlapped*>(lpOverlapped);
        const auto self = static_cast<network_udp_socket*>(ovl->user);

        network_address address;
        auto udp = std::move(self->data->udp);
        self->data->udp.type = event_type::none;
        if (self->data->rqueue.pop(self->data->udp))
            self->data->recv();
        
        detail::network_address_query::create(address, udp.addr_len, reinterpret_cast<sockaddr&>(udp.addr_buf));
        self->recv(address, udp.bytes);
    };

    WSABUF buf = { ULONG(udp.bytes.size()), reinterpret_cast<char*>(const_cast<uint8_t*>(udp.bytes.data())) };
    auto bytes = 0ul;
    if (func_recv(socket, &buf, 1, &bytes, 0, reinterpret_cast<sockaddr*>(udp.addr_buf),
        &udp.addr_len, &udp.overlapped, func) == SOCKET_ERROR)
        return last_system_error();

    return error_type();
}

error_type network_udp_socket::initialize(const uint16_t port) noexcept
{
    auto lib = "ws2_32"_lib;
    ICY_ERROR(lib.initialize());
    ICY_ERROR(detail::network_func_init(lib));

    auto wsa_cleanup = ICY_FIND_FUNC(lib, WSACleanup);
    const auto wsa_startup = ICY_FIND_FUNC(lib, WSAStartup);
    const auto func_send = ICY_FIND_FUNC(lib, WSASendTo);
    const auto func_recv = ICY_FIND_FUNC(lib, WSARecvFrom);
    const auto func_bind = ICY_FIND_FUNC(lib, bind);
    const auto func_htons = ICY_FIND_FUNC(lib, htons);
    if (!wsa_startup || !wsa_cleanup || !func_send || !func_recv || !func_bind || !func_htons)
        return make_stdlib_error(std::errc::function_not_supported);

    WSADATA wsaData;
    if (wsa_startup(WINSOCK_VERSION, &wsaData) == SOCKET_ERROR)
        return last_system_error();
    ICY_SCOPE_EXIT{ if (wsa_cleanup) wsa_cleanup(); };
    
    detail::network_socket socket;
    ICY_ERROR(socket.initialize(detail::network_socket::type::udp, network_address_type::ip_v4));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = func_htons(port);

    if (func_bind(socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        return last_system_error();

    const auto new_data = allocator_type::allocate<data_type>(1);
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);
    allocator_type::construct(new_data);
    
    new_data->lib = std::move(lib);
    new_data->wsa_cleanup = wsa_cleanup;
    new_data->func_send = func_send;
    new_data->func_recv = func_recv;
    new_data->socket = std::move(socket);
    wsa_cleanup = nullptr;

    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
    data = new_data;
    return error_type();
}
error_type network_udp_socket::send(const network_address& addr, const const_array_view<uint8_t> buffer) noexcept
{
    if (!data || !addr || buffer.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    WSABUF buf = { ULONG(buffer.size()), reinterpret_cast<char*>(const_cast<uint8_t*>(buffer.data())) };
    auto bytes = 0ul;
    if (data->func_send(data->socket, &buf, 1, &bytes, 0, addr.data(), addr.size(), nullptr, nullptr) == SOCKET_ERROR)
        return last_system_error();

    return error_type();
}
error_type network_udp_socket::recv(const size_t size) noexcept
{
    if (!data || !size)
        return make_stdlib_error(std::errc::invalid_argument);
        
    if (data->udp.type == event_type::network_recv)
    {
        detail::network_udp_overlapped ovl;
        ovl.type = event_type::network_recv;
        ovl.user = this;
        ICY_ERROR(ovl.bytes.resize(size));
        ICY_ERROR(data->rqueue.push(std::move(ovl)));
    }
    else
    {
        data->udp.type = event_type::network_recv;
        data->udp.user = this;
        ICY_ERROR(data->udp.bytes.resize(size));
        ICY_ERROR(data->recv());
    }
    return error_type();
}

network_udp_socket::network_udp_socket(const network_udp_socket& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
network_udp_socket::~network_udp_socket() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}


/*
class network_tcp_request
{
public:
    //static error_type create(const network_request_type type, size_t capacity, network_tcp_connection& conn, network_tcp_request*& request) noexcept;
    network_tcp_request(const network_request_type type, const size_t capacity, network_tcp_connection& connection) noexcept :
        type(type), capacity(capacity), connection(connection), time(connection.m_time)
    {

    }
error_type recv(const size_t max) noexcept;
error_type send() noexcept;
error_type recv_http(bool& done) noexcept;
public:
    void* unused = nullptr;
    OVERLAPPED overlapped{};
    const event_type type;
    const size_t capacity;
    network_tcp_connection& connection;
    size_t size = 0;
    struct { size_t head; size_t body; } http = {};
    clock_type::time_point time;
    uint8_t buffer[1];
};
struct network_udp_request
{
public:
    error_type recv(const SOCKET sock) noexcept;
    error_type send(const SOCKET sock) noexcept;
public:
    OVERLAPPED overlapped{};
    int address_size = 0;
    size_t size = 0;
};

void network_tcp_connection::reset() noexcept
{
    if (m_timer)
    {
        union
        {
            LARGE_INTEGER integer;
            FILETIME time;
        };
        integer.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(m_timeout).count() / 100;
        SetThreadpoolTimer(m_timer, &time, 0, 0);
    }
}
void network_tcp_connection::_close() noexcept
{
    if (m_timer)
    {
        SetThreadpoolTimer(m_timer, nullptr, 0, 0);
        WaitForThreadpoolTimerCallbacks(m_timer, TRUE);
        CloseThreadpoolTimer(m_timer);
        m_timer = nullptr;
    }
    //CancelIoEx(reinterpret_cast<HANDLE>(static_cast<SOCKET>(m_socket)), nullptr);
    m_socket.shutdown();
    m_time = {};
}
error_type network_tcp_connection::make_timer() noexcept
{
    const auto callback = [](PTP_CALLBACK_INSTANCE, const PVOID ptr, PTP_TIMER)
    {
        const auto conn = static_cast<network_tcp_connection*>(ptr);
        PostQueuedCompletionStatus(conn->m_iocp, 0, reinterpret_cast<ULONG_PTR>(ptr), nullptr);
    };
    if (!m_timer)
    {
        m_timer = CreateThreadpoolTimer(callback, this, nullptr);
        if (!m_timer)
            return last_system_error();
    }
    SetThreadpoolTimer(m_timer, nullptr, 0, 0);
    return error_type();
}
network_tcp_connection::~network_tcp_connection() noexcept
{
    _close();
    for (auto&& request : m_requests)
        icy::realloc(request, 0);
}

error_type network_tcp_request::create(const network_request_type type, const size_t capacity, network_tcp_connection& conn, network_tcp_request*& request) noexcept
{
    if (request)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(conn.m_requests.reserve(conn.m_requests.size() + 1));
    auto memory = icy::realloc(nullptr, sizeof(network_tcp_request) + capacity);
    if (!memory)
        return make_stdlib_error(std::errc::not_enough_memory);

    request = new (memory) network_tcp_request(type, capacity, conn);
    conn.m_requests.push_back(request);
    return error_type();
}
error_type network_tcp_request::recv(const size_t max) noexcept
{
    if (!network_func_recv)
        return make_stdlib_error(std::errc::function_not_supported);

    connection.reset();
    WSABUF buf = { ULONG(max - size), reinterpret_cast<CHAR*>(buffer + size) };
    auto bytes = 0ul;
    auto flags = 0ul;
    if (network_func_recv(connection.m_socket, &buf, 1, &bytes, &flags, &overlapped, nullptr) == SOCKET_ERROR)
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    return error_type();
}
error_type network_tcp_request::send() noexcept
{
    if (!network_func_send)
        return make_stdlib_error(std::errc::function_not_supported);

    connection.reset();
    WSABUF buf = { ULONG(capacity - size), reinterpret_cast<CHAR*>(buffer + size) };
    auto bytes = 0ul;
    if (network_func_send(connection.m_socket, &buf, 1, &bytes, 0, &overlapped, nullptr) == SOCKET_ERROR)
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    return error_type();
}
error_type network_tcp_request::recv_http(bool& done) noexcept
{
    connection.reset();
    if (http.body)
    {
        const auto max_size = http.body + http.head;
        if (size > max_size)
            return make_stdlib_error(std::errc::no_buffer_space);
        else if (size < max_size)
            return recv(max_size - size);
        else
            done = true;
    }
    else if (size == http.head)
    {
        done = true;
    }
    else
    {
        auto parsed = false;
        for (auto k = http.head; k < size; ++k)
        {
            const auto chr = buffer[k];
            if (chr == '\r')
            {
                if (k > size - 4)
                {
                    http.head = k;
                    break;
                }
                if (buffer[k + 1] == '\n' && buffer[k + 2] == '\r' && buffer[k + 3] == '\n')
                {
                    http.head = k + 4;
                    buffer[k] = '\0';
                    const auto cstr = "Content-Length: ";
                    auto find = strstr(reinterpret_cast<const char*>(buffer), cstr);
                    buffer[k] = '\r';
                    if (find)
                    {
                        find += strlen(cstr);
                        auto end = find;
                        for (; *end >= '0' && *end <= '9'; ++end)
                            ;
                        if (!find || _snscanf_s(find, size_t(end - find), "%zu", &http.body) != 1)
                            return make_stdlib_error(std::errc::illegal_byte_sequence);
                    }
                    parsed = true;
                    break;
                }
            }
            http.head = k;
        }
        if (parsed)
            return recv_http(done);
        else
            return recv(capacity);
    }
    return error_type();
}

error_type network_udp_request_ex::recv(const SOCKET sock) noexcept
{
    if (!network_func_recv_from)
        return make_stdlib_error(std::errc::function_not_supported);

    WSABUF buf = { ULONG(bytes.size()), reinterpret_cast<CHAR*>(bytes.data()) };
    auto recv_bytes = 0ul;
    auto flags = 0ul;
    if (network_func_recv_from(sock, &buf, 1, &recv_bytes, &flags, address.data(), &address_size, &overlapped, nullptr) == SOCKET_ERROR)
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    else
    {
        size = recv_bytes;
    }
    return error_type();
}
error_type network_udp_request_ex::send(const SOCKET sock) noexcept
{
    if (!network_func_send_to)
        return make_stdlib_error(std::errc::function_not_supported);

    WSABUF buf = { ULONG(bytes.size()), reinterpret_cast<CHAR*>(bytes.data()) };
    auto send_bytes = 0ul;
    if (network_func_send_to(sock, &buf, 1, &send_bytes, 0, address.data(), address.size(), &overlapped, nullptr) == SOCKET_ERROR)
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    else
    {
        size = send_bytes;
    }
    return error_type();
}

void network_system_base::shutdown() noexcept
{
    if (m_iocp)
    {
        CloseHandle(m_iocp);
        m_iocp = nullptr;
        m_socket.shutdown();
        if (network_func_cleanup)
            network_func_cleanup();
    }
}
error_type network_system_base::launch(const uint16_t port, const network_socket_type sock_type, const network_address_type addr_type, const size_t max_queue) noexcept
{
    if (!network_func_bind || !network_func_htons || !network_func_listen)
        return make_stdlib_error(std::errc::function_not_supported);

    network_socket socket;
    ICY_ERROR(socket.initialize(sock_type, addr_type));
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE&>(socket), m_iocp, 0, 0))
        return last_system_error();

    if (addr_type == network_address_type::ip_v6)
    {
        auto server_addr_v6 = sockaddr_in6{ };
        server_addr_v6.sin6_family = AF_INET6;
        server_addr_v6.sin6_port = network_func_htons(port);
        if (network_func_bind(socket, reinterpret_cast<sockaddr*>(&server_addr_v6), sizeof(server_addr_v6)) == SOCKET_ERROR)
            return last_system_error();
    }
    else //if (addr_type == network_address_type::ip_v4)
    {
    /*    int fd = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        u_int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
        auto null = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&null, sizeof(null));
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&null, sizeof(null));
        timeval tv = {};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

        reinterpret_cast<size_t&>(socket) = fd;

        ip_mreq mreq = {};
        mreq.imr_multiaddr.S_un.S_addr = inet_addr("230.4.4.1");
        if (network_func_setsockopt(socket, IPPROTO_IP, true ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
            return last_system_error();


        auto server_addr_v4 = sockaddr_in{ };
        server_addr_v4.sin_family = AF_INET;
        server_addr_v4.sin_port = network_func_htons(port);
        if (network_func_bind(socket, reinterpret_cast<sockaddr*>(&server_addr_v4), sizeof(server_addr_v4)) == SOCKET_ERROR)
            return last_system_error();
    }

    if (sock_type == network_socket_type::TCP && max_queue)
    {
        if (network_func_listen(socket, int(max_queue)) == SOCKET_ERROR)
            return last_system_error();
    }
    m_socket = std::move(socket);
    m_addr_type = addr_type;
    return error_type();
}
error_type network_system_base::stop(const uint32_t code) noexcept
{
    ICY_ASSERT(code, "INVALID NETWORK CODE");
    if (!PostQueuedCompletionStatus(m_iocp, code, 0, nullptr))
        return last_system_error();
    return error_type();
}

error_type network_system_tcp::connect(network_tcp_connection& conn, const network_address& address, const const_array_view<uint8_t> buffer) noexcept
{
    if (!network_func_bind || !network_func_listen || !network_func_connect)
        return make_stdlib_error(std::errc::function_not_supported);

    ICY_LOCK_GUARD_WRITE(conn.m_lock);
    conn.m_time = {};

    if (!conn.m_socket)
    {
        ICY_ERROR(conn.m_socket.initialize(address.sock_type(), address.addr_type()));

        const auto file = reinterpret_cast<HANDLE>(SOCKET(conn.m_socket));
        conn.m_iocp = CreateIoCompletionPort(file, m_iocp, reinterpret_cast<ULONG_PTR>(&conn), 0);
        if (conn.m_iocp != m_iocp)
            return last_system_error();
    }

    ICY_ERROR(conn.make_timer());
    network_tcp_request* ref = nullptr;
    ICY_ERROR(network_tcp_request::create(network_request_type::connect, buffer.size(), conn, ref));
    if (address.addr_type() == network_address_type::ip_v6)
    {
        sockaddr_in6 addr = {};
        addr.sin6_family = AF_INET6;
        if (network_func_bind(conn.m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            return last_system_error();
    }
    else
    {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        if (network_func_bind(conn.m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            return last_system_error();
    }
    auto bytes = 0ul;
    memcpy(ref->buffer, buffer.data(), buffer.size());
    if (!network_func_connect(conn.m_socket, address.data(), address.size(),
        ref->capacity ? ref->buffer : nullptr, uint32_t(ref->capacity), &bytes, &ref->overlapped))
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    conn.reset();
    return error_type();
}
error_type network_system_tcp::accept(network_tcp_connection& conn) noexcept
{
    if (!network_func_accept)
        return make_stdlib_error(std::errc::function_not_supported);

    ICY_LOCK_GUARD_WRITE(conn.m_lock);

    if (!conn.m_socket)
    {
        conn.m_time = {};
        ICY_ERROR(conn.m_socket.initialize(network_socket_type::TCP, m_addr_type));

        const auto file = reinterpret_cast<HANDLE>(SOCKET(conn.m_socket));
        conn.m_iocp = CreateIoCompletionPort(file, m_iocp, reinterpret_cast<ULONG_PTR>(&conn), 0);
        if (conn.m_iocp != m_iocp)
            return last_system_error();
    }

    network_tcp_request* ref = nullptr;
    const auto size = 2 * ((m_addr_type == network_address_type::ip_v6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) + 16);
    ICY_ERROR(network_tcp_request::create(network_request_type::accept, size, conn, ref));
    auto bytes = 0ul;
    if (!network_func_accept(m_socket, conn.m_socket, ref->buffer, 0,
        uint32_t(size / 2), uint32_t(size / 2), &bytes, &ref->overlapped))
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    return error_type();
}
error_type network_system_tcp::send(network_tcp_connection& conn, const const_array_view<uint8_t> buffer) noexcept
{
    ICY_LOCK_GUARD_WRITE(conn.m_lock);
    network_tcp_request* ref = nullptr;
    ICY_ERROR(network_tcp_request::create(network_request_type::send, buffer.size(), conn, ref));
    memcpy(ref->buffer, buffer.data(), buffer.size());
    return ref->send();
}
error_type network_system_tcp::recv(network_tcp_connection& conn, const size_t capacity) noexcept
{
    ICY_LOCK_GUARD_WRITE(conn.m_lock);
    network_tcp_request* ref = nullptr;
    ICY_ERROR(network_tcp_request::create(network_request_type::recv, capacity, conn, ref));
    return ref->recv(ref->capacity);
}
error_type network_system_tcp::disconnect(network_tcp_connection& conn) noexcept
{
    if (!network_func_disconnect)
        return make_stdlib_error(std::errc::function_not_supported);

    ICY_LOCK_GUARD_WRITE(conn.m_lock);
    network_tcp_request* ref = nullptr;
    ICY_ERROR(network_tcp_request::create(network_request_type::disconnect, 0, conn, ref));
    if (!network_func_disconnect(conn.m_socket, &ref->overlapped, TF_REUSE_SOCKET, 0))
    {
        const auto error = WSAGetLastError();
        if (error != WSA_IO_PENDING)
            return make_system_error(make_system_error_code(uint32_t(error)));
    }
    return error_type();
}
error_type network_system_tcp::loop(network_tcp_reply& reply, uint32_t& code, const network_duration timeout) noexcept
{
    if (!network_func_setsockopt || !network_func_sockaddr)
        return make_stdlib_error(std::errc::function_not_supported);

    auto entry = OVERLAPPED_ENTRY{};
    auto count = 0ul;
    const auto offset = offsetof(network_tcp_request, overlapped);
    const auto success = GetQueuedCompletionStatusEx(m_iocp, &entry, 1, &count, network_timeout(timeout), TRUE);
    if (!success)
        return last_system_error();
    if (!count)
        return error_type();

    const auto request = reinterpret_cast<network_tcp_request*>(reinterpret_cast<char*>(entry.lpOverlapped) - offset);
    auto conn = reply.conn = reinterpret_cast<network_tcp_connection*>(entry.lpCompletionKey);

    if (!entry.lpOverlapped)
    {
        if (conn)
        {
            reply.type = network_request_type::timeout;
            ICY_LOCK_GUARD_WRITE(reply.conn->m_lock);
        }
        else // if (!entry.dwNumberOfBytesTransferred)
        {
            code = entry.dwNumberOfBytesTransferred;
        }
        return error_type();
    }
    else if (!conn)
    {
        conn = reply.conn = &request->connection;
    }
    ICY_LOCK_GUARD_WRITE(conn->m_lock);

    auto ptr = std::find(conn->m_requests.begin(), conn->m_requests.end(), request);
    if (ptr == conn->m_requests.end())
    {
        reply = {};
        return error_type();
    }
    ICY_SCOPE_EXIT
    {
        auto it = std::find(conn->m_requests.begin(), conn->m_requests.end(), request);
        if (it != conn->m_requests.end())
        {
            for (auto jt = it + 1; jt != conn->m_requests.end(); ++jt, ++it)
                *it = *jt;
            conn->m_requests.pop_back();
        }
        icy::realloc(request, 0);
    };

    reply.type = request->type;
    request->size += entry.dwNumberOfBytesTransferred;
    ICY_ERROR(reply.bytes.assign(request->buffer, request->buffer + request->size));
    conn->reset();

    if (request->type == network_request_type::accept)
    {
        const auto size = 2 * ((m_addr_type == network_address_type::ip_v6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) + 16);
        if (network_func_setsockopt(conn->m_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&m_socket), sizeof(SOCKET)) == SOCKET_ERROR)
        {
            reply.error = last_system_error();
        }
        else
        {
            sockaddr* addr_loc = nullptr;
            sockaddr* addr_rem = nullptr;
            auto size_loc = 0;
            auto size_rem = 0;
            network_func_sockaddr(request->buffer, 0, uint32_t(size / 2), uint32_t(size / 2),
                &addr_loc, &size_loc, &addr_rem, &size_rem);
            reply.error = network_address_query::create(reply.conn->m_address, network_socket_type::TCP, size_t(size_rem), *addr_rem);
            conn->m_time = network_clock::now();
        }
    }
    else if (request->type == network_request_type::connect)
    {
        if (network_func_setsockopt(conn->m_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == SOCKET_ERROR)
        {
            reply.error = last_system_error();
        }
        else
        {
            conn->m_time = network_clock::now();
        }
    }
    else if (request->time != conn->m_time)
    {
        reply = {};
    }
    else if (request->type == network_request_type::disconnect)
    {
        ;
    }
    else if (entry.dwNumberOfBytesTransferred == 0)
    {
        reply.type = network_request_type::shutdown;
    }
    else if (request->type == network_request_type::recv)
    {
        if (reply.conn->m_type == network_connection_type::http)
        {
            auto done = false;
            if (reply.error = request->recv_http(done))
                ;
            else if (!done)
                reply = {};
        }
        else if (request->size < request->capacity)
        {
            if (reply.error = request->recv(request->capacity))
                ;
            else
                reply = {};
        }
    }
    else if (request->type == network_request_type::send)
    {
        if (request->size < request->capacity)
        {
            if (reply.error = request->send())
                ;
            else
                reply = {};
        }
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
/*error_type network_system_tcp::address(network_address& addr) noexcept
{
    return network_address_query::create(addr, m_addr, network_socket_type::TCP, m_socket);
}

error_type network_system_udp::multicast(const network_address& address, const bool join) noexcept
{
    if (!network_func_setsockopt)
        return make_stdlib_error(std::errc::function_not_supported);

    if (address.size() == sizeof(sockaddr_in))
    {
        ip_mreq mreq = {};
        mreq.imr_multiaddr = reinterpret_cast<sockaddr_in*>(address.data())->sin_addr;
        if (network_func_setsockopt(m_socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
            reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
            return last_system_error();
    }
    else if (address.size() == sizeof(sockaddr_in6))
    {
        ipv6_mreq mreq = {};
        mreq.ipv6mr_multiaddr = reinterpret_cast<sockaddr_in6*>(address.data())->sin6_addr;
        if (network_func_setsockopt(m_socket, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP,
            reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
            return last_system_error();
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    return error_type();
}
error_type network_system_udp::send(const network_address& address, const const_array_view<uint8_t> buffer) noexcept
{
    auto request = make_unique<network_udp_request_ex>();
    request->type = network_request_type::send;
    ICY_ERROR(request->bytes.assign(buffer));
    ICY_ERROR(network_address::copy(address, request->address));
    request->address_size = request->address.size();
    auto ptr = request.get();
    {
        ICY_LOCK_GUARD_WRITE(m_lock);
        ICY_ERROR(m_requests.push_back(std::move(request)));
    }
    ICY_ERROR(ptr->send(m_socket));
    return error_type();
}
error_type network_system_udp::recv(const size_t capacity) noexcept
{
    auto request = make_unique<network_udp_request_ex>();
    request->type = network_request_type::recv;
    ICY_ERROR(request->bytes.resize(capacity));
    ICY_ERROR(network_address_query::create(request->address, m_addr_type));
    request->address_size = request->address.size();
    auto ptr = request.get();
    {
        ICY_LOCK_GUARD_WRITE(m_lock);
        ICY_ERROR(m_requests.push_back(std::move(request)));
    }
    ICY_ERROR(ptr->recv(m_socket));
    return error_type();
}
error_type network_system_udp::loop(network_udp_request& reply, uint32_t& exit, const network_duration timeout) noexcept
{
    auto entry = OVERLAPPED_ENTRY{};
    auto count = 0ul;
    const auto offset = offsetof(network_udp_request_ex, overlapped);
    const auto success = GetQueuedCompletionStatusEx(m_iocp, &entry, 1, &count, network_timeout(timeout), TRUE);
    if (!success)
    {
        const auto error = last_system_error();
        if (error == make_system_error(make_system_error_code(WAIT_TIMEOUT)))
            return make_stdlib_error(std::errc::timed_out);
        else
            return error;
    }
    if (!count)
        return error_type();

    if (!entry.lpOverlapped)
    {
        exit = entry.dwNumberOfBytesTransferred;
        return error_type();
    }

    const auto request = reinterpret_cast<network_udp_request_ex*>(reinterpret_cast<char*>(entry.lpOverlapped) - offset);
    ICY_LOCK_GUARD_WRITE(m_lock);
    auto index = 0_z;
    for (; index < m_requests.size(); ++index)
    {
        if (m_requests[index].get() == request)
            break;
    }
    if (index < m_requests.size())
    {
        reply.type = request->type;
        reply.address = std::move(request->address);
        reply.bytes = std::move(request->bytes);
        reply.bytes.resize(entry.dwNumberOfBytesTransferred);
        for (auto k = index + 1; k < m_requests.size(); ++k)
            m_requests[k - 1] = std::move(m_requests[k]);
        m_requests.pop_back();
    }
    return error_type();
}
error_type network_system_udp::address(network_address& addr) noexcept
{
    return network_address_query::create(addr, m_addr, network_socket_type::UDP, m_socket);
}


bool icy::is_network_error(const error_type error) noexcept
{
    if (error.source == error_source_stdlib)
    {
        if (error.code >= EADDRINUSE &&
            error.code <= EWOULDBLOCK)
            return true;
    }
    else if (error.source == error_source_system)
    {
        if (error.code >= make_system_error_code(10000) && error.code < make_system_error_code(11999))
            return true;
    }
    return false;
/*
    // _CRT_NO_POSIX_ERROR_CODES
    switch (error)
    {
    case make_error(WSAENETDOWN):
    case make_error(WSAEHOSTDOWN):
    case make_error(ERROR_HOST_DOWN):
    case make_error(ENETDOWN):
        return std::errc::network_down;

    case make_error(ENETUNREACH):
    case make_error(ERROR_NETWORK_UNREACHABLE):
    case make_error(WSAENETUNREACH):
        return std::errc::network_unreachable;

    case make_error(ENETRESET):
    case make_error(WSAENETRESET):
        return std::errc::network_reset;

    case make_error(ECONNABORTED):
    case make_error(WSAECONNABORTED):
    case make_error(ERROR_OPERATION_ABORTED):
    case make_error(ERROR_NETNAME_DELETED):
    case make_error(ERROR_CONNECTION_ABORTED):
        return std::errc::connection_aborted;

    case make_error(ECONNRESET):
    case make_error(WSAECONNRESET):
        return std::errc::connection_reset;

    case make_error(ETIME):
    case make_error(ETIMEDOUT):
    case make_error(ERROR_TIMEOUT):
    case make_error(WSAETIMEDOUT):
        return std::errc::timed_out;

    case make_error(ECONNREFUSED):
    case make_error(ERROR_CONNECTION_REFUSED):
    case make_error(WSAECONNREFUSED):
        return std::errc::connection_refused;

    case make_error(EHOSTUNREACH):
    case make_error(ERROR_HOST_UNREACHABLE):
    case make_error(WSAEHOSTUNREACH):
        return std::errc::host_unreachable;

    case make_error(WSAEDISCON):
    case make_error(ERROR_NOT_CONNECTED):
    case make_error(ENOTCONN):
        return std::errc::not_connected;
    }
    return static_cast<std::errc>(0);
}*/