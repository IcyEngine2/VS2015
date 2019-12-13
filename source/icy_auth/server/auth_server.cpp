#include <icy_engine/icy_config.hpp>
#include <icy_module/icy_server.hpp>
#include <icy_engine/icy_win32.hpp>

using namespace icy;

error_type server_config_dbase::initialize(const const_array_view<string_view> keys) noexcept
{
    capacity.clear();
    ICY_ERROR(capacity.reserve(keys.size()));
    for (auto&& key : keys)
    {
        string new_str;
        ICY_ERROR(to_string(key, new_str));
        ICY_ERROR(capacity.insert(std::move(new_str), 0_z));
    }
    return {};
}
error_type server_config_dbase::from_json(const json& input) noexcept
{
    config config;

    ICY_ERROR(config.bind(key::file_path, json_type_string(), config::required));
    ICY_ERROR(config.bind(key::file_size, json_type_integer(0), config::required));
    ICY_ERROR(config(input, hash64(key::file_path), file_path));
    ICY_ERROR(config(input, hash64(key::file_size), file_size));
    file_size *= 1_mb;

    for (auto&& pair : capacity)
    {
        string str;
        ICY_ERROR(to_string("%1 %2"_s, str, key::capacity, string_view(pair.key)));
        ICY_ERROR(config.bind(str, json_type_integer(0), config::required));
        ICY_ERROR(config(input, hash64(str), pair.value));
    }
    return {};
}
error_type server_config_dbase::to_json(json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key::file_size, json_type_integer(file_size / 1_mb)));
    ICY_ERROR(output.insert(key::file_path, file_path));
    for (auto&& pair : capacity)
    {
        string str;
        ICY_ERROR(to_string("%1 %2"_s, str, key::capacity, string_view(pair.key)));
        ICY_ERROR(output.insert(str, json_type_integer(pair.value)));
    }
    return {};
}
error_type server_config_dbase::copy(const server_config_dbase& src, server_config_dbase& dst) noexcept
{
    dst.file_size = src.file_size;
    ICY_ERROR(to_string(src.file_path, dst.file_path));
    dst.capacity.clear();
    for (auto&& pair : src.capacity)
    {
        string str;
        ICY_ERROR(to_string("%1 %2"_s, str, key::capacity, string_view(pair.key)));
        auto value = size_t(pair.value);
        ICY_ERROR(dst.capacity.insert(std::move(str), std::move(value)));
    }
    return {};
}

error_type server_config_network::from_json(const json& input) noexcept
{
    config config;

    ICY_ERROR(config.bind(key::file_path, json_type_string(), config::optional));
    ICY_ERROR(config.bind(key::file_size, json_type_integer(0), config::optional));
    ICY_ERROR(config(input, hash64(key::file_path), file_path));
    ICY_ERROR(config(input, hash64(key::file_size), file_size));
    file_size *= 1_mb;

    if (const auto json_http = input.find(key::http))
    {
        ICY_ERROR(http.from_json(*json_http));
    }
    else
    {
        return make_config_error(config_error_code::is_required);
    }

    if (const auto json_errors = input.find(key::errors))
    {
        if (json_errors->type() != json_type::array)
            return make_config_error(config_error_code::invalid_type);

        if (json_errors->size() == 0)
        {
            any_code = true;
        }
        else
        {
            for (auto&& json_error : json_errors->vals())
            {
                json_type_integer code = 0;
                if (const auto error = json_error.get(code))
                    return make_config_error(config_error_code::invalid_value);

                if (code)
                {
                    string str;
                    if (const auto error = to_string(error_type(uint32_t(code), source), str))
                    {
                        if (error == make_stdlib_error(std::errc::invalid_argument))
                            return make_config_error(config_error_code::invalid_value);
                        ICY_ERROR(error);
                    }
                }
                ICY_ERROR(codes.push_back(uint32_t(code)));
            }
        }
    }
    return {};
}
error_type server_config_network::to_json(icy::json& output) const noexcept
{
    output = json_type::object;

    string str_file_path;
    ICY_ERROR(to_string(file_path, str_file_path));
    ICY_ERROR(output.insert(key::file_size, json_type_integer(file_size / 1_mb)));
    ICY_ERROR(output.insert(key::file_path, std::move(str_file_path)));

    json json_http;
    ICY_ERROR(http.to_json(json_http));
    ICY_ERROR(output.insert(key::http, std::move(json_http)));

    json json_errors = json_type::array;
    for (auto&& code : codes)
        ICY_ERROR(json_errors.push_back(json_type_integer(code)));

    if (!codes.empty())
    {
        ICY_ERROR(output.insert(key::errors, std::move(json_errors)));
    }
    return {};
}
error_type server_config_network::copy(const server_config_network& src, server_config_network& dst) noexcept
{
    dst.file_size = src.file_size;
    ICY_ERROR(to_string(src.file_path, dst.file_path));
    ICY_ERROR(dst.codes.assign(src.codes));
    ICY_ERROR(http_config::copy(src.http, dst.http));
    dst.source = src.source;
    dst.any_code = src.any_code;
    return {};
}
error_type server_network_thread::run() noexcept
{
    http_thread http(m_system.m_http);
    auto exit = false;
    while (!exit)
    {
        http_event event;
        ICY_ERROR(http.loop(event, exit));
        if (event.type == network_request_type::recv)
        {
            ICY_ERROR(process(http, event));
        }
    }
    return {};
}
error_type server_network_thread::process(http_thread& http, http_event& event) noexcept
{
    http_response http_response;
    string output;
    http_response.herror = http_error::bad_request;

    if (!event.error
        && event.request.type == http_request_type::post
        && event.request.content == http_content_type::application_json)
    {
        json request, response;
        if (const auto error = json::create(event.request.body, request))
        {
            if (error == make_stdlib_error(std::errc::illegal_byte_sequence))
                ;
            else
                ICY_ERROR(error);
        }
        if (const auto error = m_system.exec(request, response, event.address()))
        {
            if (error.source == m_system.m_config.source)
                ;
            else
                ICY_ERROR(error);
        }
        else
        {
            http_response.herror = http_error::success;
        }
        http_response.type = http_content_type::application_json;
        ICY_ERROR(to_string(response, output));
    }
    ICY_ERROR(http.reply(event, http_response, output.ubytes()));
    return {};
}

