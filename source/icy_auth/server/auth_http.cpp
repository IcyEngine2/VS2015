#include "auth_http.hpp"
#include "auth_database.hpp"
#include <icy_engine/icy_json.hpp>
#include <icy_engine/icy_file.hpp>
#include <icy_module/icy_auth.hpp>
#include <icy_lib/icy_crypto.hpp>

using namespace icy;

error_type auth_network_system::exec(const json& input, json& output, const network_address& address) noexcept
{
    auth_request auth_request;
    auto error = auth_request.from_json(input);
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
        auth_response.error = static_cast<auth_error_code>(error.code);
        auto entry = log_entry::none;
        ICY_ERROR(query_log(error.code, entry));
        if (entry != log_entry::none)
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
            to_string("%1%2\t[%3]\t%4\t%5\t%6"_s, msg, entry == log_entry::first ? ""_s : "\r\n"_s,
                string_view(time_str), string_view(addr_str), string_view(user_str), string_view(type_str), string_view(error_str));
            ICY_ERROR(write_log(msg));
        }
    }
    ICY_ERROR(auth_response.to_json(output));
    return error;
}