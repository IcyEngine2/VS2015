#include "icy_auth_http.hpp"
#include "icy_auth_database.hpp"
#include <icy_engine/core/icy_file.hpp>

using namespace icy;

error_type auth_network_thread::run() noexcept
{
    http_thread http(m_system.m_http);
    auto exit = false;
    while (!exit)
    {
        http_event event;
        ICY_ERROR(http.loop(event, exit));
        if (event.type == network_request_type::recv)
        {
            http_response http_response;
            string output;
            http_response.herror = http_error::bad_request;

            if (!event.error
                && event.request.type == http_request_type::post
                && event.request.content == http_content_type::application_json)
            {
                json response;
                const auto error = m_system.process(event.request.body, event.address(), response);
                if (response.type() != json_type::none)
                {
                    if (!response.find("Error"))
                        http_response.herror = http_error::success;
                    http_response.type = http_content_type::application_json;
                }
                if (error.source == error_source_auth)
                    ;
                else
                    ICY_ERROR(error);

                ICY_ERROR(to_string(response, output));
            }
            ICY_ERROR(http.reply(event, http_response, output.ubytes()));
        }
    }
    return {};
}

error_type auth_network_system::initialize(const auth_config_network& config, const size_t cores, const const_array_view<auth_request_type> types) noexcept
{
    ICY_ERROR(wait());
    m_threads.clear();

    ICY_ERROR(auth_config_network::copy(config, m_config));
    ICY_ERROR(m_threads.reserve(cores));
    for (auto k = 0; k < cores; ++k)
    {
        ICY_ERROR(m_threads.emplace_back(*this));
    }
    ICY_ERROR(m_types.assign(types));
    return {};
}
error_type auth_network_system::wait() noexcept
{
    m_http.shutdown();
    for (auto&& thread : m_threads)
        ICY_ERROR(thread.wait());
    m_network.shutdown();
    return {};
}
error_type auth_network_system::launch() noexcept
{
    ICY_ERROR(wait());
    ICY_ERROR(m_network.initialize());
    ICY_ERROR(m_http.launch(m_network, m_config.http));
    for (auto&& thread : m_threads)
        ICY_ERROR(thread.launch());
    return {};
}
error_type auth_network_system::status(string& msg) const noexcept
{
    for (auto&& thread : m_threads)
    {
        ICY_ERROR(msg.appendf("\r\nThread [%1]: %2"_s, thread.index(), thread.running() ? "Running" : "Error: "_s));
        if (!thread.running())
        {
            const auto error = thread.error();
            ICY_ERROR(msg.appendf("\"%1\" (code %2) - %3"_s, error.source, error.code, error));
        }
    }
    return {};
}
error_type auth_network_system::process(const string_view str_request, const network_address& address, json& json_output) noexcept
{
    error_type error;

    json json_request;
    auth_request auth_request;

    if (!error)
        error = json::create(str_request, json_request);
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
            to_string("%1%2\t[%3]\t%4\t%5\t%6"_s, msg, log_entry == log_entry_first ? ""_s : "\r\n"_s,
                string_view(time_str), string_view(addr_str), string_view(user_str), string_view(type_str), string_view(error_str));
            
            file log;
            ICY_ERROR(log.open(m_config.file_path, file_access::app, file_open::open_always, file_share::read | file_share::write));
            ICY_ERROR(log.append(msg.bytes().data(), msg.bytes().size()));
        }
    }
    ICY_ERROR(auth_response.to_json(json_output));
    return error;
}