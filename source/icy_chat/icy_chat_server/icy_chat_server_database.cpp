#pragma once

#include "icy_chat_server.hpp"

using namespace icy;

error_type chat_database::initialize(const icy::string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(m_base.initialize("chat_dbase.dat"_s, 1_gb));
    database_txn_write txn;
    ICY_ERROR(txn.initialize(m_base));
    ICY_ERROR(m_dbi_users.initialize_create_any_key(txn, "users"_s));
    ICY_ERROR(m_dbi_rooms.initialize_create_any_key(txn, "rooms"_s));
    ICY_ERROR(txn.commit());
    return error_type();
}
error_type chat_database::exec(const icy::chat_request& request, icy::chat_response& response) noexcept
{
    switch (request.type)
    {
    case chat_request_type::user_send_text:
    {
        auth_client_connect_module connect;
        if (const auto error = request.encrypted_module_connect.decode(password, connect))
        {
            response.error = chat_error_code::access_denied;
            return error_type();
        }
        if (request.text.empty())
        {
            response.error = chat_error_code::invalid_message_size;
            return error_type();
        }
        if (!request.user && !request.room || request.user == connect.userguid)
        {
            response.error = chat_error_code::invalid_user;
            return error_type();
        }

        chat_entry entry;
        entry.room = request.room;
        entry.user = connect.userguid;
        entry.time = request.time;
        entry.size = uint32_t(request.text.bytes().size());

        database_txn_write txn;
        ICY_ERROR(txn.initialize(m_base));
        if (request.room)
        {
            auto key = request.room;
            database_cursor_read cur_room;
            ICY_ERROR(cur_room.initialize(txn, m_dbi_rooms));
            const_array_view<uint8_t> val;
            if (const auto error = cur_room.get_var_by_type(key, val, database_oper_read::none))
            {
                response.error = chat_error_code::invalid_room;
                return error_type();
            }
            auto is_valid = false;
            const auto ptr = reinterpret_cast<const guid*>(val.data());
            for (auto k = 0u; k < val.size() / sizeof(guid); ++k)
            {
                if (ptr[k] == connect.userguid)
                {
                    is_valid = true;
                    break;
                }
            }
            if (!is_valid)
            {
                response.error = chat_error_code::invalid_room;
                return error_type();
            }

            database_cursor_write cur_user;
            ICY_ERROR(cur_user.initialize(txn, m_dbi_users));

            for (auto k = 0u; k < val.size() / sizeof(guid); ++k)
            {
                if (ptr[k] == connect.userguid)
                    continue;

                const_array_view<uint8_t> bytes;
                key = ptr[k];
                if (const auto error = cur_user.get_var_by_type(key, bytes, database_oper_read::none))
                {
                    if (error != database_error_not_found)
                        return error;
                }

                array_view<uint8_t> write;
                ICY_ERROR(cur_user.put_var_by_type(ptr[k], bytes.size() + sizeof(chat_entry) + entry.size, database_oper_write::none, write));
                memcpy(write.data() + 0, bytes.data(), bytes.size());
                memcpy(write.data() + bytes.size(), &entry, sizeof(chat_entry));
                memcpy(write.data() + bytes.size() + sizeof(chat_entry), request.text.bytes().data(), entry.size);
            }
        }
        else if (request.user)
        {
            auto key = request.user;
            database_cursor_write cur_user;
            ICY_ERROR(cur_user.initialize(txn, m_dbi_users));

            const_array_view<uint8_t> bytes;
            if (const auto error = cur_user.get_var_by_type(key, bytes, database_oper_read::none))
            {
                if (error == database_error_not_found)
                {
                    response.error = chat_error_code::invalid_user;
                    return error_type();
                }
                return error;
            }

            array_view<uint8_t> write;
            ICY_ERROR(cur_user.put_var_by_type(key, bytes.size() + sizeof(chat_entry) + entry.size, database_oper_write::none, write));
            memcpy(write.data() + 0, bytes.data(), bytes.size());
            memcpy(write.data() + bytes.size(), &entry, sizeof(chat_entry));
            memcpy(write.data() + bytes.size() + sizeof(chat_entry), request.text.bytes().data(), entry.size);
        }
        ICY_ERROR(txn.commit());
        break;
    } 
    case chat_request_type::user_update:
    {
        auth_client_connect_module connect;
        if (const auto error = request.encrypted_module_connect.decode(password, connect))
        {
            response.error = chat_error_code::access_denied;
            return error_type();
        }
        database_txn_write txn;
        ICY_ERROR(txn.initialize(m_base));
        {
            database_cursor_write cur_user;
            ICY_ERROR(cur_user.initialize(txn, m_dbi_users));

            auto clear = false;
            const_array_view<uint8_t> bytes;
            if (const auto error = cur_user.get_var_by_type(connect.userguid, bytes, database_oper_read::none))
            {
                if (error != database_error_not_found)
                    return error;
                clear = true;
            }
            if (!bytes.empty())
            {
                const auto ptr = reinterpret_cast<const chat_entry*>(bytes.data());
                if (bytes.size() < sizeof(chat_entry) ||
                    bytes.size() < sizeof(chat_entry) + ptr->size)
                {
                    clear = true;
                }
                else
                {
                    response.time = ptr->time;
                    response.room = ptr->room;
                    response.user = ptr->user;
                    if (const auto error = to_string(const_array_view<uint8_t>(bytes.data() + sizeof(chat_entry), ptr->size), response.text))
                    {
                        clear = true;
                    }
                }
                if (!clear)
                {
                    const auto offset = sizeof(chat_entry) + ptr->size;
                    array_view<uint8_t> write;
                    ICY_ERROR(cur_user.put_var_by_type(connect.userguid, bytes.size() - offset, database_oper_write::none, write));
                    memcpy(write.data(), bytes.data() + offset, bytes.size() - offset);
                }               
            }
            if (clear)
            {
                array_view<uint8_t> write;
                ICY_ERROR(cur_user.put_var_by_type(connect.userguid, 0, database_oper_write::none, write));
            }
        }
        ICY_ERROR(txn.commit());
        break;
    }

    default:
        response.error = chat_error_code::invalid_type;
        return error_type();
    }
    return error_type();
}