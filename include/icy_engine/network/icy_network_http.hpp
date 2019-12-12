#pragma once

#include "icy_http.hpp"
#include "icy_network.hpp"

namespace icy
{
    class json;
	class http_event;
	class http_thread;
	class http_system;

	class http_event
	{
		friend http_thread;
	public:
		network_request_type type = network_request_type::none;
		http_request request;
		error_type error;
		const network_address& address() const noexcept
		{
			return m_conn->address();
		}
	private:
		network_connection* m_conn = nullptr;
	};
	class http_thread
	{
	public:
		http_thread(http_system& system) noexcept;
		~http_thread() noexcept;
		error_type loop(http_event& event, bool& exit) noexcept;
		error_type reply(http_event& event, const http_response& response, const const_array_view<uint8_t> bytes) noexcept;
	private:
		http_system& m_system;
	};

	class http_config
	{
	public:
        struct key
        {
            static constexpr string_view port = "Port"_s;
            static constexpr string_view addr_type = "IP Address Type"_s;
            static constexpr string_view max_conn = "Maximum Connections"_s;
            static constexpr string_view max_size = "Maximum Request Length"_s;
            static constexpr string_view timeout = "Timeout"_s;
            static constexpr string_view file_path = "File Path"_s;
            static constexpr string_view file_size = "File Size"_s;
        };
        struct default_values
        {
            static constexpr auto max_size = 4_kb;
        };
    public:
		http_config() noexcept
		{

		}
		http_config(const uint16_t port, const size_t max_conn, const size_t max_size = default_values::max_size,
			const network_address_type addr_type = network_address_type::unknown, 
			const network_clock::duration timeout = network_default_timeout) noexcept :
			port(port), max_conn(max_conn), max_size(max_size), addr_type(addr_type), timeout(timeout)
		{

		}
        error_type from_json(const json& input) noexcept;
        error_type to_json(json& output) const noexcept;
        static error_type copy(const http_config& src, http_config& dst) noexcept;
	public:
		uint16_t port = 0;
		uint64_t max_conn = 0;
        uint64_t max_size = default_values::max_size;
		network_address_type addr_type = network_address_type::unknown;
		network_clock::duration timeout = network_default_timeout;
        string file_path;       //  log file path
        size_t file_size = 0;   //  log file size
	};
	class http_system
	{
		friend http_thread;
	public:		
        http_system() noexcept
        {

        }
        http_system(http_system&& rhs) noexcept : m_network(rhs.m_network), m_clients(rhs.m_clients),
            m_config(std::move(rhs.m_config)), m_cores(rhs.m_cores.load(std::memory_order_acquire))
        {
            rhs.m_network = nullptr;
            rhs.m_clients = nullptr;
            rhs.m_cores.store(0, std::memory_order_release);
        }
        ICY_DEFAULT_MOVE_ASSIGN(http_system);
		~http_system() noexcept
		{
			shutdown();
		}
		error_type launch(network_system& network, const http_config& config) noexcept;
		void shutdown() noexcept;
        const http_config& config() const noexcept
        {
            return m_config;
        }
	private:
		network_system* m_network = nullptr;
		network_connection* m_clients = nullptr;
		http_config m_config;
		std::atomic<size_t> m_cores = 0;
	};
}