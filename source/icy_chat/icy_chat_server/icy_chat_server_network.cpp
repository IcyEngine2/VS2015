#include "icy_chat_server.hpp"
#include <icy_engine/network/icy_network.hpp>

using namespace icy;

error_type chat_server_application::run_network() noexcept
{
    shared_ptr<event_queue> loop;
    ICY_ERROR(create_event_system(loop, 0
        | event_type::network_recv 
        | event_type::network_connect));

    while (*loop)
    {
        event event;
        ICY_ERROR(loop->pop(event));
        if (!event)
            break;

        if (shared_ptr<event_system>(event->source) != http_server)
            continue;

        const auto& event_data = event->data<network_event>();
        const auto hash = event_data.conn.hash();

        if (event->type == event_type::network_disconnect)
        {
            /*const auto it = conn_to_guid.find(hash);
            if (it != conn_to_guid.end())
            {
                const auto guid = it->value;
                conn_to_guid.erase(it);
                const auto jt = guid_to_user.find(guid);
                if (jt != guid_to_user.end())
                {
                    auto& conn_list = jt->value.connections;
                    const auto kt = conn_list.find(hash);
                    if (kt != conn_list.end())
                        conn_list.erase(kt);
                }
            }*/
        }
        else if (event->type == event_type::network_connect)
        {
            ICY_ERROR(http_server->recv(event_data.conn));
        }
        else if (event->type == event_type::network_recv)
        {
            const auto& hrequest = event_data.http.request;
            if (!hrequest || hrequest->type != http_request_type::post || hrequest->content != http_content_type::application_json)
            {
                ICY_ERROR(http_server->disc(event_data.conn));
                continue;
            }
            string_view body_str;
            if (to_string(hrequest->body, body_str))
            {
                ICY_ERROR(http_server->disc(event_data.conn));
                continue;
            }
            icy::json json;
            if (const auto error = to_value(body_str, json))
            {
                if (error == make_stdlib_error(std::errc::not_enough_memory))
                    return error;
                ICY_ERROR(http_server->disc(event_data.conn));
                continue;
            }
            chat_request request;
            if (const auto error = request.from_json(json))
            {
                if (error == make_stdlib_error(std::errc::not_enough_memory))
                    return error;
                ICY_ERROR(http_server->disc(event_data.conn));
                continue;
            }
            
            chat_response response;
            ICY_ERROR(database.exec(request, response));
            ICY_ERROR(response.to_json(json));
            string str;
            ICY_ERROR(to_string(json, str));

            http_response hresponse;
            hresponse.type = http_content_type::application_json;
            hresponse.body.assign(str.ubytes());
            hresponse.herror = http_error::success;
            ICY_ERROR(http_server->send(event_data.conn, hresponse));
            ICY_ERROR(http_server->recv(event_data.conn));
        }
    }

    return error_type();
}

/*

            if (request.type == chat_request_type::user_send_text||
                msg.type == chat_message_type::system_connect)
            {
                auth_client_connect_module auth;
                if (msg.encrypted_module_connect.decode(crypto_password, auth)) // bad password
                {
                    ICY_ERROR(http_server->disc(event_data.conn));
                    continue;
                }

                auto it = guid_to_user.find(auth.userguid);
                if (it == guid_to_user.end())
                {
                    ICY_ERROR(guid_to_user.insert(auth.userguid, chat_user(), &it));
                }

                auto& conn_list = it->value.connections;
                auto jt = conn_list.find(hash);
                if (jt == conn_list.end())
                {
                    ICY_ERROR(conn_list.insert(hash, chat_connection(), &jt));
                    ICY_ERROR(conn_to_guid.insert(hash, auth.userguid));
                }
                jt->value.network = event_data.conn;
                jt->value.expire = auth.expire;
                jt->value.session = auth.session;

                database_txn_write txn;
                txn.initialize(database);
                database_cursor_write cur;
                cur.initialize(txn, dbi_users);



            }
            else if (msg.type == chat_message_type::user_send)
            {
                const auto it = conn_to_guid.find(hash);
                if (it == conn_to_guid.end())
                {
                    ICY_ERROR(http_server->disc(event_data.conn));
                    continue;
                }
                ICY_ERROR(exec(, msg));

            }
            else // bad event type
            {
                ICY_ERROR(http_server->disc(event_data.conn));
                continue;
            }
*/