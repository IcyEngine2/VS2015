#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>

using SOCKET = size_t;
struct _TP_TIMER;
struct _OVERLAPPED;
struct sockaddr;

namespace icy
{
    static const auto network_code_exit = UINT32_MAX;
	using network_clock = std::chrono::system_clock;
	using network_duration = network_clock::duration;
	class network_address;
	class network_address_query;
    struct network_tcp_reply;
	class network_tcp_request;
	class network_tcp_connection;
	class network_system_base;
    class network_system_tcp;
    class network_system_udp;
	//template<typename K, typename V> class map;

	enum class network_address_type : uint32_t
	{
		unknown,
		ip_v4,
		ip_v6,
	};
	enum class network_socket_type : uint32_t
	{
		TCP,
		UDP,
	};
	enum class network_connection_type : uint32_t
	{
		none,
		http,
	};
	enum class network_request_type : uint32_t
	{
		none,
		connect,
		disconnect,	//	expected
		shutdown,	//	unexpected
		accept,
		send,
		recv,
		timeout,
	};
	bool is_network_error(const error_type error) noexcept;

	static const auto network_default_timeout = std::chrono::seconds{ 5 };
	inline unsigned long network_timeout(const network_duration timeout) noexcept
	{
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
		return ms.count() < 0 ? 0 : ms.count() > UINT32_MAX ? UINT32_MAX : (unsigned long)(ms.count());
	}

