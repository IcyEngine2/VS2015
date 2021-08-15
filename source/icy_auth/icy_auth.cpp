#include <icy_auth/icy_auth.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/core/icy_queue.hpp>

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
template<typename T> static error_type auth_from_json(const icy::json& object, T& auth)
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
    string_view str_client;
    string_view str_server;
    ICY_ERROR(to_string(const_array_view<char>(buffer_client), str_client));
    ICY_ERROR(to_string(const_array_view<char>(buffer_server), str_server));
    ICY_ERROR(json.insert(auth_str_key_client_ticket, str_client));
    ICY_ERROR(json.insert(auth_str_key_server_ticket, str_server));
    return error_type();
}
error_type auth_response_msg_client_connect::to_json(json& json) const noexcept
{
    char buffer_client[base64_encode_size(sizeof(encrypted_client_connect))] = {};
    char buffer_module[base64_encode_size(sizeof(encrypted_module_connect))] = {};
    ICY_ERROR(base64_encode(encrypted_client_connect, buffer_client));
    ICY_ERROR(base64_encode(encrypted_module_connect, buffer_module));
    json = json_type::object;
    string_view str_client;
    string_view str_module;
    ICY_ERROR(to_string(const_array_view<char>(buffer_client), str_client));
    ICY_ERROR(to_string(const_array_view<char>(buffer_module), str_module));
    ICY_ERROR(json.insert(auth_str_key_client_ticket, str_client));
    ICY_ERROR(json.insert(auth_str_key_module_ticket, str_module));
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
    ICY_ERROR(to_string(const_array_view<char>(bytes), str));
    output = std::move(str);
    return error_type();
}
error_type auth_request_msg_client_ticket::to_json(json& output) const noexcept
{
    char bytes[base64_encode_size(sizeof(encrypted_time))] = {};
    ICY_ERROR(base64_encode(encrypted_time, bytes));
    string str;
    ICY_ERROR(to_string(const_array_view<char>(bytes), str));
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
    string_view str_password;
    ICY_ERROR(to_string(const_array_view<char>(buffer_password), str_password));
    ICY_ERROR(output.insert(auth_str_key_password, str_password));
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
    string_view str_ticket;
    ICY_ERROR(to_string(const_array_view<char>(buffer_ticket), str_ticket));
    ICY_ERROR(output.insert(auth_str_key_server_ticket, str_ticket));
    ICY_ERROR(output.insert(auth_str_key_module, json_type_integer(module)));
    return error_type();
}

ICY_STATIC_NAMESPACE_BEG
struct internal_message
{
    auth_request_type type = auth_request_type::none;
    guid guid;
    uint64_t module = 0;
    uint64_t username = 0;
    crypto_key password;
    string address;
    auth_clock::duration timeout = {};
    crypto_msg<auth_client_ticket_server> encrypted_server_ticket;
};
class auth_system_data : public auth_system
{
public:
    error_type initialize(const string_view hostname) noexcept;
    ~auth_system_data() noexcept
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
private:
    error_type exec() noexcept override;
    error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type client_create(const guid guid, const uint64_t username, const crypto_key& password) noexcept override;
    error_type client_ticket(const guid guid, const uint64_t username, const crypto_key& password) noexcept override;
    error_type client_connect(const guid guid, const uint64_t username, const crypto_key& password, const uint64_t module, const crypto_msg<auth_client_ticket_server>& encrypted_server_ticket) noexcept override;
    error_type module_create(const guid guid, const uint64_t module) noexcept override;
    error_type module_update(const guid guid, const uint64_t module, const string_view address, const auth_clock::duration timeout, const crypto_key& password) noexcept override;
private:
    sync_handle m_sync;
    array<network_address> m_address;
    shared_ptr<event_thread> m_thread;
};
ICY_STATIC_NAMESPACE_END

