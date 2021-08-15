#include <icy_chat/icy_chat.hpp>
#include <icy_engine/network/icy_network.hpp>

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
    case chat_error_code::invalid_format:
        return copy("Invalid format"_s, str);
    }
    return make_stdlib_error(std::errc::invalid_argument);
}
static string_view to_string(const chat_message_type type) noexcept
{
    switch (type)
    {
    case chat_message_type::user_connect:
        return "userConnect"_s;
    case chat_message_type::user_send:
        return "userSend"_s;
    case chat_message_type::user_recv:
        return "userRecv"_s;

    case chat_message_type::system_connect:
        return "systemConnect"_s;
    case chat_message_type::system_create_room:
        return "systemCreateRoom"_s;
    case chat_message_type::system_invite_user:
        return "systemInviteUser"_s;
    case chat_message_type::system_remove_user:
        return "systemRemoveUser"_s;
    }
    return string_view();
}

class chat_system_data : public chat_system
{
public:
    chat_system_data(const uint64_t username, const crypto_key& password) : m_username(username), m_password(password)
    {

    }
    error_type initialize(const string_view hostname) noexcept;
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
    error_type connect() noexcept override;
    error_type send_to_user(const string_view text, const icy::guid& user) noexcept override;
    error_type send_to_room(const string_view text, const icy::guid& room) noexcept override;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
    string m_hostname;
    uint64_t m_username = 0;
    crypto_key m_password;
    crypto_key m_session;
    shared_ptr<network_system_http_client> m_http;
};
ICY_STATIC_NAMESPACE_END

error_type icy::to_json(const chat_message& input, json& output) noexcept
{
    output = json_type::object;
    if (input.type == chat_message_type::user_connect ||
        input.type == chat_message_type::system_connect)
    {
        char buffer[base64_encode_size(sizeof(chat_message::encrypted_module_connect))] = {};
        ICY_ERROR(base64_encode(input.encrypted_module_connect, buffer));
        string_view str;
        to_string(const_array_view<char>(buffer), str);
        ICY_ERROR(output.insert(chat_key_module, str));
    }
    string guid_user;
    string guid_room;
    string str_time;
    ICY_ERROR(to_string(input.user, guid_user));
    ICY_ERROR(to_string(input.room, guid_room));
    ICY_ERROR(to_string(input.time, str_time, false));
    if (input.user) { ICY_ERROR(output.insert(chat_key_user, guid_user)); }
    if (input.room) { ICY_ERROR(output.insert(chat_key_room, guid_room)); }
    ICY_ERROR(output.insert(chat_key_time, str_time));
    ICY_ERROR(output.insert(chat_key_text, input.text));
    ICY_ERROR(output.insert(chat_key_type, ::to_string(input.type)));
    ICY_ERROR(output.insert(chat_key_version, input.version));
    return error_type();
}
error_type icy::from_json(const json& input, chat_message& output) noexcept
{
    if (input.type() != json_type::object)
        return make_chat_error(chat_error_code::invalid_format);

    json_type_integer version = 0;
    input.get(chat_key_version, version);
    if (version != chat_version)
        return make_chat_error(chat_error_code::invalid_version);

    output.type = chat_message_type::none;
    const auto str_type = input.get(chat_key_type);
    for (auto k = 1u; k < uint32_t(chat_message_type::_total); ++k)
    {
        if (::to_string(chat_message_type(k)) == str_type)
        {
            output.type = chat_message_type(k);
            break;
        }
    }
    if (output.type == chat_message_type::none)
        return make_chat_error(chat_error_code::invalid_type);

    const auto str_time = input.get(chat_key_time);
    if (const auto error = to_value(str_time, output.time, false))
        return make_chat_error(chat_error_code::invalid_format);

    const auto guid_user = input.get(chat_key_user);
    const auto guid_room = input.get(chat_key_room);

    if (!guid_user.empty())
    {
        if (const auto error = to_value(guid_user, output.user))
            return make_chat_error(chat_error_code::invalid_format);
    }
    if (!guid_room.empty())
    {
        if (const auto error = to_value(guid_room, output.room))
            return make_chat_error(chat_error_code::invalid_format);
    }

    ICY_ERROR(copy(input.get(chat_key_text), output.text));

    if (output.type == chat_message_type::system_connect ||
        output.type == chat_message_type::user_connect)
    {
        const auto module_str = input.get(chat_key_module);
        if (module_str.bytes().size() != base64_encode_size(sizeof(chat_message::encrypted_module_connect)))
            return make_chat_error(chat_error_code::invalid_format);

        uint8_t buffer[sizeof(chat_message::encrypted_module_connect)] = {};
        ICY_ERROR(base64_decode(module_str, buffer));
        memcpy(&output.encrypted_module_connect, buffer, sizeof(buffer));
    }
    return error_type();
}

