#include "icy_auth_http.hpp"
#include "icy_auth_database.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

error_type auth_network_system::initialize(const auth_config_network& config, const size_t cores, const const_array_view<auth_request_type> types) noexcept
{
    ICY_ERROR(auth_config_network::copy(config, m_config));
    ICY_ERROR(m_types.assign(types));
    ICY_ERROR(create_network_http_server(m_network, m_config.http));
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = m_network.get();
    return error_type();
}
error_type auth_network_system::status(string& msg) const noexcept
{
    auto& thread = *m_thread;
    string_view status;
    if (thread.state() == thread_state::done)
    {
        if (thread.error())
            status = "Error: "_s;
        else
            status = "Done"_s;
    }
    else if (thread.state() == thread_state::run)
    {
        status = "Running"_s;
    }
    else
    {
        status = "-"_s;
    }

    ICY_ERROR(msg.appendf("Thread [%1]: %2"_s, thread.index(), status));
    if (const auto error = thread.error())
    {
        ICY_ERROR(msg.appendf("\"%1\" (code %2) - %3"_s, error.source, error.code, error));
    }
    return error_type();
}
error_type auth_network_system::process(const string_view str_request, const network_address& address, json& json_output) noexcept
{
    error_type error;

    json json_request;
    auth_request auth_request;

    if (!error)
        error = to_value(str_request, json_request);
    if (error == make_stdlib_error(std::errc::illegal_byte_sequence))
        error = make_auth_error(auth_error_code::invalid_json);

    if (!error)
        error = auth_request.from_json(json_request);

    auth_response auth_response(auth_request);
    if (!error)
    {
        const auto found = std::find(m_types.begin(), m_types.end(), auth_request.type);
        if (found == m_types.end())
            error = make_auth_error(auth_error_code::invalid_json_type);
    }
    if (!error)
    {
        const auto now = std::chrono::system_clock::now();
        if (auth_request.time > now || auth_request.time < now - m_config.http.timeout)
            error = make_auth_error(auth_error_code::timeout);
    }
    if (!error)
        error = m_dbase.exec(auth_request, auth_response);

    if (!error || error.source == error_source_auth)
    {
        auth_response.error = auth_error_code(error.code);
        
        auto log_entry = 0;
        const auto log_entry_first = -1;
        const auto log_entry_next = +1;

        if (m_config.file_size && !m_config.file_path.empty())
        {
            const auto valid_code = m_config.any_error ? true :
                std::find(m_config.errors.begin(), m_config.errors.end(), error.code) != m_config.errors.end();
            if (valid_code)
            {
                file_info info;
                info.initialize(m_config.file_path);
                if (info.size < m_config.file_size)
                    log_entry = info.size ? log_entry_next : log_entry_first;
            }
        }
        if (log_entry)
        {
            string time_str;
            string addr_str;
            string user_str;
            string type_str;
            string error_str;
            ICY_ERROR(to_string(auth_clock::now(), time_str, false));
            ICY_ERROR(to_string(address, addr_str));
            ICY_ERROR(to_string(auth_request.username, 0x10, user_str));

            if (auth_request.type == auth_request_type::client_connect)
            {
                json_type_integer module = 0;
                auth_request.message.get(module);
                ICY_ERROR(to_string(uint64_t(module), 0x10, type_str));
                type_str.insert(type_str.begin(), "Connect "_s);
            }
            if (auth_request.type != auth_request_type::none)
            {
                ICY_ERROR(to_string(auth_request.type, type_str));
            }
            else
            {
                ICY_ERROR(to_string("Unknown type"_s, type_str));
            }

            if (error.code)
            {
                ICY_ERROR(to_string(error, error_str));
            }
            else
            {
                ICY_ERROR(to_string("Sucess"_s, error_str));
            }
            string msg;
            ICY_ERROR(to_string("%1%2\t[%3]\t%4\t%5\t%6"_s, msg, log_entry == log_entry_first ? ""_s : "\r\n"_s,
                string_view(time_str), string_view(addr_str), string_view(user_str), string_view(type_str), string_view(error_str)));
            
            file log;
            ICY_ERROR(log.open(m_config.file_path, file_access::app, file_open::open_always, file_share::read | file_share::write));
            ICY_ERROR(log.append(msg.bytes().data(), msg.bytes().size()));
        }
    }
    ICY_ERROR(auth_response.to_json(json_output));
    return error;
}