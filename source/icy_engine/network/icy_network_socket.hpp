#pragma once

#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Iphlpapi.h>
#include <MSWSock.h>

namespace icy
{ 
    namespace detail
    {
        error_type network_func_init(library& lib) noexcept;

        error_type network_setopt(SOCKET sock, const int opt, int value, const int level = 0xFFFF) noexcept;
        extern decltype(&::getsockopt) network_func_getsockopt;
        extern decltype(&::setsockopt) network_func_setsockopt;
        extern decltype(&::shutdown) network_func_shutdown;
        extern decltype(&::closesocket) network_func_closesocket;
        extern decltype(&::WSASocketW) network_func_socket;

        template<typename T>
        static error_type query_socket_func(library& lib, const SOCKET socket, GUID guid, T& func) noexcept
        {
            const auto wsa_ioctl = ICY_FIND_FUNC(lib, WSAIoctl);
            if (!wsa_ioctl)
                return make_stdlib_error(std::errc::function_not_supported);
            auto bytes = 0ul;
            if (wsa_ioctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                &func, sizeof(func), &bytes, nullptr, nullptr) == SOCKET_ERROR)
                return last_system_error();
            else
                return {};
        }

        class network_socket
        {
        public:
            enum class type { tcp, udp };
        public:
            network_socket() noexcept = default;
            network_socket(network_socket&& rhs) noexcept : m_value(rhs.m_value)
            {
                rhs.m_value = SOCKET(-1);
            }
            ICY_DEFAULT_MOVE_ASSIGN(network_socket);
            ~network_socket() noexcept
            {
                shutdown();
            }
            void shutdown() noexcept;
            icy::error_type initialize(const type sock_type, const icy::network_address_type addr_type) noexcept;
            icy::error_type setopt(const int opt, int value, const int level = 0xFFFF) noexcept
            {
                return icy::detail::network_setopt(m_value, opt, value, level);
            }
            operator SOCKET() const noexcept
            {
                return m_value;
            }
            type get_type() const noexcept;
        private:
            SOCKET m_value = -1;
        };
        struct network_connection;

        struct network_tcp_overlapped
        {
            OVERLAPPED overlapped = {};
            network_connection* conn = nullptr;
            event_type type = event_type::none;
            array<uint8_t> bytes;
            uint32_t offset = 0;
            uint32_t http_header = 0;   //  "Content-Length: X"
            uint32_t http_length = 0;   //  X
            uint32_t http_offset = 0;   //  "\r\n\r\n"
        };
        struct network_udp_overlapped
        {
            OVERLAPPED overlapped = {};
            event_type type = event_type::none;
            array<uint8_t> bytes;
            char addr_buf[sizeof(sockaddr_in6)];
            int addr_len = sizeof(addr_buf);
            void* user = nullptr;
        };
    }
}