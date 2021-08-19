#include <icy_engine/resource/icy_engine_resource.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct internal_message
{
    resource_header header;
    string path;
    array<uint8_t> bytes;
};
static const auto key_time_create = "timeC"_s;
static const auto key_time_update = "timeU"_s;
static const auto key_hash = "hash"_s;
static const auto key_type = "type"_s;
static const auto key_name = "name"_s;
static const auto key_data = "data"_s;
class resource_system_data : public resource_system
{
public:
    ~resource_system_data() noexcept override
    {
        if (m_thread)
            m_thread->wait();
        filter(0);
    }
    error_type initialize(const string_view path, const size_t capacity) noexcept;
    error_type exec() noexcept override;
    error_type signal(const event_data*) noexcept override
    {
        return m_sync.wake();
    }
private:
    const icy::thread& thread() const noexcept
    {
        return *m_thread;
    }
    error_type load(const resource_header& header) noexcept override;
    error_type store(const resource_header& header, const string_view path) noexcept override;
    error_type store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept override;
    error_type list(map<guid, resource_data>& output) const noexcept override;
private:
    error_type parse_image(const_array_view<uint8_t> input, matrix<color>& output) noexcept;
private:
    sync_handle m_sync;
    shared_ptr<event_thread> m_thread;
    database_system_write m_dbase;
    database_dbi m_dbi_header;
    database_dbi m_dbi_binary;
};
ICY_STATIC_NAMESPACE_END

