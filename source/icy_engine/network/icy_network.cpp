#include <icy_engine/network/icy_network.hpp>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Iphlpapi.h>
#include <MSWSock.h>

using namespace icy;

static LPFN_ACCEPTEX network_func_accept;
static LPFN_GETACCEPTEXSOCKADDRS network_func_sockaddr;
static LPFN_CONNECTEX network_func_connect;
static LPFN_DISCONNECTEX network_func_disconnect;

static decltype(&::shutdown) network_func_shutdown;
static decltype(&::closesocket) network_func_closesocket;
static decltype(&::WSASocketW) network_func_socket;
static decltype(&::setsockopt) network_func_setsockopt;
static decltype(&::WSARecv) network_func_recv;
static decltype(&::WSASend) network_func_send;
static decltype(&::bind) network_func_bind;
static decltype(&::listen) network_func_listen;
static decltype(&::WSAAddressToStringW) network_func_addr_to_string;
static decltype(&::htons) network_func_htons;
static decltype(&::WSACleanup) network_func_cleanup;

class icy::network_address_query
{
public:
	network_address_query() noexcept = default;
	network_address_query(const network_address_query&) = delete;
	~network_address_query() noexcept
	{
		if (info && free) free(info);
		if (event) CloseHandle(event);
	}
	static error_type create(network_address& addr, const size_t addr_len, const sockaddr& addr_ptr) noexcept
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
	static void __stdcall callback(DWORD, DWORD, OVERLAPPED* overlapped) noexcept
	{
		const auto query = reinterpret_cast<network_address_query*>(overlapped);
		auto next = query->info;
		while (next)
		{
			network_address new_addr;
			if (next->ai_socktype == SOCK_DGRAM)
				new_addr.m_sock_type = network_socket_type::UDP;

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
	OVERLAPPED overlapped = {};
	PADDRINFOEX info = nullptr;
	HANDLE event = nullptr;
	error_type error;
	array<network_address>* buffer = nullptr;
    decltype(&FreeAddrInfoExW) free = nullptr;
};
class icy::network_request_base
{
public:
	static error_type create(const network_request_type type, size_t capacity, network_connection& conn, network_request_base*& request) noexcept;
	network_request_base(const network_request_type type, const size_t capacity, network_connection& connection) noexcept :
		type(type), capacity(capacity), connection(connection), time(connection.m_time)
	{

	}
	error_type recv(const size_t max) noexcept;
	error_type send() noexcept;
	error_type recv_http(bool& done) noexcept;
public:
	void* unused = nullptr;
	OVERLAPPED overlapped{};
	const network_request_type type;
	const size_t capacity;
	network_connection& connection;
	size_t size = 0;
	struct { size_t head; size_t body; } http = {};
	network_clock::time_point time;
	uint8_t buffer[1];
};

#define ICY_WSA_LIBRARY "ws2_32.dll"_lib

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
error_type network_address::copy(const network_address& from, network_address& to) noexcept
{
	string str;
	ICY_ERROR(to_string(from.m_name, str));
	auto new_addr = static_cast<sockaddr*>(icy::realloc(nullptr, from.m_addr_len));
	if (!new_addr && from.m_addr_len) return make_stdlib_error(std::errc::not_enough_memory);
	if (to.m_addr) icy::realloc(to.m_addr, 0);

	to.m_sock_type = from.m_sock_type;
	to.m_addr_type = from.m_addr_type;
	to.m_addr_len = from.m_addr_len;
	to.m_addr = new_addr;
	to.m_name = std::move(str);
	memcpy(new_addr, from.m_addr, from.m_addr_len);
	return {};
}

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
error_type network_socket::initialize(const network_socket_type sock_type, const network_address_type addr_type) noexcept
{
	shutdown();

	auto success = false;
	ICY_SCOPE_EXIT{ if (!success) shutdown(); };

    if (!network_func_socket || !network_func_setsockopt)
        return make_stdlib_error(std::errc::function_not_supported);

	m_value = network_func_socket(addr_type == network_address_type::ip_v6 ? AF_INET6 : AF_INET,
		sock_type == network_socket_type::TCP ? SOCK_STREAM : SOCK_DGRAM,
		sock_type == network_socket_type::TCP ? IPPROTO_TCP : IPPROTO_UDP,
		nullptr, 0, WSA_FLAG_OVERLAPPED);

	if (m_value == INVALID_SOCKET)
		return last_system_error();

	if (sock_type == network_socket_type::TCP)
	{
		ICY_ERROR(setopt(TCP_NODELAY, TRUE, IPPROTO_TCP));
		ICY_ERROR(setopt(SO_KEEPALIVE, FALSE));
	}
	ICY_ERROR(setopt(SO_REUSEADDR, TRUE));
	ICY_ERROR(setopt(SO_RCVBUF, 0));
	ICY_ERROR(setopt(SO_SNDBUF, 0));

	timeval tv = {}; 

    if (network_func_setsockopt(*this, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
		return last_system_error();
	if (network_func_setsockopt(*this, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) == SOCKET_ERROR)
		return last_system_error();

	success = true;
	return {};
}
error_type network_socket::setopt(const int opt, int value, const int level) noexcept
{
    if (!network_func_setsockopt)
        return make_stdlib_error(std::errc::function_not_supported);

	return network_func_setsockopt(m_value, level, opt,
		reinterpret_cast<const char*>(&value), sizeof(value)) == SOCKET_ERROR ? last_system_error() : error_type{};
}

void network_connection::reset() noexcept
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
void network_connection::_close() noexcept
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
error_type network_connection::make_timer() noexcept
{
	const auto callback = [](PTP_CALLBACK_INSTANCE, const PVOID ptr, PTP_TIMER)
	{
		const auto conn = static_cast<network_connection*>(ptr);
		PostQueuedCompletionStatus(conn->m_iocp, 0, reinterpret_cast<ULONG_PTR>(ptr), nullptr);
	};
	if (!m_timer)
	{
		m_timer = CreateThreadpoolTimer(callback, this, nullptr);
		if (!m_timer)
			return last_system_error();
	}
    SetThreadpoolTimer(m_timer, nullptr, 0, 0);
	return {};
}
network_connection::~network_connection() noexcept
{
	for (auto&& request : m_requests)
        icy::realloc(request, 0);
}

error_type network_request_base::create(const network_request_type type, const size_t capacity, network_connection& conn, network_request_base*& request) noexcept
{
	if (request)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(conn.m_requests.reserve(conn.m_requests.size() + 1));
	auto memory = icy::realloc(nullptr, sizeof(network_request_base) + capacity);
	if (!memory)
		return make_stdlib_error(std::errc::not_enough_memory);
	
	request = new (memory) network_request_base(type, capacity, conn);
	conn.m_requests.push_back(request);
	return {};
}
error_type network_request_base::recv(const size_t max) noexcept
{
    if (!network_func_recv)
        return make_stdlib_error(std::errc::function_not_supported);

	connection.reset();
	WSABUF buf = { ULONG(max - size), reinterpret_cast<CHAR*>(buffer + size) };
	auto bytes = 0ul;
	auto flags = 0ul;
	if (network_func_recv(connection.m_socket, &buf, 1, &bytes, &flags, &overlapped, nullptr) == SOCKET_ERROR)
	{
		const auto error = GetLastError();
		if (error != WSA_IO_PENDING)
			return make_system_error(make_system_error_code(uint32_t(error)));
	}
	return {};
}
error_type network_request_base::send() noexcept
{
    if (!network_func_send)
        return make_stdlib_error(std::errc::function_not_supported);

	connection.reset();
	WSABUF buf = { ULONG(capacity - size), reinterpret_cast<CHAR*>(buffer + size) };
	auto bytes = 0ul;
	if (network_func_send(connection.m_socket, &buf, 1, &bytes, 0, &overlapped, nullptr) == SOCKET_ERROR)
	{
		const auto error = GetLastError();
		if (error != WSA_IO_PENDING)
			return make_system_error(make_system_error_code(uint32_t(error)));
	}
	return {};
}
error_type network_request_base::recv_http(bool& done) noexcept
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
	return {};
}

void network_system::shutdown() noexcept
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
error_type network_system::initialize() noexcept
{
	shutdown();

    ICY_ERROR(m_library.initialize());

	auto done = false;
	auto wsaData = WSADATA{};

    const auto wsa_startup = ICY_FIND_FUNC(m_library, WSAStartup);
    const auto wsa_ioctl = ICY_FIND_FUNC(m_library, WSAIoctl);
    network_func_cleanup = ICY_FIND_FUNC(m_library, WSACleanup);
    
    if (!wsa_startup || !network_func_cleanup || !wsa_ioctl)
        return make_stdlib_error(std::errc::function_not_supported);

    if (wsa_startup(WINSOCK_VERSION, &wsaData) == SOCKET_ERROR)
        return last_system_error();

	if (MAKEWORD(wsaData.wHighVersion, wsaData.wVersion) != WINSOCK_VERSION)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_SCOPE_EXIT{ if (!done) network_func_cleanup(); };

	auto iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	ICY_SCOPE_EXIT{ if (iocp) CloseHandle(iocp); };

	const auto func = [wsa_ioctl](const SOCKET socket, GUID guid, auto& func) -> error_type
	{
		auto bytes = 0ul;
		if (wsa_ioctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
			&func, sizeof(func), &bytes, nullptr, nullptr) == SOCKET_ERROR)
			return last_system_error();
		else
			return {};
	};

    network_func_shutdown = reinterpret_cast<decltype(&::shutdown)>(m_library.find("shutdown"));
    network_func_closesocket = ICY_FIND_FUNC(m_library, closesocket);
    network_func_socket = ICY_FIND_FUNC(m_library, WSASocketW);
    network_func_setsockopt = ICY_FIND_FUNC(m_library, setsockopt);
    network_func_recv = ICY_FIND_FUNC(m_library, WSARecv);
    network_func_send = ICY_FIND_FUNC(m_library, WSASend);
    network_func_bind = reinterpret_cast<decltype(&::bind)>(m_library.find("bind"));
    network_func_listen = ICY_FIND_FUNC(m_library, listen);
    network_func_addr_to_string = ICY_FIND_FUNC(m_library, WSAAddressToStringW);
    network_func_htons = ICY_FIND_FUNC(m_library, htons);

	network_socket socket;
	ICY_ERROR(socket.initialize(network_socket_type::TCP, network_address_type::ip_v4));
	ICY_ERROR(func(socket, WSAID_ACCEPTEX, network_func_accept));
	ICY_ERROR(func(socket, WSAID_CONNECTEX, network_func_connect));
	ICY_ERROR(func(socket, WSAID_DISCONNECTEX, network_func_disconnect));
	ICY_ERROR(func(socket, WSAID_GETACCEPTEXSOCKADDRS, network_func_sockaddr));


	m_iocp = iocp;
	iocp = nullptr;
	done = true;
	return {};
}
error_type network_system::address(const string_view host, const string_view port, const network_duration timeout, const network_socket_type type, array<network_address>& array) noexcept
{
    const auto func_get_addr_info = ICY_FIND_FUNC(m_library, GetAddrInfoExW);
    const auto func_cancel_addr_info = ICY_FIND_FUNC(m_library, GetAddrInfoExCancel);
    const auto func_free_addr_info = ICY_FIND_FUNC(m_library, FreeAddrInfoExW);
    if (!func_get_addr_info || !func_cancel_addr_info || !func_free_addr_info)
        return make_stdlib_error(std::errc::function_not_supported);

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
	hints.ai_socktype = type == network_socket_type::UDP ? SOCK_DGRAM : SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	network_address_query query;
	query.buffer = &array;
	query.event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    query.free = func_free_addr_info;
	if (!query.event)
		return last_system_error();

	HANDLE cancel = nullptr;
    const auto error = func_get_addr_info(buf_addr, buf_port, NS_ALL, nullptr, &hints, &query.info, nullptr,
		&query.overlapped, &network_address_query::callback, &cancel);

	if (error == 0)
	{
		network_address_query::callback(uint32_t(error), 0, &query.overlapped);
	}
	else if (error != WSA_IO_PENDING)
	{
		return make_system_error(make_system_error_code(uint32_t(error)));
	}
	else if (WaitForSingleObject(query.event, network_timeout(timeout)) == WAIT_TIMEOUT)
	{
		func_cancel_addr_info(&cancel);
		WaitForSingleObject(query.event, INFINITE);
		return make_stdlib_error(std::errc::timed_out);
	}
	return query.error;
}
error_type network_system::launch(const uint16_t port, const network_socket_type sock_type, const network_address_type addr_type, const size_t max_queue) noexcept
{
    if (!network_func_bind || !network_func_htons || !network_func_listen)
        return make_stdlib_error(std::errc::function_not_supported);

	if (sock_type != network_socket_type::TCP || !port)
		return make_stdlib_error(std::errc::invalid_argument);
	
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
		auto server_addr_v4 = sockaddr_in{ };
		server_addr_v4.sin_family = AF_INET;
		server_addr_v4.sin_port = network_func_htons(port);
		if (network_func_bind(socket, reinterpret_cast<sockaddr*>(&server_addr_v4), sizeof(server_addr_v4)) == SOCKET_ERROR)
			return last_system_error();
	}
	

	if (network_func_listen(socket, int(max_queue)) == SOCKET_ERROR)
		return last_system_error();

	m_socket = std::move(socket);
	m_addr = addr_type;
	return {};
}
error_type network_system::connect(network_connection& conn, const network_address& address, const const_array_view<uint8_t> send) noexcept
{
    if (!network_func_bind || !network_func_listen || !network_func_connect)
        return make_stdlib_error(std::errc::function_not_supported);

	ICY_LOCK_GUARD(conn.m_lock);
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
	network_request_base* ref = nullptr;
	ICY_ERROR(network_request_base::create(network_request_type::connect, send.size(), conn, ref));
	memcpy(ref->buffer, send.data(), send.size());

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
	if (!network_func_connect(conn.m_socket, address.data(), address.size(), ref->buffer, 
		uint32_t(ref->capacity), &bytes, &ref->overlapped))
	{
		const auto error = GetLastError();
		if (error != WSA_IO_PENDING)
			return make_system_error(make_system_error_code(uint32_t(error)));
	}
	conn.reset();
	return {};
}
error_type network_system::accept(network_connection& conn) noexcept
{
    if (!network_func_accept)
        return make_stdlib_error(std::errc::function_not_supported);

	ICY_LOCK_GUARD(conn.m_lock);

	if (!conn.m_socket)
	{
		conn.m_time = {};
		ICY_ERROR(conn.m_socket.initialize(network_socket_type::TCP, m_addr));

		const auto file = reinterpret_cast<HANDLE>(SOCKET(conn.m_socket));
		conn.m_iocp = CreateIoCompletionPort(file, m_iocp, reinterpret_cast<ULONG_PTR>(&conn), 0);
		if (conn.m_iocp != m_iocp)
			return last_system_error();
	}

	network_request_base* ref = nullptr;
	const auto size = 2 * ((m_addr == network_address_type::ip_v6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) + 16);
	ICY_ERROR(network_request_base::create(network_request_type::accept, size, conn, ref));
	auto bytes = 0ul;
	if (!network_func_accept(m_socket, conn.m_socket, ref->buffer, 0, 
		uint32_t(size / 2), uint32_t(size / 2), &bytes, &ref->overlapped))
	{
		const auto error = GetLastError();
		if (error != WSA_IO_PENDING)
			return make_system_error(make_system_error_code(uint32_t(error)));
	}
	return {};
}
error_type network_system::send(network_connection& conn, const const_array_view<uint8_t> buffer) noexcept
{
	ICY_LOCK_GUARD(conn.m_lock);
	network_request_base* ref = nullptr;
	ICY_ERROR(network_request_base::create(network_request_type::send, buffer.size(), conn, ref));
	memcpy(ref->buffer, buffer.data(), buffer.size());
	return ref->send();
}
error_type network_system::recv(network_connection& conn, const size_t capacity) noexcept
{
	ICY_LOCK_GUARD(conn.m_lock);
	network_request_base* ref = nullptr;
	ICY_ERROR(network_request_base::create(network_request_type::recv, capacity, conn, ref));
	return ref->recv(ref->capacity);
}
error_type network_system::disconnect(network_connection& conn) noexcept
{
    if (!network_func_disconnect)
        return make_stdlib_error(std::errc::function_not_supported);

	ICY_LOCK_GUARD(conn.m_lock);
	network_request_base* ref = nullptr;
	ICY_ERROR(network_request_base::create(network_request_type::disconnect, 0, conn, ref));
	if (!network_func_disconnect(conn.m_socket, &ref->overlapped, TF_REUSE_SOCKET, 0))
	{
		const auto error = GetLastError();
		if (error != WSA_IO_PENDING)
			return make_system_error(make_system_error_code(uint32_t(error)));
	}
	return {};
}
error_type network_system::loop(network_reply& reply, bool& exit, const network_duration timeout) noexcept
{
    if (!network_func_setsockopt || !network_func_sockaddr)
        return make_stdlib_error(std::errc::function_not_supported);

    auto entry = OVERLAPPED_ENTRY{};
    auto count = 0ul;
    const auto offset = offsetof(network_request_base, overlapped);
    const auto success = GetQueuedCompletionStatusEx(m_iocp, &entry, 1, &count, network_timeout(timeout), TRUE);
    if (!success)
        return last_system_error();
    if (!count)
        return {};

    const auto request = reinterpret_cast<network_request_base*>(reinterpret_cast<char*>(entry.lpOverlapped) - offset);
    auto conn = reply.conn = reinterpret_cast<network_connection*>(entry.lpCompletionKey);

    if (!entry.lpOverlapped)
    {
        if (conn)
        {
            reply.type = network_request_type::timeout;
            ICY_LOCK_GUARD(reply.conn->m_lock);
        }
        else
        {
            exit = true;
        }
        return {};
    }
    else if (!conn)
    {
        conn = reply.conn = &request->connection;
    }
    ICY_LOCK_GUARD(conn->m_lock);

    auto ptr = std::find(conn->m_requests.begin(), conn->m_requests.end(), request);
    if (ptr == conn->m_requests.end())
    {
        reply = {};
        return {};
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
		const auto size = 2 * ((m_addr == network_address_type::ip_v6 ? sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) + 16);
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
			reply.error = network_address_query::create(reply.conn->m_address, size_t(size_rem), *addr_rem);
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
	return {};
}
error_type network_system::stop() noexcept
{
	if (!PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr))
		return last_system_error();
	return {};
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
	return static_cast<std::errc>(0);*/
}

error_type icy::to_string(const network_address& address, string& str) noexcept
{
    if (!network_func_addr_to_string)
        return make_stdlib_error(std::errc::function_not_supported);

	if (!address.size())
	{
		str.clear();
		return {};
	}
	wchar_t buffer[64] = {};
    auto size = DWORD(sizeof(buffer) / sizeof(*buffer));
	if (network_func_addr_to_string(address.data(), uint32_t(address.size()), nullptr, buffer, &size) == SOCKET_ERROR)
		return last_system_error();
	return to_string(const_array_view<wchar_t>(buffer, size), str);
}