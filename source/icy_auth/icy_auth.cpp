#include <icy_auth/icy_auth.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_json.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
constexpr auto auth_version = 2021'08'09'0002;
constexpr auto auth_str_key_version = "Version"_s;
constexpr auto auth_str_key_type = "Type"_s;
constexpr auto auth_str_key_time = "Time"_s;
constexpr auto auth_str_key_timeout = "Timeout"_s;
constexpr auto auth_str_key_guid = "GUID"_s;
constexpr auto auth_str_key_error_code = "Error Code"_s;
constexpr auto auth_str_key_error_text = "Error Text"_s;
constexpr auto auth_str_key_username = "Username"_s;
constexpr auto auth_str_key_password = "Password"_s;
constexpr auto auth_str_key_userguid = "Userguid"_s;
constexpr auto auth_str_key_address = "Address"_s;
constexpr auto auth_str_key_module = "Module"_s;
constexpr auto auth_str_key_message = "Message"_s;
constexpr auto auth_str_key_server_ticket = "Server Ticket"_s;
constexpr auto auth_str_key_client_ticket = "Client Ticket"_s;
constexpr auto auth_str_key_module_ticket = "Module Ticket"_s;
constexpr std::pair<string_view, auth_request_type> auth_str_request_map[] =
{
    { "Client Create"_s, auth_request_type::client_create },
    { "Client Ticket"_s, auth_request_type::client_ticket },
    { "Client Connect"_s, auth_request_type::client_connect  },
    { "Module Create"_s, auth_request_type::module_create },
    { "Module Update"_s, auth_request_type::module_update },
};
static error_type auth_error_to_string(const unsigned integer_code, const string_view, string& str) noexcept
{
    const auto code = auth_error_code(integer_code);
    string_view prefix;
    switch (auth_error_category(code))
    {
    case auth_error_code::invalid_json:
        prefix = "Invalid JSON"_s;
        break;

    case auth_error_code::client_access:
        prefix = "Client access denied"_s;
        break;

    case auth_error_code::module_access:
        prefix = "Module access denied"_s;
        break;
        
    case auth_error_code::timeout:
        return to_string("Auth request timeout"_s, str);
    }

    str.clear();
    ICY_ERROR(str.append(prefix));
    if (code != auth_error_category(code))
        ICY_ERROR(str.append(": "_s));
    
    switch (code)
    {
    case auth_error_code::invalid_json: 
        return str.append("root object"_s);
    case auth_error_code::invalid_json_guid:
        return str.append("message guid"_s);
    case auth_error_code::invalid_json_error:
        return str.append("error code"_s);
    case auth_error_code::invalid_json_message:
        return str.append("message body"_s);
    case auth_error_code::invalid_json_password:
        return str.append("password"_s);
    case auth_error_code::invalid_json_time:
        return str.append("time string format"_s);
    case auth_error_code::invalid_json_type:
        return str.append("request type"_s);
    case auth_error_code::invalid_json_username:
        return str.append("username"_s);
    case auth_error_code::invalid_json_version:
        return str.append("protocol version"_s);

    case auth_error_code::client_access:
        return {};

    case auth_error_code::client_max_capacity:
        return str.append("too many users"_s);        
    case auth_error_code::client_already_exists:
        return str.append("user already exists"_s);
    case auth_error_code::client_not_found:
        return str.append("user not found"_s);
    case auth_error_code::client_invalid_password:
        return str.append("invalid password"_s);
    case auth_error_code::client_invalid_ticket:
        return str.append("invalid ticket"_s);
    case auth_error_code::client_expired_ticket:
        return str.append("expired ticket"_s);
    case auth_error_code::client_invalid_module:
        return str.append("invalid module"_s);

    case auth_error_code::module_access:
        return {};
    case auth_error_code::module_max_capacity:
        return str.append("too many modules"_s);
    case auth_error_code::module_already_exists:
        return str.append("module already exists"_s);
    case auth_error_code::module_not_found:
        return str.append("module not found"_s);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
template<typename T> static error_type auth_from_json(const json& object, T& auth)
{
    if (object.type() != json_type::object)
        return make_auth_error(auth_error_code::invalid_json);

    json_type_integer version = 0;
    object.get(auth_str_key_version, version);
    if (version != auth_version)
        return make_auth_error(auth_error_code::invalid_json_version);

    auth.type = auth_request_type::none;
    const auto str_type = object.get(auth_str_key_type);
    for (auto&& pair : auth_str_request_map)
    {
        if (pair.first == str_type)
        {
            auth.type = pair.second;
            break;
        }
    }
    if (auth.type == auth_request_type::none)
        return make_auth_error(auth_error_code::invalid_json_type);

    const auto str_time = object.get(auth_str_key_time);
    if (const auto error = to_value(str_time, auth.time, false))
    {
        if (error == make_stdlib_error(std::errc::illegal_byte_sequence))
            return make_auth_error(auth_error_code::invalid_json_time);
    }

    const auto str_guid = object.get(auth_str_key_guid);
    if (const auto error = to_value(str_guid, auth.guid))
    {
        if (error == make_stdlib_error(std::errc::illegal_byte_sequence))
            return make_auth_error(auth_error_code::invalid_json_guid);
    }

    const auto json_message = object.find(auth_str_key_message);
    if (json_message)
        ICY_ERROR(copy(*json_message, auth.message));

    return error_type();
}
ICY_STATIC_NAMESPACE_END
const error_source icy::error_source_auth = register_error_source("auth"_s, auth_error_to_string);
error_type icy::to_string(const auth_request_type type, string& str) noexcept
{
    for (auto&& pair : auth_str_request_map)
    {
        if (pair.second == type)
            return to_string(pair.first, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}

error_type auth_request::to_json(json& output) const noexcept
{
    output = json_type::object;
    
    string str_time;
    string str_guid;
    string str_type;
    ICY_ERROR(to_string(time, str_time, false));
    ICY_ERROR(to_string(guid, str_guid));
    ICY_ERROR(to_string(type, str_type));

    ICY_ERROR(output.insert(auth_str_key_version, json_type_integer(auth_version)));
    ICY_ERROR(output.insert(auth_str_key_time, std::move(str_time)));
    ICY_ERROR(output.insert(auth_str_key_guid, std::move(str_guid)));
    ICY_ERROR(output.insert(auth_str_key_type, std::move(str_type)));
    if (username)
    {
        ICY_ERROR(output.insert(auth_str_key_username, json_type_integer(username)));
    }
    if (message.type() != json_type::none)
    {
        json json_message;
        ICY_ERROR(copy(message, json_message));
        ICY_ERROR(output.insert(auth_str_key_message, std::move(json_message)));
    }
    return error_type();
}
error_type auth_request::from_json(const json& input) noexcept
{
    ICY_ERROR(auth_from_json(input, *this));
    json_type_integer json_username = 0;
    input.get(auth_str_key_username, json_username);
    if (json_username)
        username = uint64_t(json_username);
    return error_type();
}

error_type auth_response::to_json(json& output) const noexcept
{
    output = json_type::object;

    string str_time;
    string str_guid;
    string str_type;
    ICY_ERROR(to_string(time, str_time, false));
    ICY_ERROR(to_string(guid, str_guid));
    ICY_ERROR(to_string(type, str_type));

    ICY_ERROR(output.insert(auth_str_key_version, json_type_integer(auth_version)));
    ICY_ERROR(output.insert(auth_str_key_time, std::move(str_time)));
    ICY_ERROR(output.insert(auth_str_key_guid, std::move(str_guid)));
    ICY_ERROR(output.insert(auth_str_key_type, std::move(str_type)));
    ICY_ERROR(output.insert(auth_str_key_error_code, json_type_integer(error)));
    if (error != auth_error_code::none)
    {
        string str_error_text;
        ICY_ERROR(to_string(make_auth_error(error), str_error_text));
        ICY_ERROR(output.insert(auth_str_key_error_text, std::move(str_error_text)));
    }

    if (message.type() != json_type::none)
    {
        json json_message;
        ICY_ERROR(copy(message, json_message));
        ICY_ERROR(output.insert(auth_str_key_message, std::move(json_message)));
    }
    return error_type();
}
error_type auth_response::from_json(const json& input) noexcept
{
    const auto old_guid = guid;
    const auto old_type = type;

    ICY_ERROR(auth_from_json(input, *this));

    if (guid != old_guid)
        return make_auth_error(auth_error_code::invalid_json_guid);
    if (type != old_type)
        return make_auth_error(auth_error_code::invalid_json_type);
    json_type_integer error_code = 0;
    if (input.get(auth_str_key_error_code, error_code))
        return make_auth_error(auth_error_code::invalid_json_error);

    if (error_code)
    {
        string error_text;
        if (const auto to_string_error = to_string(make_auth_error(auth_error_code(error_code)), error_text))
        {
            if (to_string_error == make_stdlib_error(std::errc::invalid_argument))
                return make_auth_error(auth_error_code::invalid_json_error);
            ICY_ERROR(to_string_error);
        }
        error = auth_error_code(error_code);
    }
    return error_type();
}

error_type auth_response_msg_client_ticket::to_json(json& json) const noexcept
{
    char buffer_client[base64_encode_size(sizeof(encrypted_client_ticket))] = {};
    char buffer_server[base64_encode_size(sizeof(encrypted_server_ticket))] = {};
    ICY_ERROR(base64_encode(encrypted_client_ticket, buffer_client));
    ICY_ERROR(base64_encode(encrypted_server_ticket, buffer_server));
    json = json_type::object;
    ICY_ERROR(json.insert(auth_str_key_client_ticket, string_view(buffer_client, _countof(buffer_client))));
    ICY_ERROR(json.insert(auth_str_key_server_ticket, string_view(buffer_server, _countof(buffer_server))));
    return error_type();
}
error_type auth_response_msg_client_connect::to_json(json& json) const noexcept
{
    char buffer_client[base64_encode_size(sizeof(encrypted_client_connect))] = {};
    char buffer_module[base64_encode_size(sizeof(encrypted_module_connect))] = {};
    ICY_ERROR(base64_encode(encrypted_client_connect, buffer_client));
    ICY_ERROR(base64_encode(encrypted_module_connect, buffer_module));
    json = json_type::object;
    ICY_ERROR(json.insert(auth_str_key_client_ticket, string_view(buffer_client, _countof(buffer_client))));
    ICY_ERROR(json.insert(auth_str_key_module_ticket, string_view(buffer_module, _countof(buffer_module))));
    return error_type();
}
error_type auth_response_msg_client_ticket::from_json(const json& response) noexcept
{
    if (const auto error = base64_decode(response.get(auth_str_key_client_ticket), encrypted_client_ticket))
        return make_auth_error(auth_error_code::invalid_json_message);
    if (const auto error = base64_decode(response.get(auth_str_key_server_ticket), encrypted_server_ticket))
        return make_auth_error(auth_error_code::invalid_json_message);
    return error_type();
}
error_type auth_response_msg_client_connect::from_json(const json& response) noexcept
{
    if (const auto error = base64_decode(response.get(auth_str_key_client_ticket), encrypted_client_connect))
        return make_auth_error(auth_error_code::invalid_json_message);
    if (const auto error = base64_decode(response.get(auth_str_key_module_ticket), encrypted_module_connect))
        return make_auth_error(auth_error_code::invalid_json_message);
    return error_type();
}




error_type auth_request_msg_client_create::from_json(const json& input) noexcept
{
    if (const auto error = base64_decode(input.get(), password))
        return make_auth_error(auth_error_code::invalid_json_message);
    return error_type();
}
error_type auth_request_msg_client_ticket::from_json(const json& input) noexcept
{
    if (const auto error = base64_decode(input.get(), encrypted_time))
        return make_auth_error(auth_error_code::invalid_json_message);
    return error_type();
}
error_type auth_request_msg_client_connect::from_json(const json& input) noexcept
{
    json_type_integer json_module = 0;
    input.get(auth_str_key_module, json_module);
    if (!json_module)
        return make_auth_error(auth_error_code::invalid_json_message);
    module = uint64_t(json_module);

    const auto base64 = input.get(auth_str_key_server_ticket);
    if (const auto error = base64_decode(base64, encrypted_server_ticket))
        return make_auth_error(auth_error_code::invalid_json_message);

    return error_type();
}
error_type auth_request_msg_module_create::from_json(const json& input) noexcept
{
    json_type_integer json_module = 0;
    input.get(json_module);
    if (!json_module)
        return make_auth_error(auth_error_code::invalid_json_message);
    
    module = uint64_t(json_module);
    return error_type();
}
error_type auth_request_msg_module_update::from_json(const json& input) noexcept
{
    if (const auto error = base64_decode(input.get(auth_str_key_password), array_view<uint8_t>(password.data(), password.size())))
        return make_auth_error(auth_error_code::invalid_json_message);

    ICY_ERROR(to_string(input.get(auth_str_key_address), address));
    
    json_type_integer timeout_ms = 0;
    input.get(auth_str_key_timeout, timeout_ms);
    if (!timeout_ms)
        return make_auth_error(auth_error_code::invalid_json_message);
    timeout = std::chrono::milliseconds(timeout_ms);

    json_type_integer json_module = 0;
    input.get(auth_str_key_module, json_module);
    if (!json_module)
        return make_auth_error(auth_error_code::invalid_json_message);
    module = uint64_t(json_module);    

    return error_type();
}
error_type auth_request_msg_client_create::to_json(json& output) const noexcept
{
    char bytes[base64_encode_size(sizeof(password))] = {};
    ICY_ERROR(base64_encode(password, bytes));
    string str;
    ICY_ERROR(to_string(string_view(bytes, _countof(bytes)), str));
    output = std::move(str);
    return error_type();
}
error_type auth_request_msg_client_ticket::to_json(json& output) const noexcept
{
    char bytes[base64_encode_size(sizeof(encrypted_time))] = {};
    ICY_ERROR(base64_encode(encrypted_time, bytes));
    string str;
    ICY_ERROR(to_string(string_view(bytes, _countof(bytes)), str));
    output = std::move(str);
    return error_type();
}
error_type auth_request_msg_module_create::to_json(json& output) const noexcept
{
    output = json_type_integer(module);
    return error_type();
}
error_type auth_request_msg_module_update::to_json(json& output) const noexcept
{
    output = json_type::object;
    char buffer_password[base64_encode_size(sizeof(password))] = {};
    ICY_ERROR(base64_encode(password, buffer_password));
    ICY_ERROR(output.insert(auth_str_key_password, string_view(buffer_password, _countof(buffer_password))));
    ICY_ERROR(output.insert(auth_str_key_timeout, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
    ICY_ERROR(output.insert(auth_str_key_module, json_type_integer(module)));
    ICY_ERROR(output.insert(auth_str_key_address, address));
    return error_type();
}
error_type auth_request_msg_client_connect::to_json(json& output) const noexcept
{
    output = json_type::object;
    char buffer_ticket[base64_encode_size(sizeof(encrypted_server_ticket))] = {};
    ICY_ERROR(base64_encode(encrypted_server_ticket, buffer_ticket));
    ICY_ERROR(output.insert(auth_str_key_server_ticket, string_view(buffer_ticket, _countof(buffer_ticket))));
    ICY_ERROR(output.insert(auth_str_key_module, json_type_integer(module)));
    return error_type();
}