error_type resource_binary::to_json(icy::json& output) const noexcept
{
    ICY_ERROR(output.insert(key_time_create, time_create));
    ICY_ERROR(output.insert(key_time_create, time_update));
    ICY_ERROR(output.insert(key_hash, hash));
    return error_type();
}
error_type resource_binary::from_json(const icy::json& input) noexcept
{
    if (input.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(input.get(key_time_create, time_create));
    ICY_ERROR(input.get(key_time_update, time_update));
    ICY_ERROR(input.get(key_hash, hash));
    return error_type();
}
error_type resource_data::to_json(icy::json& output) const noexcept
{
    ICY_ERROR(output.insert(key_type, to_string(type)));
    ICY_ERROR(output.insert(key_name, name));
    icy::json map = json_type::object;
    for (auto&& pair : binary)
    {
        icy::json binary;
        ICY_ERROR(pair.value.to_json(binary));
        ICY_ERROR(map.insert(to_string(resource_locale(pair.key)), std::move(binary)));
    }
    ICY_ERROR(output.insert(key_data, std::move(map)));
    return error_type();
}
error_type resource_data::from_json(const icy::json& input) noexcept
{
    if (input.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto type_str = input.get(key_type);
    for (auto k = 1u; k < uint32_t(resource_type::_total); ++k)
    {
        if (type_str == to_string(resource_type(k)))
        {
            type = resource_type(k);
            break;
        }
    }
    ICY_ERROR(copy(input.get(key_name), name));
    const auto data = input.find(key_data);
    if (!data || data->type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    for (auto k = 0u; k < data->size(); ++k)
    {
        const auto& key = data->keys()[k];
        const auto& val = data->vals()[k];

        resource_binary new_val;
        ICY_ERROR(new_val.from_json(val));

        for (auto k = 0u; k < uint32_t(resource_locale::_total); ++k)
        {
            if (key == to_string(resource_locale(k)))
            {
                ICY_ERROR(binary.insert(k, std::move(new_val)));
                break;
            }
        }
    }
    return error_type();
}

error_type resource_system_data::initialize(const string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(m_dbase.initialize(path, capacity));

    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_dbase));
    ICY_ERROR(m_dbi_header.initialize_create_any_key(txn, "header"_s));
    ICY_ERROR(m_dbi_binary.initialize_create_any_key(txn, "binary"_s));
    ICY_ERROR(txn.commit());

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    filter(event_type::system_internal);
    return error_type();
}
error_type resource_system_data::exec() noexcept
{
    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type != event_type::system_internal)
                continue;

            const auto& event_data = event->data<internal_message>();
            
            resource_event new_event;
            new_event.header.index = event_data.header.index;
            new_event.header.locale = event_data.header.locale;
            new_event.header.type = event_data.header.type;

            array<uint8_t> bytes;
            const_array_view<uint8_t> write = event_data.bytes;

            if (!event_data.path.empty())
            {
                file input;
                input.open(event_data.path, file_access::read, file_open::open_existing, file_share::read, file_flag::none);
                auto size = input.info().size;
                if (!size) new_event.error = make_stdlib_error(std::errc::no_such_file_or_directory);
                if (!new_event.error) new_event.error = bytes.resize(size);
                if (!new_event.error) new_event.error = input.read(bytes.data(), size);
                if (!new_event.error) new_event.error = bytes.resize(size);
                if (!new_event.error) write = bytes;
            }

            resource_data resource;
            const auto& parse = [&new_event, &resource](database_cursor_read& cur)
            {
                string_view str;
                if (!new_event.error)
                {
                    if (const auto error = cur.get_str_by_type(new_event.header.index, str, database_oper_read::none))
                    {
                        if (error != database_error_not_found)
                            return error;
                        new_event.error = error;
                    }
                }
                icy::json json;
                if (!new_event.error && !str.empty()) new_event.error = to_value(str, json);
                if (!new_event.error && json.type() == json_type::object) new_event.error = resource.from_json(json);
                return error_type();
            };

            if (!write.empty())
            {
                database_txn_write txn;
                database_cursor_write cur_header;
                database_cursor_write cur_binary;
                ICY_ERROR(txn.initialize(m_dbase));
                ICY_ERROR(cur_header.initialize(txn, m_dbi_header));
                ICY_ERROR(cur_binary.initialize(txn, m_dbi_binary));
                if (!new_event.error)
                {
                    ICY_ERROR(parse(cur_header));
                    if (!event_data.header.name.empty())
                    {
                        ICY_ERROR(copy(event_data.header.name, resource.name));
                    }
                    if (resource.type != resource_type::none)
                    {
                        if (event_data.header.type != resource.type)
                            new_event.error = make_stdlib_error(std::errc::invalid_argument);
                    }
                    else
                    {
                        resource.type = event_data.header.type;
                    }
                }
                array_view<uint8_t> output;
                if (!new_event.error)
                {
                    auto it = resource.binary.find(uint32_t(event_data.header.locale));
                    if (it == resource.binary.end())
                    {
                        ICY_ERROR(resource.binary.insert(uint32_t(event_data.header.locale), resource_binary(), &it));
                    }
                    it->value.hash = hash64(write.data(), write.size());
                    const auto now = clock_type::now();
                    if (it->value.time_create == clock_type::time_point())
                        it->value.time_create = now;
                    it->value.time_update = now;
                    new_event.error = cur_binary.put_var_by_type(it->value.hash, write.size(), database_oper_write::none, output);
                }
                if (!new_event.error)
                {
                    if (write.size() != output.size())
                        return make_unexpected_error();
                    memcpy(output.data(), write.data(), write.size());
                }
                if (!new_event.error)
                {
                    json json;
                    ICY_ERROR(resource.to_json(json));
                    string str;
                    ICY_ERROR(to_string(json, str));
                    new_event.error = cur_header.put_str_by_type(event_data.header.index, database_oper_write::none, str);
                }
                if (!new_event.error)
                {
                    cur_header = {};
                    cur_binary = {};
                    new_event.error = txn.commit();
                }
                ICY_ERROR(event::post(this, event_type::resource_store, std::move(new_event)));
            }
            else
            {
                database_txn_read txn;
                database_cursor_read cur_header;
                database_cursor_read cur_binary;
                ICY_ERROR(txn.initialize(m_dbase));
                ICY_ERROR(cur_header.initialize(txn, m_dbi_header));
                ICY_ERROR(cur_binary.initialize(txn, m_dbi_binary));
                if (!new_event.error)
                {
                    ICY_ERROR(parse(cur_header));
                    ICY_ERROR(copy(resource.name, new_event.header.name));
                }
                const_array_view<uint8_t> input;
                if (!new_event.error)
                {
                    if (const auto ptr = resource.binary.try_find(uint32_t(event_data.header.locale)))
                    {
                        if (const auto error = cur_binary.get_var_by_type(ptr->hash, input, database_oper_read::none))
                        {
                            if (error != database_error_not_found)
                                return error;
                        }
                        if (!input.empty())
                        {
                            new_event.hash = ptr->hash;
                            new_event.time_create = ptr->time_create;
                            new_event.time_update = ptr->time_update;
                        }
                    }
                }
                if (!new_event.error)
                {
                    new_event.error = make_stdlib_error(std::errc::invalid_argument);
                    switch (event_data.header.type)
                    {
                    case resource_type::text:
                    {
                        if (resource.type == resource_type::text)
                            new_event.error = to_string(input, new_event.data.text);
                        break;
                    }
                    case resource_type::image:
                    {
                        if (resource.type == resource_type::image)
                            new_event.error = parse_image(input, new_event.data.image);
                        break;
                    }
                    case resource_type::user:
                    {
                        new_event.error = new_event.data.user.assign(input);
                        break;
                    }
                    }
                }
                ICY_ERROR(event::post(this, event_type::resource_load, std::move(new_event)));
            }
          
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type resource_system_data::load(const resource_header& header) noexcept
{
    if (!header)
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type resource_system_data::store(const resource_header& header, const string_view path) noexcept
{
    if (!header || path.empty())
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(copy(header.name, msg.header.name));
    ICY_ERROR(copy(path, msg.path));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();

}
error_type resource_system_data::store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept
{
    if (!header || bytes.empty())
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(copy(header.name, msg.header.name));
    ICY_ERROR(msg.bytes.assign(bytes));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type resource_system_data::list(map<guid, resource_data>& output) const noexcept
{
    database_txn_read txn;
    database_cursor_read cur_header;
    ICY_ERROR(txn.initialize(m_dbase));
    ICY_ERROR(cur_header.initialize(txn, m_dbi_header));

    guid key;
    string_view str;
    auto error = cur_header.get_str_by_type(key, str, database_oper_read::first);
    while (!error)
    {
        icy::json json;
        ICY_ERROR(to_value(str, json));
        resource_data new_resource;
        ICY_ERROR(new_resource.from_json(json));
        ICY_ERROR(output.insert(key, std::move(new_resource)));
        error = cur_header.get_str_by_type(key, str, database_oper_read::next);
    }
    if (error != database_error_not_found)
        return error;
    return error_type();
}
error_type resource_system_data::parse_image(const const_array_view<uint8_t> input, matrix<color>& output) noexcept
{
    return error_type();
}

error_type icy::create_resource_system(shared_ptr<resource_system>& system, const string_view path, const size_t capacity) noexcept
{
    shared_ptr<resource_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(path, capacity));
    system = std::move(new_ptr);
    return error_type();
}