	class network_address
	{
        friend network_address_query;
	public:
		network_address() noexcept = default;
		network_address(network_address&& rhs) noexcept;
		ICY_DEFAULT_MOVE_ASSIGN(network_address);
		~network_address() noexcept;
		static error_type copy(const network_address& from, network_address& to) noexcept;
		auto data() const noexcept
		{
			return m_addr;
		}
		auto size() const noexcept
		{
			return int(m_addr_len);
		}
		auto addr_type() const noexcept
		{
			return m_addr_type;
		}
		auto sock_type() const noexcept
		{
			return m_sock_type;
		}
		auto name() const noexcept
		{
			return string_view(m_name);
		}
		explicit operator bool() const noexcept
		{
			return !!m_addr;
		}
	private:
		network_socket_type m_sock_type = network_socket_type::TCP;
		network_address_type m_addr_type = network_address_type::unknown;
		size_t m_addr_len = 0;
		string m_name;
		sockaddr* m_addr = nullptr;
	};
	class network_socket
	{
	public:
		network_socket() noexcept : m_value{ SOCKET(-1) }
		{

		}
		network_socket(network_socket&& rhs) noexcept : m_value{ rhs.m_value }
		{
			rhs.m_value = SOCKET(-1);
		}
		ICY_DEFAULT_MOVE_ASSIGN(network_socket);
		~network_socket() noexcept
		{
			shutdown();
		}
		explicit operator bool() const noexcept
		{
			return m_value != SOCKET(-1);
		}
		operator SOCKET() const noexcept
		{
			return m_value;
		}
		error_type setopt(const int opt, int value, const int level = 0xffff) noexcept;
        error_type initialize(const network_socket_type sock_type, const network_address_type addr_type) noexcept;
        void shutdown() noexcept;
	private:
		SOCKET m_value;
	};
    struct network_tcp_reply
    {
        array<uint8_t> bytes;
        error_type error;
        network_request_type type = network_request_type::none;
        network_tcp_connection* conn = nullptr;
    };
    struct network_udp_request
    {
        array<uint8_t> bytes;
        network_request_type type = network_request_type::none;
        network_address address;
    };
	class network_tcp_connection
	{
		friend network_tcp_request;
		friend network_system_tcp;
	public:
        network_tcp_connection(const network_connection_type type) noexcept : m_type(type)
		{

		}
        network_tcp_connection(const network_tcp_connection& rhs) = delete;
		~network_tcp_connection() noexcept;
		auto type() const
		{
			return m_type;
		}
		auto& address() const noexcept
		{
			return m_address;
		}
		auto timeout() const noexcept
		{
			return m_timeout;
		}
		auto timeout(const network_duration value) noexcept
		{
			m_timeout = value;
			reset();
		}
		void _close() noexcept;
	private:
		void reset() noexcept;
		error_type make_timer() noexcept;
	private:
		const network_connection_type m_type;
		network_socket m_socket;
		network_duration m_timeout = network_default_timeout;
		network_address m_address;
		_TP_TIMER* m_timer = nullptr;
		void* m_iocp = nullptr;
		network_clock::time_point m_time;
		detail::spin_lock<> m_lock;
		array<network_tcp_request*> m_requests;
	};
    class network_system_base
    {
    public:
        network_system_base(const network_socket_type sock_type) noexcept : m_sock_type(sock_type)
        {

        }
        network_system_base(network_system_base&& rhs) noexcept : m_sock_type(rhs.m_sock_type),
            m_library(std::move(rhs.m_library)), m_socket(std::move(rhs.m_socket)), m_addr_type(rhs.m_addr_type), m_iocp(rhs.m_iocp)
        {
            rhs.m_iocp = nullptr;
        }
        ~network_system_base() noexcept
        {
            shutdown();
        }
        void shutdown() noexcept;
        error_type initialize() noexcept;
        error_type address(const string_view host, const string_view port, const network_duration timeout, const network_socket_type type, array<network_address>& array) noexcept;
        error_type stop(const uint32_t code) noexcept;
    protected:
        error_type launch(const uint16_t port, const network_socket_type sock_type, const network_address_type addr_type, const size_t max_queue) noexcept;
    protected:
        const network_socket_type m_sock_type;
        library m_library = "ws2_32.dll"_lib;
        network_socket m_socket;
        network_address_type m_addr_type = network_address_type::unknown;
        void* m_iocp = nullptr;
    };
	class network_system_tcp : public network_system_base
	{
	public:
        network_system_tcp() noexcept : network_system_base(network_socket_type::TCP)
        {

        }
        error_type launch(const uint16_t port, const network_address_type addr_type, const size_t max_queue) noexcept
        {
            return network_system_base::launch(port, network_socket_type::TCP, addr_type, max_queue);
        }
		error_type connect(network_tcp_connection& conn, const network_address& address, const const_array_view<uint8_t> bytes) noexcept;
		error_type disconnect(network_tcp_connection& conn) noexcept;
		error_type accept(network_tcp_connection& conn) noexcept;
		error_type send(network_tcp_connection& conn, const const_array_view<uint8_t> buffer) noexcept;
		error_type recv(network_tcp_connection& conn, const size_t capacity) noexcept;
		error_type loop(network_tcp_reply& reply, uint32_t& code, const network_duration timeout = std::chrono::seconds(UINT32_MAX)) noexcept;
	};
    class network_system_udp : public network_system_base
    {
    public:
        network_system_udp() noexcept : network_system_base(network_socket_type::UDP)
        {

        }
        error_type launch(const uint16_t port, const network_address_type addr_type, const size_t max_queue) noexcept
        {
            return network_system_base::launch(port, network_socket_type::UDP, addr_type, max_queue);
        }
        error_type multicast(const network_address& address, const bool join) noexcept;
        error_type send(const network_address& address, const_array_view<uint8_t> bytes) noexcept;
        error_type recv(const size_t capacity) noexcept;
        error_type loop(network_udp_request& request, uint32_t& exit, const network_duration timeout = std::chrono::seconds(UINT32_MAX)) noexcept;
    private:
        detail::spin_lock<> m_lock;
        array<unique_ptr<network_udp_request>> m_requests;
    };
	error_type to_string(const network_address& address, string& str) noexcept;
    inline error_type to_string(const network_address_type type, string& str) noexcept
    {
        switch (type)
        {
        case network_address_type::ip_v4:
            return to_string("IPv4"_s, str);
        case network_address_type::ip_v6:
            return to_string("IPv6"_s, str);
        }
        return to_string("Any"_s, str);
    }
}