error_type auth_system_data::initialize(const string_view hostname) noexcept
{
    ICY_ERROR(m_sync.initialize());

    const auto it = hostname.find(":"_s);
    if (it == hostname.end())
        return make_stdlib_error(std::errc::invalid_argument);
    
    const auto addr = string_view(hostname.begin(), it);
    const auto port = string_view(it + 1, hostname.end());

    uint32_t port_index = 0;
    ICY_ERROR(to_value(port, port_index));
    if (port_index < 0x400 || port_index >= 0xFFFF)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(network_address::query(m_address, addr, port));

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    const uint64_t event_types = 0
        | event_type::system_internal
        | event_type::network_disconnect
        | event_type::network_recv;

    filter(event_types);
    return error_type();
}
error_type auth_system_data::exec() noexcept
{
    shared_ptr<network_system_http_client> network;


    using pair_type = std::pair<auth_request, crypto_key>;
    icy::mpsc_queue<pair_type> requests_queue;
    pair_type current_request;
    
    const auto func = [&]
    {
        if (network)
            return error_type();

        http_request hrequest;
        hrequest.type = http_request_type::post;
        hrequest.content = http_content_type::application_json;

        icy::json json;
        ICY_ERROR(current_request.first.to_json(json));
        string str;
        ICY_ERROR(to_string(json, str));
        ICY_ERROR(hrequest.body.assign(str.ubytes()));
        for (auto&& addr : m_address)
        {
            if (create_network_http_client(network, addr, hrequest, network_default_timeout, 0x1000) == error_type())
                break;
        }
        if (!network)
        {
            auth_event new_event;
            new_event.type = current_request.first.type;
            new_event.guid = current_request.first.guid;
            new_event.error = make_stdlib_error(std::errc::host_unreachable);
            ICY_ERROR(event::post(this, auth_event_type, std::move(new_event)));
            return error_type();
        }
        ICY_ERROR(network->thread().launch());
        ICY_ERROR(network->thread().rename("Auth HTTP Thread"_s));
        return error_type();
    };

    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<internal_message>();

                auth_request new_request;
                new_request.guid = event_data.guid;
                new_request.time = auth_clock::now();
                new_request.username = event_data.username;
                new_request.type = event_data.type;

                switch (event_data.type)
                {
                case auth_request_type::client_create:
                {
                    auth_request_msg_client_create auth_msg;
                    auth_msg.password = event_data.password;
                    ICY_ERROR(auth_msg.to_json(new_request.message));
                    break;
                }
                case auth_request_type::client_ticket:
                {
                    auth_request_msg_client_ticket auth_msg;
                    auth_msg.encrypted_time = crypto_msg<std::chrono::system_clock::time_point>(event_data.password, new_request.time);
                    ICY_ERROR(auth_msg.to_json(new_request.message));
                    break;
                }
                case auth_request_type::client_connect:
                {
                    auth_request_msg_client_connect auth_msg;
                    auth_msg.module = event_data.module;
                    auth_msg.encrypted_server_ticket = event_data.encrypted_server_ticket;
                    ICY_ERROR(auth_msg.to_json(new_request.message));
                    break;
                }
                case auth_request_type::module_create:
                {
                    auth_request_msg_module_create auth_msg;
                    auth_msg.module = event_data.module;
                    ICY_ERROR(auth_msg.to_json(new_request.message));
                    break;
                }
                case auth_request_type::module_update:
                {
                    auth_request_msg_module_update auth_msg;
                    auth_msg.module = event_data.module;
                    auth_msg.password = event_data.password;
                    auth_msg.timeout = event_data.timeout;
                    ICY_ERROR(copy(event_data.address, auth_msg.address));
                    ICY_ERROR(auth_msg.to_json(new_request.message));
                    break;
                }
                }
                ICY_ERROR(requests_queue.push(std::make_pair(std::move(new_request), event_data.password)));
                if (network)
                    continue;

                if (!current_request.first.guid)
                {
                    if (requests_queue.pop(current_request))
                        ICY_ERROR(func());
                }
            }
            else if (event->type == event_type::network_disconnect)
            {
                if (shared_ptr<network_system_http_client>(event->source).get() != network.get())
                    continue;
                network = nullptr;
                const auto& event_data = event->data<network_event>();
                auth_event new_event;
                new_event.type = current_request.first.type;
                new_event.guid = current_request.first.guid;
                new_event.error = event_data.error;
                ICY_ERROR(event::post(this, auth_event_type, std::move(new_event)));
                current_request = pair_type();
            }
            else if (event->type == event_type::network_recv)
            {
                if (shared_ptr<network_system_http_client>(event->source).get() != network.get())
                    continue;
                network = nullptr;
                const auto& event_data = event->data<network_event>();
                if (current_request.first.guid)
                {
                    const auto& request = current_request.first;
                    auth_event new_event;
                    new_event.type = request.type;
                    new_event.guid = request.guid;
                    if (event_data.error)
                    {
                        new_event.error = event_data.error;
                    }
                    else if (event_data.http.response)
                    {
                        const auto& body = event_data.http.response->body;
                        icy::json json;
                        
                        auth_response response;
                        response.guid = request.guid;
                        response.type = request.type;

                        string_view body_str;
                        if (!new_event.error) new_event.error = to_string(body, body_str);
                        if (!new_event.error) new_event.error = to_value(body_str, json);
                        if (!new_event.error) new_event.error = response.from_json(json);
                        if (!new_event.error)
                        {
                            if (response.time < request.time - network_default_timeout ||
                                response.time > request.time + network_default_timeout)
                                new_event.error = make_auth_error(auth_error_code::timeout);
                        }
                        if (response.error != auth_error_code::none)
                            new_event.error = make_auth_error(response.error);
                        
                        switch (response.type)
                        {
                        case auth_request_type::client_create:
                            break;

                        case auth_request_type::client_ticket:
                        {
                            auth_response_msg_client_ticket data;
                            if (!new_event.error) new_event.error = data.from_json(response.message);
                            if (!new_event.error)
                            {
                                if (const auto error = data.encrypted_client_ticket.decode(current_request.second, new_event.client_ticket))
                                    new_event.error = make_auth_error(auth_error_code::client_invalid_password);
                                else
                                    new_event.encrypted_server_ticket = data.encrypted_server_ticket;
                            }
                            break;
                        }
                        case auth_request_type::client_connect:
                        {
                            auth_response_msg_client_connect data;
                            if (!new_event.error) new_event.error = data.from_json(response.message);
                            if (!new_event.error)
                            {
                                if (const auto error = data.encrypted_client_connect.decode(current_request.second, new_event.client_connect))
                                    new_event.error = make_auth_error(auth_error_code::client_invalid_password);
                                else
                                    new_event.encrypted_module_connect = data.encrypted_module_connect;
                            }
                            break;
                        }
                        case auth_request_type::module_create:
                            break;

                        case auth_request_type::module_update:
                            break;
                        }
                    }
                    else
                    {
                        new_event.error = make_unexpected_error();
                    }
                    ICY_ERROR(event::post(this, auth_event_type, std::move(new_event)));
                    current_request = pair_type();
                }
                else
                {
                    auth_event new_event;
                    new_event.error = make_unexpected_error();
                    ICY_ERROR(event::post(this, auth_event_type, std::move(new_event)));                    
                    return error_type();
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type auth_system_data::client_create(const guid guid, const uint64_t username, const crypto_key& password) noexcept
{
    if (!guid || !username)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.guid = guid;
    msg.type = auth_request_type::client_create;
    msg.username = username;
    msg.password = password;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type auth_system_data::client_ticket(const guid guid, const uint64_t username, const crypto_key& password) noexcept
{
    if (!guid || !username)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.guid = guid;
    msg.type = auth_request_type::client_ticket;
    msg.username = username;
    msg.password = password;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type auth_system_data::client_connect(const guid guid, const uint64_t username, const crypto_key& password, const uint64_t module, const crypto_msg<auth_client_ticket_server>& encrypted_server_ticket) noexcept
{
    if (!guid || !username || !module)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.guid = guid;
    msg.type = auth_request_type::client_connect;
    msg.username = username;
    msg.password = password;
    msg.encrypted_server_ticket = encrypted_server_ticket;
    msg.module = module;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type auth_system_data::module_create(const guid guid, const uint64_t module) noexcept
{
    if (!guid || !module)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.guid = guid;
    msg.type = auth_request_type::module_create;
    msg.module = module;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type auth_system_data::module_update(const guid guid, const uint64_t module, const string_view address, const auth_clock::duration timeout, const crypto_key& password) noexcept
{
    if (!guid || !module || !timeout.count())
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.guid = guid;
    msg.type = auth_request_type::module_update;
    msg.module = module;
    msg.password = password;
    ICY_ERROR(copy(address, msg.address));
    msg.timeout = timeout;
    return event_system::post(nullptr, event_type::system_internal, std::move(msg));
}
error_type icy::create_auth_system(shared_ptr<auth_system>& system, const string_view hostname) noexcept
{
    shared_ptr<auth_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(hostname));
    system = std::move(new_system);
    return error_type();
}

extern const event_type icy::auth_event_type = icy::next_event_user();