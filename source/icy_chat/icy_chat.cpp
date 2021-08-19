#include <icy_chat/icy_chat.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/core/icy_queue.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
static const uint64_t chat_version = 2021'08'16'0004;
static const auto chat_key_module = "module"_s;
static const auto chat_key_version = "version"_s;
static const auto chat_key_type = "type"_s;
static const auto chat_key_text = "text"_s;
static const auto chat_key_room = "room"_s;
static const auto chat_key_time = "time"_s;
static const auto chat_key_user = "user"_s;
static const auto chat_key_guid = "guid"_s;
static const auto chat_key_error = "error"_s;

static error_type make_chat_error(const chat_error_code code) noexcept 
{
    return error_type(uint32_t(code), error_source_chat);
}
static error_type chat_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
    switch (chat_error_code(code))
    {
    case chat_error_code::none:
        return error_type();

    case chat_error_code::invalid_version:
        return copy("Invalid version"_s, str);
    case chat_error_code::invalid_utf_string:
        return copy("Invalid utf string"_s, str);
    case chat_error_code::invalid_json:
        return copy("Invalid json"_s, str);
    case chat_error_code::invalid_hostname:
        return copy("Invalid hostname"_s, str);
    case chat_error_code::invalid_message_size:
        return copy("Invalid message size"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
static string_view to_string(const chat_request_type type) noexcept
{
    switch (type)
    {
    case chat_request_type::user_send_text:
        return "userSendText"_s;
    case chat_request_type::user_update:
        return "userUpdate"_s;

    case chat_request_type::system_create_room:
        return "systemCreateRoom"_s;
    case chat_request_type::system_invite_user:
        return "systemInviteUser"_s;
    case chat_request_type::system_remove_user:
        return "systemRemoveUser"_s;
    }
    return string_view();
}
enum class internal_message_type
{
    none,
    connect,
    update,
    send,
};
struct internal_message
{
    internal_message_type type = internal_message_type::none;
    array<network_address> address;
    crypto_msg<auth_client_connect_module> encrypted_module_connect;   //  with module password
    crypto_key session;
    guid user;
    guid room;
    string text;
};
class chat_system_data : public chat_system
{
public:
    error_type initialize() noexcept;
    ~chat_system_data() noexcept
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
    error_type connect(const auth_client_connect_client& client, const crypto_msg<auth_client_connect_module>& module) noexcept override;
    error_type update() noexcept override;
    error_type send_to_user(const string_view text, const icy::guid& user) noexcept override;
    error_type send_to_room(const string_view text, const icy::guid& room) noexcept override;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
};
ICY_STATIC_NAMESPACE_END

error_type chat_request::to_json(json& output) const noexcept
{
    const auto& input = *this;
    output = json_type::object;

    char buffer[base64_encode_size(sizeof(crypto_msg<auth_client_connect_module>))] = {};
    ICY_ERROR(base64_encode(input.encrypted_module_connect, buffer));
    string_view str;
    to_string(const_array_view<char>(buffer), str);
    ICY_ERROR(output.insert(chat_key_module, str));

    string str_time;
    ICY_ERROR(to_string(input.time, str_time, false));

    if (input.user)
    {
        string str_user;
        ICY_ERROR(to_string(input.user, str_user));
        ICY_ERROR(output.insert(chat_key_user, str_user));
    }
    if (input.room)
    {
        string str_room;
        ICY_ERROR(to_string(input.room, str_room));
        ICY_ERROR(output.insert(chat_key_room, str_room));
    }
    if (input.guid)
    {
        string str_guid;
        ICY_ERROR(to_string(input.guid, str_guid));
        ICY_ERROR(output.insert(chat_key_guid, str_guid));
    }
    ICY_ERROR(output.insert(chat_key_time, str_time));
    ICY_ERROR(output.insert(chat_key_text, input.text));
    ICY_ERROR(output.insert(chat_key_type, ::to_string(input.type)));
    ICY_ERROR(output.insert(chat_key_version, chat_version));
    return error_type();
}
error_type chat_request::from_json(const json& input) noexcept
{
    auto& output = *this;
    if (input.type() != json_type::object)
        return make_chat_error(chat_error_code::invalid_json);

    json_type_integer version = 0;
    input.get(chat_key_version, version);
    if (version != chat_version)
        return make_chat_error(chat_error_code::invalid_version);

    output.type = chat_request_type::none;
    const auto str_type = input.get(chat_key_type);
    for (auto k = 1u; k < uint32_t(chat_request_type::_total); ++k)
    {
        if (::to_string(chat_request_type(k)) == str_type)
        {
            output.type = chat_request_type(k);
            break;
        }
    }
    if (output.type == chat_request_type::none)
        return make_chat_error(chat_error_code::invalid_type);

    const auto str_time = input.get(chat_key_time);
    if (const auto error = to_value(str_time, output.time, false))
        return make_chat_error(chat_error_code::invalid_json);

    const auto str_guid = input.get(chat_key_guid);
    const auto str_user = input.get(chat_key_user);
    const auto str_room = input.get(chat_key_room);

    if (!str_guid.empty())
    {
        if (const auto error = to_value(str_guid, output.guid))
            return make_chat_error(chat_error_code::invalid_json);
    }
    if (!str_user.empty())
    {
        if (const auto error = to_value(str_user, output.user))
            return make_chat_error(chat_error_code::invalid_json);
    }
    if (!str_room.empty())
    {
        if (const auto error = to_value(str_room, output.room))
            return make_chat_error(chat_error_code::invalid_json);
    }
    ICY_ERROR(copy(input.get(chat_key_text), output.text));

    const auto module_str = input.get(chat_key_module);
    if (module_str.bytes().size() == base64_encode_size(sizeof(crypto_msg<auth_client_connect_module>)))
    {
        uint8_t buffer[sizeof(crypto_msg<auth_client_connect_module>)] = {};
        ICY_ERROR(base64_decode(module_str, buffer));
        memcpy(&output.encrypted_module_connect, buffer, sizeof(buffer));
    }
    return error_type();
}

error_type chat_response::to_json(json& output) const noexcept
{
    const auto& input = *this;
    output = json_type::object;

    string str_time;
    ICY_ERROR(to_string(input.time, str_time, false));
    
    if (input.user) 
    {
        string str_user;
        ICY_ERROR(to_string(input.user, str_user));
        ICY_ERROR(output.insert(chat_key_user, str_user)); 
    }
    if (input.room)
    {
        string str_room;
        ICY_ERROR(to_string(input.room, str_room));
        ICY_ERROR(output.insert(chat_key_room, str_room));
    }
    if (input.guid)
    {
        string str_guid;
        ICY_ERROR(to_string(input.guid, str_guid));
        ICY_ERROR(output.insert(chat_key_guid, str_guid));
    }
    
    ICY_ERROR(output.insert(chat_key_error, uint32_t(input.error)));
    ICY_ERROR(output.insert(chat_key_time, str_time));
    ICY_ERROR(output.insert(chat_key_text, input.text));
    ICY_ERROR(output.insert(chat_key_version, chat_version));
    return error_type();
}
error_type chat_response::from_json(const json& input) noexcept
{
    auto& output = *this;
    if (input.type() != json_type::object)
        return make_chat_error(chat_error_code::invalid_json);

    json_type_integer version = 0;
    input.get(chat_key_version, version);
    if (version != chat_version)
        return make_chat_error(chat_error_code::invalid_version);

    const auto str_time = input.get(chat_key_time);
    if (const auto error = to_value(str_time, output.time, false))
        return make_chat_error(chat_error_code::invalid_json);

    const auto str_error = input.get(chat_key_error);
    uint32_t code = 0;
    if (const auto error = to_value(str_error, code))
        return make_chat_error(chat_error_code::invalid_json);

    output.error = chat_error_code(code);

    const auto str_guid = input.get(chat_key_guid);
    const auto str_user = input.get(chat_key_user);
    const auto str_room = input.get(chat_key_room);

    if (!str_guid.empty())
    {
        if (const auto error = to_value(str_guid, output.guid))
            return make_chat_error(chat_error_code::invalid_json);
    }
    if (!str_user.empty())
    {
        if (const auto error = to_value(str_user, output.user))
            return make_chat_error(chat_error_code::invalid_json);
    }
    if (!str_room.empty())
    {
        if (const auto error = to_value(str_room, output.room))
            return make_chat_error(chat_error_code::invalid_json);
    }
    ICY_ERROR(copy(input.get(chat_key_text), output.text));
    
    
    return error_type();
}

const error_source icy::error_source_chat = register_error_source("chat"_s, chat_error_to_string);
error_type chat_system_data::initialize() noexcept
{
    ICY_ERROR(m_sync.initialize());

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    const uint64_t event_types = 0
        | event_type::system_internal
        | event_type::network_disconnect
        | event_type::network_recv;

    filter(event_types);
    return error_type();
}
error_type chat_system_data::exec() noexcept
{

    crypto_msg<auth_client_connect_module> encrypted_module_connect;
    array<network_address> address;
    crypto_key session;
    shared_ptr<network_system_http_client> network;

    const auto reset = [this, &network](const error_type error)
    {
        network = nullptr;
        chat_event new_event;
        new_event.error = error;
        ICY_ERROR(event::post(this, chat_event_type, std::move(new_event)));
        return error_type();
    };

    mpsc_queue<chat_request> requests_queue;
    chat_request current_request;

    const auto func = [&]
    {
        http_request hrequest;
        hrequest.type = http_request_type::post;
        hrequest.content = http_content_type::application_json;

        icy::json json;
        ICY_ERROR(current_request.to_json(json));
        string str;
        ICY_ERROR(to_string(json, str));
        ICY_ERROR(hrequest.body.assign(str.ubytes()));
        if (!network)
        {
            for (auto&& addr : address)
            {
                if (create_network_http_client(network, addr, hrequest, network_default_timeout, network_default_buffer) == error_type())
                    break;
            }
            if (!network)
            {
                return reset(make_stdlib_error(std::errc::host_unreachable));
            }
            ICY_ERROR(network->thread().launch());
            ICY_ERROR(network->thread().rename("Chat HTTP Thread"_s));
        }
        else
        {
            ICY_ERROR(network->send(hrequest));
            ICY_ERROR(network->recv());
        }
        return error_type();
    };

    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<internal_message>();
                if (event_data.type == internal_message_type::connect)
                {
                    encrypted_module_connect = event_data.encrypted_module_connect;
                    address = std::move(event_data.address);
                    session = event_data.session;            
                }
                else if (address.empty())
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_hostname)));
                    continue;
                }
                else
                {
                    chat_request request;
                    request.time = auth_clock::now();
                    request.version = chat_version;
                    switch (event_data.type)
                    {
                    case internal_message_type::send:
                        request.type = chat_request_type::user_send_text;
                        break;
                    case internal_message_type::update:
                        request.type = chat_request_type::user_update;
                        break;
                    }
                    if (request.type == chat_request_type::none)
                    {
                        ICY_ERROR(reset(make_stdlib_error(std::errc::invalid_argument)));
                        continue;
                    }
                    request.user = event_data.user;
                    request.room = event_data.room;
                    request.guid = guid::create();
                    request.text = std::move(event_data.text);

                    ICY_ERROR(requests_queue.push(std::move(request)));
                    if (network)
                        continue;

                    if (!current_request.guid && requests_queue.pop(current_request))
                    {
                        ICY_ERROR(func());
                    }
                }
            }
            else if ((event->type & event_type::network_any) && network)
            {
                if (shared_ptr<event_system>(event->source) != network)
                    continue;

                const auto& event_data = event->data<network_event>();
                if (event_data.error)
                {
                    ICY_ERROR(reset(event_data.error));
                    continue;
                }
                if (event->type == event_type::network_disconnect)
                {
                    network = nullptr;
                    continue;
                }
               
                if (!event_data.http.request
                    || event_data.http.request->type != http_request_type::post
                    || event_data.http.request->content != http_content_type::application_json)
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_json)));
                    continue;
                }
                string_view body_str;
                if (const auto error = to_string(event_data.http.request->body, body_str))
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_utf_string)));
                    continue;
                }

                icy::json json;
                if (const auto error = to_value(body_str, json))
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_json)));
                    continue;
                }
                chat_response response;
                if (const auto error = response.from_json(json))
                {
                    ICY_ERROR(reset(error));
                    continue;
                }
                if (response.version != chat_version)
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_version)));
                    continue;
                }
                const auto type = current_request.type;
                current_request = chat_request();
                
                if (type == chat_request_type::user_update)
                {
                    chat_event new_event;
                    new_event.time = response.time;
                    new_event.text = std::move(response.text);
                    new_event.room = response.room;
                    new_event.user = response.user;
                    new_event.error = make_chat_error(response.error);
                    ICY_ERROR(event::post(this, chat_event_type, std::move(new_event)));
                }
                if (requests_queue.pop(current_request))
                {
                    ICY_ERROR(func());
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type chat_system_data::connect(const auth_client_connect_client& client, const crypto_msg<auth_client_connect_module>& module) noexcept
{
    string_view hostname;
    ICY_ERROR(to_string(const_array_view<char>(client.address), hostname));

    const auto it = hostname.find(":"_s);
    if (it == hostname.end())
        return make_chat_error(chat_error_code::invalid_hostname);

    const auto addr = string_view(hostname.begin(), it);
    const auto port = string_view(it + 1, hostname.end());
    uint32_t port_index = 0;
    ICY_ERROR(to_value(port, port_index));
    if (port_index < 0x400 || port_index >= 0xFFFF)
        return make_chat_error(chat_error_code::invalid_hostname);

    internal_message new_event;
    ICY_ERROR(network_address::query(new_event.address, addr, port));
    if (new_event.address.empty())
        return make_stdlib_error(std::errc::host_unreachable);
    
    new_event.type = internal_message_type::connect;
    new_event.session = client.session;
    new_event.encrypted_module_connect = module;
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type chat_system_data::update() noexcept
{
    internal_message new_event;
    new_event.type = internal_message_type::update;
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type chat_system_data::send_to_user(const string_view text, const icy::guid& user) noexcept
{
    internal_message new_event;
    new_event.type = internal_message_type::send;
    new_event.user = user;
    ICY_ERROR(copy(text, new_event.text));
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type chat_system_data::send_to_room(const string_view text, const icy::guid& room) noexcept
{
    internal_message new_event;
    new_event.type = internal_message_type::send;
    new_event.room = room;
    ICY_ERROR(copy(text, new_event.text));
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type icy::create_chat_system(shared_ptr<chat_system>& system) noexcept
{
    shared_ptr<chat_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize());
    system = std::move(new_system);
    return error_type();
}

extern const event_type icy::chat_event_type = icy::next_event_user();