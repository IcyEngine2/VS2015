#pragma once

#include "icy_network_socket.hpp"
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/network/icy_http.hpp>

struct icy::detail::network_connection
{
    network_connection() noexcept = default;
    ~network_connection() noexcept
    {
        shutdown();
    }
    network_connection(network_connection&&) noexcept = default;
    void shutdown() noexcept;
    error_type accept(network_system_data& system) noexcept;
    error_type do_recv() noexcept;
    error_type next_recv(array<uint8_t>&& bytes) noexcept;
    error_type do_send() noexcept;
    error_type next_send(array<uint8_t>&& bytes) noexcept;
    error_type do_disc() noexcept;
    error_type next_disc() noexcept;
    
    network_socket socket;
    uint32_t version = 0;
    detail::network_tcp_overlapped ovl_recv;
    detail::network_tcp_overlapped ovl_send;
    mpsc_queue<detail::network_tcp_overlapped> rqueue;
    mpsc_queue<detail::network_tcp_overlapped> squeue;
    network_address addr;
    struct
    {
        unique_ptr<http_request> request;
        unique_ptr<http_response> response;
    } http;
};
class icy::detail::network_system_data
{
    friend detail::network_connection;
    static constexpr auto http_header_bits = 0x10;  //  64 kb
    static constexpr auto http_length_bits = 0x1B;  //  64 mb
    static constexpr auto http_max_header = (1 << http_header_bits) - 1;
    static constexpr auto http_max_length = (1 << http_length_bits) - 1;

    struct network_command
    {
        network_tcp_connection conn;
        event_type type = event_type::none;
        array<uint8_t> bytes;
        network_address address;
        struct
        {
            unique_ptr<http_request> request;
            unique_ptr<http_response> response;
        } http;
    };
public:
    static void destroy(network_system_data*& ptr) noexcept;
    static error_type create(network_system_data*& ptr, const bool http) noexcept;
    ~network_system_data() noexcept
    {
        shutdown();
    }
    error_type initialize(const bool http) noexcept;
    void shutdown() noexcept;
    error_type launch(const network_server_config& args, const network_socket::type sock_type) noexcept;
    error_type connect(const network_address& address, const_array_view<uint8_t> bytes,
        const duration_type timeout, array<uint8_t>&& recv_buffer = {}) noexcept;
    error_type cancel() noexcept;
    error_type post(const network_tcp_connection conn, const event_type type, array<uint8_t>&& bytes, const network_address* addr = nullptr) noexcept;
    error_type post(const network_tcp_connection conn, unique_ptr<http_response>&& response) noexcept;
    error_type post(const network_tcp_connection conn, unique_ptr<http_request>&& request) noexcept;
    error_type loop_tcp(event_system& system) noexcept;
    error_type loop_udp(event_system& system) noexcept;
    error_type join(const network_address& addr, bool join) noexcept;
    const network_server_config& config() const noexcept
    {
        return m_config;
    }
private:
    uint32_t addr_size() const noexcept
    {
        return (m_config.addr_type == network_address_type::ip_v6 ?
            sizeof(SOCKADDR_IN6) : sizeof(SOCKADDR_IN)) + 16;
    }
private:
    library m_library = "ws2_32"_lib;
    decltype(&WSACleanup) m_wsa = nullptr;
    void* m_iocp = nullptr;
    network_socket m_socket;
    decltype(&::bind) m_bind = nullptr;
    decltype(&::htons) m_htons = nullptr;
    mpsc_queue<network_command> m_cmds;
    array<network_connection> m_conn;
    network_server_config m_config;
    array< detail::network_udp_overlapped> m_udp_recv;
    struct udp_type
    {
        detail::network_udp_overlapped ovl;
        mpsc_queue<detail::network_udp_overlapped> queue;
    } m_udp_send;
    bool m_http = false;
};