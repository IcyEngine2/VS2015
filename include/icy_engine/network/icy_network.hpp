#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_event.hpp>
#include "icy_http.hpp"

struct sockaddr;

namespace icy
{
    namespace detail
    {
        class network_address_query;
        class network_system_data;
    }

    static const auto network_default_timeout = std::chrono::seconds{ 5 };
    static const auto network_default_queue = 5;
    static const auto network_default_buffer = 16 * 1024;

    class thread;
    class json;
    struct http_request;
    struct http_response;
	enum class network_address_type : uint32_t
	{
		ip_v4,
		ip_v6,
	};
    struct network_server_config
    {
        uint16_t port = 0;
        uint32_t capacity = 0;
        uint32_t queue = network_default_queue;
        uint32_t buffer = network_default_buffer;
        duration_type timeout = network_default_timeout;
        network_address_type addr_type = network_address_type::ip_v4;
    };
    class network_address;
    template<> inline int compare<network_address>(const network_address& lhs, const network_address& rhs) noexcept;

    class network_address
    {
        friend detail::network_address_query;
        friend error_type copy(const network_address& src, network_address& dst) noexcept;
    public:
        static error_type query(array<network_address>& array, const string_view host,
            const string_view port, const duration_type timeout = network_default_timeout) noexcept;
        rel_ops(network_address);
        network_address() noexcept = default;
        network_address(network_address&& rhs) noexcept;
        ICY_DEFAULT_MOVE_ASSIGN(network_address);
        ~network_address() noexcept;
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
        uint16_t port() const noexcept;
        void port(uint16_t port) noexcept;
        explicit operator bool() const noexcept
        {
            return !!m_addr;
        }
    private:
        network_address_type m_addr_type = network_address_type::ip_v4;
        size_t m_addr_len = 0;
        string m_name;
        sockaddr* m_addr = nullptr;
    };

    template<> inline int compare<network_address>(const network_address& lhs, const network_address& rhs) noexcept
    {
        if (lhs.size() < rhs.size())
            return -1;
        else if (lhs.size() > rhs.size())
            return +1;
        else
            return memcmp(lhs.data(), rhs.data(), lhs.size());
    }

    struct network_tcp_connection
    {
        network_tcp_connection(const uint32_t index = 0) noexcept : index(index)
        {

        }
        uint32_t index;
    };
    struct network_event
    {
        network_address address;
        array<uint8_t> bytes;
        error_type error;
        network_tcp_connection conn;
        struct
        {
            unique_ptr<http_request> request;
            unique_ptr<http_response> response;
        } http;
    };
    
    class network_system_tcp_client;
    error_type create_network_tcp_client(shared_ptr<network_system_tcp_client>& system,
        const network_address& address, const const_array_view<uint8_t> bytes, const duration_type timeout) noexcept;
    error_type create_network_tcp_client(shared_ptr<network_system_tcp_client>& system,
        const network_address& address, const http_request& http, const duration_type timeout) noexcept;
    class network_system_tcp_client : public event_system
    {
        friend error_type create_network_tcp_client(shared_ptr<network_system_tcp_client>& system,
            const network_address& address, const const_array_view<uint8_t> bytes, const duration_type timeout) noexcept;
        friend error_type create_network_tcp_client(shared_ptr<network_system_tcp_client>& system,
            const network_address& address, const http_request& http, const duration_type timeout) noexcept;
    public:
        ~network_system_tcp_client() noexcept;
        const icy::thread& thread() const noexcept
        {
            return *m_thread;
        }
        icy::thread& thread() noexcept
        {
            return *m_thread;
        }
        error_type send(const const_array_view<uint8_t> buffer) noexcept;
        error_type recv(const size_t capacity) noexcept;
        error_type signal(const event_data* event) noexcept override;
        error_type exec_once() noexcept;
    private:
        error_type exec() noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
        shared_ptr<icy::thread> m_thread;
    };
    
