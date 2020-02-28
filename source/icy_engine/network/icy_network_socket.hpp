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
        error_type network_setopt(SOCKET sock, const int opt, int value, const int level = 0xFFFF) noexcept; 
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
        private:
            SOCKET m_value = -1;
        };
    }
}