#include <icy_engine/network/icy_network_http.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

error_type http_system::launch(network_system& network, const http_config& config) noexcept
{
    if (!config.max_conn || !config.port)
        return make_stdlib_error(std::errc::invalid_argument);

    http_config copy_config;
    ICY_ERROR(http_config::copy(config, copy_config));

	shutdown();
	auto done = false;
	ICY_SCOPE_EXIT{ if (!done) shutdown(); };

	auto clients = allocator_type::allocate<network_connection>(config.max_conn);
	if (!clients)
		return make_stdlib_error(std::errc::not_enough_memory);
	ICY_SCOPE_EXIT
	{
		if (!done)
		{
			for (auto k = config.max_conn; k--;)
				allocator_type::destroy(clients + k);
			allocator_type::deallocate(clients);
		}
	};
	for (auto k = 0u; k < config.max_conn; ++k)
		allocator_type::construct(clients + k, network_connection_type::http);
	
	ICY_ERROR(network.launch(config.port, network_socket_type::TCP, config.addr_type, config.max_conn));
	for (auto k = 0u; k < config.max_conn; ++k)
	{
		clients[k].timeout(config.timeout);
		ICY_ERROR(network.accept(clients[k]));		
	}
	m_clients = clients;
	m_config = std::move(copy_config);
	m_network = &network;
	done = true;
	return {};
}
error_type http_thread::loop(http_event& event, bool& exit) noexcept
{
	while (true)
	{
		network_reply reply;
		ICY_ERROR(m_system.m_network->loop(reply, exit));
		if (exit)
			break;

		//event.event = network_request_type::disconnect;
		event.m_conn = reply.conn;
		event.type = reply.type;
		event.request = {};

#define TRY_RECONNECT(X)\
		if (is_network_error(X) \
		|| (X) == make_stdlib_error(std::errc::illegal_byte_sequence)	\
		|| (X) == make_stdlib_error(std::errc::no_buffer_space))		\
		{ event.error = X; return m_system.m_network->accept(*reply.conn); }\
		else if (X) { event.error = X; break; }

		if (reply.error)
		{
			TRY_RECONNECT(reply.error);
			//reply.conn->timeout(m_system.m_config.timeout);
			//return m_system.m_network->accept(*reply.conn, http_min_size);
		}
		else if (
			reply.type == network_request_type::accept ||
			reply.type == network_request_type::recv)
		{
			if (reply.type == network_request_type::recv)
			{
               
				const auto text_error = event.request.initialize(
                    string_view(reinterpret_cast<const char*>(reply.bytes.data()), reply.bytes.size()));
				TRY_RECONNECT(text_error);
			}
			else
			{
				const auto recv_error = m_system.m_network->recv(*reply.conn, m_system.m_config.max_size);
				TRY_RECONNECT(recv_error);
			}
			break;
		}
		else if (reply.type == network_request_type::shutdown)
		{
			reply.conn->_close();
			return m_system.m_network->accept(*reply.conn);
		}
		else if (reply.type == network_request_type::timeout)
		{
			return m_system.m_network->disconnect(*reply.conn);
		}
		else if (reply.type == network_request_type::disconnect)
		{
			return m_system.m_network->accept(*reply.conn);
		}		
	}
	return {};
}
error_type http_thread::reply(http_event& event, const http_response& response, const const_array_view<uint8_t> bytes) noexcept
{
	if (event.type != network_request_type::recv || !event.m_conn)
		return make_stdlib_error(std::errc::invalid_argument);
	
	string msg;
	ICY_ERROR(to_string(response, msg, bytes));

    if (m_system.m_config.file_size && !m_system.m_config.file_path.empty())
    {
        file_info info;
        ICY_ERROR(info.initialize(m_system.m_config.file_path));
        if (info.size < m_system.m_config.file_size)
        {
            file log;
            ICY_ERROR(log.open(m_system.m_config.file_path, file_access::app, file_open::open_always, file_share::read | file_share::write));
            string addr_str;
            string rqst_str;
            ICY_ERROR(to_string(event.address(), addr_str));
            ICY_ERROR(to_string(event.request, rqst_str));
            string write;
            ICY_ERROR(to_string("%5__TIME__ : %1\r\n__FROM__ : %2\r\n__RECV__ : %3\r\n__SEND__ : %4"_s,
                write, std::chrono::system_clock::now(),
                string_view(addr_str), string_view(rqst_str), string_view(msg), info.size ? "\r\n"_s : ""_s));
            ICY_ERROR(log.append(write.bytes().data(), write.bytes().size()));
        }
    }

	if (event.error = m_system.m_network->send(*event.m_conn, msg.ubytes()))
		;
	else if (event.error = m_system.m_network->disconnect(*event.m_conn))
		;
	else
		return {};

	event.m_conn->_close();
	return m_system.m_network->accept(*event.m_conn);
}
http_thread::http_thread(http_system& system) noexcept : m_system(system)
{
	system.m_cores.fetch_add(1);
}
http_thread::~http_thread() noexcept
{
	m_system.m_cores.fetch_sub(1);
}
void http_system::shutdown() noexcept
{
	if (m_network)
	{
		while (m_cores.load() != 0)
		{
			m_network->stop();
            SwitchToThread();
		}
	}
	if (m_clients)
	{
		for (auto k = m_config.max_conn; k; --k)
			allocator_type::destroy(m_clients + k - 1);
		allocator_type::deallocate(m_clients);
		m_clients = nullptr;
	}
}

error_type http_config::from_json(const icy::json& json) noexcept
{
    if (const auto val = json.find(key::addr_type))
    {
        switch (hash(val->get()))
        {
        case "IPv4"_hash: addr_type = network_address_type::ip_v4; break; 
        case "IPv6"_hash: addr_type = network_address_type::ip_v6; break;
        }
    }
    json.get(key::file_path, file_path);
    json.get(key::file_size, file_size);
    json.get(key::port, port);
    json.get(key::max_conn, max_conn);
    json.get(key::max_size, max_size);

    auto timeout_ms = 0u;
    json.get(key::timeout, timeout_ms);
    timeout = std::chrono::milliseconds(timeout_ms);
    file_size *= 1_mb;
    
    return {};
}
error_type http_config::to_json(json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key::port, json_type_integer(port)));
    ICY_ERROR(output.insert(key::max_conn, json_type_integer(max_conn)));
    if (max_size != default_values::max_size)
    {
        ICY_ERROR(output.insert(key::max_size, json_type_integer(max_size)));
    }
    if (timeout != network_default_timeout)
    {
        ICY_ERROR(output.insert(key::timeout, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
    }
    
    if (!file_path.empty() && file_size >= 1_mb)
    {
        ICY_ERROR(output.insert(key::file_path, file_path));
        ICY_ERROR(output.insert(key::file_size, json_type_integer(file_size / 1_mb)));
    }

    if (addr_type != network_address_type::unknown)
    {
        string str_addr_type;
        ICY_ERROR(to_string(addr_type, str_addr_type));
        ICY_ERROR(output.insert(key::addr_type, std::move(str_addr_type)));
    }
    return {};
}
error_type http_config::copy(const http_config& src, http_config& dst) noexcept
{
    dst.port = src.port;
    dst.max_conn = src.max_conn;
    dst.max_size = src.max_size;
    dst.addr_type = src.addr_type;
    dst.timeout = src.timeout;
    dst.file_size = src.file_size;
    return to_string(src.file_path, dst.file_path);
}