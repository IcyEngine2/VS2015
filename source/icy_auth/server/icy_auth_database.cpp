#include "icy_auth_database.hpp"
#include "icy_auth_config.hpp"

#if _DEBUG
#pragma comment(lib, "libsodiumd")
#else
#pragma comment(lib, "libsodium")
#endif

using namespace icy;

static error_type save_date_time(char (&str)[auth_date_length]) noexcept
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    tm time;
    if (const auto error = gmtime_s(&time, &now))
        return make_stdlib_error(std::errc(error));
    strftime(str, _countof(str), "%F %T", &time);
    return error_type();
}

error_type auth_database::initialize(const auth_config_dbase& config) noexcept
{
    ICY_ERROR(m_data.initialize(config.file_path, config.file_size));
    
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_data));

    ICY_ERROR(m_dbi_usr.initialize_create_int_key(txn, "clients"_s));
    ICY_ERROR(m_dbi_mod.initialize_create_int_key(txn, "modules"_s));
    
    if (m_dbi_usr.size(txn) > config.clients)
        return make_auth_error(auth_error_code::client_max_capacity);
    if (m_dbi_mod.size(txn) > config.modules)
        return make_auth_error(auth_error_code::module_max_capacity);
    
    ICY_ERROR(txn.commit());
    m_max_clients = config.clients;
    m_max_modules = config.modules;
    m_timeout = config.timeout;
    return {};
}
error_type auth_database::query(const auth_query_flag flags, auth_query_callback& callback) const noexcept
{
    database_txn_read txn;
    ICY_ERROR(txn.initialize(m_data));
    
    if (flags & (auth_query_flag::client_date | auth_query_flag::client_guid | auth_query_flag::client_name))
    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(txn, m_dbi_usr));

        auth_client_query_data query;
        const auto func = [this, flags, &cur, &query](const database_oper_read oper) -> error_type
        {
            uint64_t name = 0;
            auth_client_data saved;
            ICY_ERROR(cur.get_type_by_type(name, saved, oper));

            if (flags & auth_query_flag::client_name) query.name = name;
            if (flags & auth_query_flag::client_guid) query.guid = saved.guid;
            if (flags & auth_query_flag::client_date)
            {
                ICY_ERROR(to_value(string_view(saved.date, _countof(saved.date)), query.date));
            }
            return error_type();
        };

        auto error = func(database_oper_read::first);
        while (!error)
        {
            ICY_ERROR(callback(query));
            error = func(database_oper_read::next);
        }
        if (error == database_error_not_found)
            ;
        else
            ICY_ERROR(error);
    }
    if (flags & (
        auth_query_flag::module_addr | 
        auth_query_flag::module_date | 
        auth_query_flag::module_guid | 
        auth_query_flag::module_name |
        auth_query_flag::module_time))
    {
        database_cursor_read cur;
        ICY_ERROR(cur.initialize(txn, m_dbi_mod));

        auth_module_query_data query;
        const auto func = [this, flags, &cur, &query](const database_oper_read oper) -> error_type
        {
            uint64_t name = 0;
            auth_module_data saved;
            ICY_ERROR(cur.get_type_by_type(name, saved, oper));

            if (flags & auth_query_flag::module_name) query.name = name;
            if (flags & auth_query_flag::module_guid) query.guid = saved.guid;
            if (flags & auth_query_flag::module_date)
            {
                ICY_ERROR(to_value(string_view(saved.date, _countof(saved.date)), query.date));
            }
            if (flags & auth_query_flag::module_addr)
            {
                static_assert(sizeof(query.addr) == auth_addr_length && sizeof(saved.addr) == auth_addr_length, "INVALID ADDRESS SIZE");
                memcpy(query.addr, saved.addr, auth_addr_length);
            }
            if (flags & auth_query_flag::module_time)
            {
                query.timeout = std::chrono::milliseconds(saved.timeout);
            }
            return error_type();
        };

        auto error = func(database_oper_read::first);
        while (!error)
        {
            ICY_ERROR(callback(query));
            error = func(database_oper_read::next);
        }
        if (error == database_error_not_found)
            ;
        else
            ICY_ERROR(error);
    }
    return error_type();
}
error_type auth_database::exec(const auth_request& request, auth_response& response) noexcept
{
    const auto func = [this, &request, &response](auto& input, auto& output) -> error_type
    {
        ICY_ERROR(input.from_json(request.message));
        ICY_ERROR(exec(request, input, output));
        ICY_ERROR(output.to_json(response.message));
        return error_type();
    };

    switch (request.type)
    {
    case auth_request_type::client_create:
    {
        auth_request_msg_client_create input;
        auth_response_msg_client_create output;
        ICY_ERROR(func(input, output));
        break;
    }
    case auth_request_type::client_ticket:
    {
        auth_request_msg_client_ticket input;
        auth_response_msg_client_ticket output;
        ICY_ERROR(func(input, output));
        break;
    }
    case auth_request_type::client_connect:
    {
        auth_request_msg_client_connect input;
        auth_response_msg_client_connect output;
        ICY_ERROR(func(input, output));
        break;
    }
    case auth_request_type::module_create:
    {
        auth_request_msg_module_create input;
        auth_response_msg_module_create output;
        ICY_ERROR(func(input, output));
        break;
    }
    case auth_request_type::module_update:
    {
        auth_request_msg_module_update input;
        auth_response_msg_module_update output;
        ICY_ERROR(func(input, output));
        break;
    }
    default:
        return make_auth_error(auth_error_code::invalid_json_type);
    }
    return error_type();
}
error_type auth_database::exec(const auth_request& request, const auth_request_msg_client_create& input, auth_response_msg_client_create&) noexcept
{
    auth_client_data new_client;
    ICY_ERROR(guid::create(new_client.guid));
    ICY_ERROR(save_date_time(new_client.date));
    new_client.password = input.password;
    auto key = request.username;

    const auto func = [this](auto& txn, auto& cur) -> error_type
    {
        ICY_ERROR(txn.initialize(m_data));

        if (m_dbi_usr.size(txn) >= m_max_clients)
            return make_auth_error(auth_error_code::client_max_capacity);

        ICY_ERROR(cur.initialize(txn, m_dbi_usr));
        return {};
    };

    {
        database_txn_read txn;
        database_cursor_read cur;
        ICY_ERROR(func(txn, cur));
        if (const auto error = cur.get_type_by_type(key, new_client, database_oper_read::none))
        {
            if (error == database_error_not_found)
                ;   //  ok
            else
                return error;
        }
        else
            return make_auth_error(auth_error_code::client_already_exists);
    }
    database_txn_write txn;
    {
        database_cursor_write cur;
        ICY_ERROR(func(txn, cur));
        if (const auto error = cur.put_type_by_type(key, database_oper_write::unique, new_client))
        {
            if (error == database_error_key_exist)
                return make_auth_error(auth_error_code::client_already_exists);
            ICY_ERROR(error);
        }
    }
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type auth_database::exec(const auth_request& request, const auth_request_msg_client_ticket& input, auth_response_msg_client_ticket& output) noexcept
{
    database_txn_read txn;
    database_cursor_read cur;
    ICY_ERROR(txn.initialize(m_data));
    ICY_ERROR(cur.initialize(txn, m_dbi_usr));

    auto key = request.username;
    auth_client_data val;
    if (const auto error = cur.get_type_by_type(key, val, database_oper_read::none))
    {
        if (error == database_error_not_found)
            return make_auth_error(auth_error_code::client_not_found);
        ICY_ERROR(error);
    }

    std::chrono::system_clock::time_point time;
    if (const auto error = input.encrypted_time.decode(val.password, time))
        return make_auth_error(auth_error_code::client_invalid_password);
    if (time < request.time || time > request.time + std::chrono::seconds(1))
        return make_auth_error(auth_error_code::client_invalid_password);

    auth_client_ticket_client ticket_client;
    auth_client_ticket_server ticket_server;
    const auto now = std::chrono::system_clock::now();
    ticket_client.expire = now + m_timeout;
    ticket_server.expire = now + m_timeout;
    output.encrypted_client_ticket = crypto_msg<auth_client_ticket_client>(val.password, ticket_client);
    output.encrypted_server_ticket = crypto_msg<auth_client_ticket_server>(m_password, ticket_server);
    return error_type();
}
error_type auth_database::exec(const auth_request& request, const auth_request_msg_client_connect& input, auth_response_msg_client_connect& output) noexcept
{
    auth_client_ticket_server ticket;
    if (const auto error = input.encrypted_server_ticket.decode(m_password, ticket))
        return make_auth_error(auth_error_code::client_invalid_ticket);
    
    const auto now = std::chrono::system_clock::now();
    if (now > ticket.expire)
        return make_auth_error(auth_error_code::client_expired_ticket);

    const auto session = crypto_key(crypto_random);

    database_txn_read txn;
    database_cursor_read cur_client;
    database_cursor_read cur_module;
    ICY_ERROR(txn.initialize(m_data));
    ICY_ERROR(cur_client.initialize(txn, m_dbi_usr));
    ICY_ERROR(cur_module.initialize(txn, m_dbi_mod));

    auth_module_data module;
    auth_client_data client;
    auto key_module = input.module;
    auto key_client = request.username;

    if (const auto error = cur_module.get_type_by_type(key_module, module, database_oper_read::none))
        return make_auth_error(auth_error_code::client_invalid_module);
    if (const auto error = cur_client.get_type_by_type(key_client, client, database_oper_read::none))
        return make_auth_error(auth_error_code::client_not_found);

    auth_client_connect_client connect_client;
    auth_client_connect_module connect_module;
    memcpy(connect_client.address, module.addr, auth_addr_length);
    connect_client.expire = connect_module.expire = now + std::chrono::milliseconds(module.timeout);
    connect_client.session = connect_module.session = session;
    connect_module.userguid = client.guid;
    output.encrypted_client_connect = crypto_msg<auth_client_connect_client>(client.password, connect_client);
    output.encrypted_module_connect = crypto_msg<auth_client_connect_module>(module.password, connect_module);
    return error_type();
}
error_type auth_database::exec(const auth_request&, const auth_request_msg_module_create& input, auth_response_msg_module_create&) noexcept
{
    auth_module_data new_module;
    ICY_ERROR(guid::create(new_module.guid));
    ICY_ERROR(save_date_time(new_module.date));
    auto key = input.module;

    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_data));

    {
        database_cursor_write cur;        
        if (m_dbi_mod.size(txn) >= m_max_modules)
            return make_auth_error(auth_error_code::module_max_capacity);

        ICY_ERROR(cur.initialize(txn, m_dbi_mod));
        if (const auto error = cur.put_type_by_type(key, database_oper_write::unique, new_module))
        {
            if (error == database_error_key_exist)
                return make_auth_error(auth_error_code::module_already_exists);
            ICY_ERROR(error);
        }
    }
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type auth_database::exec(const auth_request&, const auth_request_msg_module_update& input, auth_response_msg_module_update&) noexcept
{
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_data));

    {
        database_cursor_write cur;
        ICY_ERROR(cur.initialize(txn, m_dbi_mod));

        auth_module_data edit_module;
        auto key = input.module;
        if (const auto error = cur.get_type_by_type(key, edit_module, database_oper_read::none))
        {
            if (error == database_error_not_found)
                return make_auth_error(auth_error_code::module_not_found);
        }
        edit_module.password = input.password;
        edit_module.timeout = ms_timeout(input.timeout);
        snprintf(edit_module.addr, _countof(edit_module.addr), "%s", input.address.bytes().data());
        ICY_ERROR(cur.put_type_by_type(key, database_oper_write::none, edit_module));
    }
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type auth_database::clear_clients() noexcept
{
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_data));
    ICY_ERROR(m_dbi_usr.clear(txn));
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type auth_database::clear_modules() noexcept
{
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_data));
    ICY_ERROR(m_dbi_mod.clear(txn));
    ICY_ERROR(txn.commit());
    return error_type();
}