const error_source icy::error_source_chat = register_error_source("chat"_s, chat_error_to_string);
error_type chat_system_data::initialize(const string_view hostname) noexcept
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

    ICY_ERROR(copy(hostname, m_hostname));

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    const uint64_t event_types = 0
        | auth_event_type
        | event_type::system_internal
        | event_type::network_disconnect
        | event_type::network_recv;

    filter(event_types);
    return error_type();
}
error_type chat_system_data::exec() noexcept
{
    shared_ptr<auth_system> auth;

    const auto reset = [this, &auth](const error_type error)
    {
        auth = nullptr;
        m_http = nullptr;
        chat_event new_event;
        new_event.error = error;
        ICY_ERROR(event::post(this, chat_event_type, std::move(new_event)));
        return error_type();
    };

    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<chat_message>();
                if (event_data.type == chat_message_type::user_connect)
                {
                    if (const auto error = create_auth_system(auth, m_hostname))
                    {
                        ICY_ERROR(reset(error));
                        continue;
                    }
                    ICY_ERROR(auth->client_ticket(guid::create(), m_username, m_password));
                    ICY_ERROR(auth->thread().launch());
                    ICY_ERROR(auth->thread().rename("Auth Thread"_s));
                }
                else if (event_data.type == chat_message_type::user_send)
                {
                    if (!m_http)
                    {
                        chat_event new_event;
                        new_event.error = make_stdlib_error(std::errc::not_connected);
                        ICY_ERROR(event::post(this, chat_event_type, std::move(new_event)));
                        continue;
                    }
                    event_data.time = auth_clock::now();
                    event_data.version = chat_version;
                    
                    json json;
                    ICY_ERROR(to_json(event_data, json));

                    string str;
                    ICY_ERROR(to_string(json, str));

                    http_request hrequest;
                    hrequest.type = http_request_type::post;
                    hrequest.content = http_content_type::application_json;
                    ICY_ERROR(hrequest.body.assign(str.ubytes()));
                    ICY_ERROR(m_http->send(hrequest));
                }
            }
            else if ((event->type & event_type::network_any) && m_http)
            {
                if (shared_ptr<event_system>(event->source) != m_http)
                    continue;

                const auto& event_data = event->data<network_event>();
                if (event_data.error)
                {
                    ICY_ERROR(reset(event_data.error));
                    continue;
                }
                if (event->type == event_type::network_disconnect)
                {
                    ICY_ERROR(reset(make_stdlib_error(std::errc::connection_aborted)));
                    continue;
                }
                if (!event_data.http.request
                    || event_data.http.request->type != http_request_type::post
                    || event_data.http.request->content != http_content_type::application_json)
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_format)));
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
                chat_message message;
                if (const auto error = from_json(json, message))
                {
                    ICY_ERROR(reset(error));
                    continue;
                }
                if (message.type != chat_message_type::user_recv)
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_type)));
                    continue;
                }
                if (message.version != chat_version)
                {
                    ICY_ERROR(reset(make_chat_error(chat_error_code::invalid_version)));
                    continue;
                }
                chat_event new_event;
                new_event.time = message.time;
                new_event.text = std::move(message.text);
                new_event.room = message.room;
                new_event.user = message.user;
                ICY_ERROR(event::post(this, chat_event_type, std::move(new_event)));
                ICY_ERROR(m_http->recv());
            }
            else if (event->type == auth_event_type && auth)
            {
                if (shared_ptr<event_system>(event->source) != auth)
                    continue;

                const auto& event_data = event->data<auth_event>();
                if (event_data.error)
                {
                    ICY_ERROR(reset(event_data.error));
                    continue;
                }
                if (event_data.type == auth_request_type::client_ticket)
                {
                    ICY_ERROR(auth->client_connect(guid::create(), m_username, m_password, hash64(chat_auth_module), event_data.encrypted_server_ticket));
                }
                else if (event_data.type == auth_request_type::client_connect)
                {
                    auth = nullptr;
                    string_view addr_str;
                    ICY_ERROR(to_string(const_array_view<char>(event_data.client_connect.address), addr_str));

                    const auto it = addr_str.find(":"_s);
                    if (it == addr_str.end())
                    {
                        ICY_ERROR(reset(make_stdlib_error(std::errc::host_unreachable)));
                        continue;
                    }
                    const auto port = string_view(it + 1, addr_str.end());
                    uint32_t port_index = 0;
                    if (to_value(port, port_index) || port_index < 0x400 || port_index >= 0xFFFF)
                    {
                        ICY_ERROR(reset(make_stdlib_error(std::errc::host_unreachable)));
                        continue;
                    }

                    array<network_address> addr_list;
                    if (const auto error = network_address::query(addr_list, string_view(addr_str.begin(), it), port))
                    {
                        ICY_ERROR(reset(error));
                        continue;
                    }
                    
                    chat_message msg;
                    msg.encrypted_module_connect = event_data.encrypted_module_connect;
                    msg.time = auth_clock::now();
                    msg.version = chat_version;
                    msg.type = chat_message_type::user_connect;

                    json json;
                    ICY_ERROR(to_json(msg, json));
                    
                    string str;
                    ICY_ERROR(to_string(json, str));

                    http_request hrequest;
                    hrequest.type = http_request_type::post;
                    hrequest.content = http_content_type::application_json;
                    ICY_ERROR(hrequest.body.assign(str.ubytes()));

                    for (auto&& addr : addr_list)
                    {
                        if (create_network_http_client(m_http, addr, hrequest, max_timeout, network_default_buffer))
                            continue;
                    }
                    if (!m_http)
                    {
                        ICY_ERROR(reset(make_stdlib_error(std::errc::host_unreachable)));
                        continue;
                    }
                    ICY_ERROR(m_http->thread().launch());
                    ICY_ERROR(m_http->thread().rename("Chat HTTP Thread"_s));
                    ICY_ERROR(m_http->recv());
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type chat_system_data::connect() noexcept
{
    chat_message new_event;
    new_event.type = chat_message_type::user_connect;
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type chat_system_data::send_to_user(const string_view text, const icy::guid& user) noexcept
{
    chat_message new_event;
    new_event.type = chat_message_type::user_send;
    new_event.user = user;
    ICY_ERROR(copy(text, new_event.text));
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type chat_system_data::send_to_room(const string_view text, const icy::guid& room) noexcept
{
    chat_message new_event;
    new_event.type = chat_message_type::user_send;
    new_event.room = room;
    ICY_ERROR(copy(text, new_event.text));
    return event_system::post(nullptr, event_type::system_internal, std::move(new_event));
}
error_type icy::create_chat_system(shared_ptr<chat_system>& system, const string_view hostname, const uint64_t username, const crypto_key& password) noexcept
{
    shared_ptr<chat_system_data> new_system;
    ICY_ERROR(make_shared(new_system, username, password));
    ICY_ERROR(new_system->initialize(hostname));
    system = std::move(new_system);
    return error_type();
}

extern const event_type icy::chat_event_type = icy::next_event_user();