    class network_system_udp_client;
    error_type create_network_udp_client(shared_ptr<network_system_udp_client>& system, const network_server_config& args) noexcept;
    class network_system_udp_client : public event_system
    {
        friend error_type create_network_udp_client(shared_ptr<network_system_udp_client>& system, const network_server_config& args) noexcept;
    public:
        ~network_system_udp_client() noexcept;
        const icy::thread& thread() const noexcept
        {
            return *m_thread;
        }
        icy::thread& thread() noexcept
        {
            return *m_thread;
        }
        error_type join(const network_address& multicast) noexcept;
        error_type leave(const network_address& multicast) noexcept;
        error_type send(const network_address& address, const const_array_view<uint8_t> buffer) noexcept;
        error_type signal(const event_data* event) noexcept override;
    private:
        error_type exec() noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
        shared_ptr<icy::thread> m_thread;
    };

    class network_system_http_client;
    error_type create_network_http_client(shared_ptr<network_system_http_client>& system,
        const network_address& address, const http_request& request, const duration_type timeout, const size_t buffer) noexcept;

    class network_system_http_client : public event_system
    {
        friend error_type create_network_http_client(shared_ptr<network_system_http_client>& system,
            const network_address& address, const http_request& request, const duration_type timeout, const size_t buffer) noexcept;
    public:
        ~network_system_http_client() noexcept;
        const icy::thread& thread() const noexcept
        {
            return *m_thread;
        }
        icy::thread& thread() noexcept
        {
            return *m_thread;
        }
        error_type signal(const event_data* event) noexcept override;
        error_type send(const http_response& response) noexcept;
        error_type send(const http_request& request) noexcept;
        error_type recv() noexcept;
    private:
        error_type exec() noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
        shared_ptr<icy::thread> m_thread;
    };

    class network_system_tcp_server;
    error_type create_network_tcp_server(shared_ptr<network_system_tcp_server>& system, const network_server_config& args) noexcept;
    class network_system_tcp_server : public event_system
    {
        friend error_type create_network_tcp_server(shared_ptr<network_system_tcp_server>& system, const network_server_config& args) noexcept;
    public:
        ~network_system_tcp_server() noexcept;
        error_type open(const network_tcp_connection conn) noexcept;
        error_type close(const network_tcp_connection conn) noexcept;
        error_type send(const network_tcp_connection conn, const const_array_view<uint8_t> buffer) noexcept;
        error_type recv(const network_tcp_connection conn, const size_t capacity) noexcept;
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
    };
    class network_system_udp_server : public event_system
    {
        friend error_type create_network_udp_server(shared_ptr<network_system_udp_server>& system, const network_server_config& args) noexcept;
    public:
        ~network_system_udp_server() noexcept;
        error_type send(const network_address& addr, const const_array_view<uint8_t> buffer) noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
    };

    class network_system_http_server;
    error_type create_network_http_server(shared_ptr<network_system_http_server>& system, const network_server_config& args) noexcept;
    class network_system_http_server : public event_system
    {
        friend error_type create_network_http_server(shared_ptr<network_system_http_server>& system, const network_server_config& args) noexcept;
    public:
        ~network_system_http_server() noexcept;
        error_type send(const network_tcp_connection conn, const http_response& response) noexcept;
        error_type send(const network_tcp_connection conn, const http_request& request) noexcept;
        error_type recv(const network_tcp_connection conn) noexcept;
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
    private:
        detail::network_system_data* m_data = nullptr;
    };

    class network_udp_socket
    {
    public:
        network_udp_socket() noexcept = default;
        network_udp_socket(const network_udp_socket&) noexcept;
        ~network_udp_socket() noexcept;
        error_type initialize(const uint16_t port) noexcept;
        error_type send(const network_address& addr, const const_array_view<uint8_t> buffer) noexcept;
        error_type recv(const size_t size) noexcept;
    public:
        virtual void recv(const network_address& addr, const const_array_view<uint8_t> buffer) const noexcept
        {

        }
    private:
        class data_type;
        data_type* data = nullptr;
    };

    error_type to_string(const network_address& address, string& str) noexcept;
    
    error_type to_json(const network_server_config& src, json& dst) noexcept;
    error_type to_value(const json& src, network_server_config& dst) noexcept;
}