error_type server_network_system::initialize(const server_config_network& config, const size_t cores) noexcept
{
    ICY_ERROR(wait());
    m_threads.clear();

    ICY_ERROR(server_config_network::copy(config, m_config));
    ICY_ERROR(m_threads.reserve(cores));
    for (auto k = 0; k < cores; ++k)
    {
        ICY_ERROR(m_threads.emplace_back(*this));
    }
    return {};
}
error_type server_network_system::query_log(const uint32_t code, log_entry& entry) const noexcept
{
    if (m_config.file_size && !m_config.file_path.empty())
    {
        auto valid_code = false;
        if (m_config.any_code)
        {
            valid_code = true;
        }
        else
        {
            valid_code = std::find(m_config.codes.begin(), m_config.codes.end(), code) != m_config.codes.end();
        }
        if (valid_code)
        {
            file_info info;
            info.initialize(m_config.file_path);
            if (info.size < m_config.file_size)
                entry = info.size ? log_entry::next : log_entry::first;
        }
    }
    return {};
}
error_type server_network_system::write_log(const string_view msg) const noexcept
{
    if (msg.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    file log;
    ICY_ERROR(log.open(m_config.file_path, file_access::app, file_open::open_always, file_share::read | file_share::write));
    ICY_ERROR(log.append(msg.bytes().data(), msg.bytes().size()));
    return {};
}
error_type server_network_system::wait() noexcept
{
    m_http.shutdown();
    for (auto&& thread : m_threads)
        ICY_ERROR(thread.wait());
    m_network.shutdown();
    return {};
}
error_type server_network_system::launch() noexcept
{
    ICY_ERROR(wait());
    ICY_ERROR(m_network.initialize());
    ICY_ERROR(m_http.launch(m_network, m_config.http));
    for (auto&& thread : m_threads)
        ICY_ERROR(thread.launch());
    return {};
}
error_type server_network_system::status(string& msg) const noexcept
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

error_type icy::server_console_loop(const event_type event_error, const event_type event_post, bool& exit, const clock_type::duration timeout) noexcept
{
    timer timer;
    ICY_ERROR(timer.initialize(SIZE_MAX, timeout));

    shared_ptr<event_loop> loop;
    ICY_ERROR(event_loop::create(loop, uint64_t(event_error) | event_type::global_timer));
    ICY_ERROR(icy::event::post(nullptr, event_type(event_post)));
    while (true)
    {
        event event;
        ICY_ERROR(loop->loop(event));
        if (event->type == event_type::global_quit)
        {
            exit = true;
            return {};
        }
        else if (event->type == event_error)
        {
            const auto error = event->data<error_type>();
            string str;
            if (error)
            {
                ICY_ERROR(to_string(" Error: (%1) code %2 - %3."_s, str, error.source, error.code, error));
                ICY_ERROR(console_write(str));
            }
            else
            {
                ICY_ERROR(console_write(". Success!"_s));
            }
            break;
        }
        else if (event->type == event_type::global_timer)
        {
            auto pair = event->data<timer::pair>();
            if (pair.timer == &timer)
            {
                ICY_ERROR(console_write("."_s));
            }
        }
    }
    return {};
}

/*
if (const auto json_types = input.find(key::types))
    {
        if (json_types->type() != json_type::array)
            return make_config_error(config_error_code::invalid_type);

        if (json_types->size() == 0)
        {
            any_type = true;
        }
        else
        {
            for (auto&& json_type : json_types->vals())
            {
                json_type_integer type = 0;
                json_type.get(type);
                if (!type)
                    return make_config_error(config_error_code::invalid_value);

                if (type)
                {
                    string str;
                    if (const auto error = type_to_string(uint32_t(type), str))
                    {
                        if (error == make_stdlib_error(std::errc::invalid_argument))
                            return make_config_error(config_error_code::invalid_value);
                        ICY_ERROR(error);
                    }
                }
                ICY_ERROR(types.push_back(uint32_t(type)));
            }
        }
    }


    json json_types = json_type::array;
    for (auto&& type : types)
        ICY_ERROR(json_types.push_back(json_type_integer(type)));

    if (!types.empty())
    {
        ICY_ERROR(output.insert(key::types, std::move(json_types)));
    }
*/