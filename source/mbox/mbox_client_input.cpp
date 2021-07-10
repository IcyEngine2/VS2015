#include <icy_engine/network/icy_network.hpp>
#include "mbox_client.hpp"
#include "mbox_network.hpp"

using namespace icy;

error_type input_thread::run() noexcept
{
    array<network_address> addr_list;
    ICY_ERROR(network_address::query(addr_list, MBOX_MULTICAST_ADDR, MBOX_MULTICAST_PORT));
    if (addr_list.empty())
        return error_type();

    const auto udp_addr = std::move(addr_list[0]);

    shared_ptr<network_system_udp_client> udp;

    network_server_config config;
    config.capacity = 1;
    config.port = udp_addr.port();
    config.timeout = max_timeout;

    ICY_ERROR(create_network_udp_client(udp, config));
    ICY_ERROR(udp->join(udp_addr));
    ICY_ERROR(udp->thread().launch());
    ICY_ERROR(udp->thread().rename("UDP Thread"_s));

    network_address server_addr;

    while (true)
    {
        event event;
        ICY_ERROR(m_loop->pop(event));
        if (!event)
            break;

        mbox_udp_request response;

        if (event->type == event_type::system_internal)
        {
            const auto& event_data = event->data<input_message>();
            if (!server_addr)
                continue;

            response.type = mbox_request_type::input;
            response.input[0] = event_data;
        }
        else if (event->type == event_type::network_recv)
        {
            auto& event_data = event->data<network_event>();
            if (event_data.bytes.size() != sizeof(mbox_udp_request))
                continue;

            const auto request = reinterpret_cast<const mbox_udp_request*>(event_data.bytes.data());
            if (request->uid != MBOX_MULTICAST_PING)
                continue;

            if (request->source != mbox_source_type::server)
                continue;

            switch (request->type)
            {
            case mbox_request_type::ping:
            {
                response.type = mbox_request_type::ping;
                server_addr = std::move(event_data.address);
                break;
            }
            case mbox_request_type::input:
            {
                for (auto&& msg : request->input)
                {
                    if (msg.type == input_type::none)
                        break;
                    ICY_ERROR(m_recv_queue.push(msg));
                }
                break;
            }
            case mbox_request_type::exec:
            {
                if (request->source != mbox_source_type::server)
                    continue;

                const auto cmd = string_view(request->text, strnlen(request->text, sizeof(request->text)));
                if (cmd == "exit"_s)
                    return error_type();

                break;
            }

            default:
                break;
            }
            if (response.type != mbox_request_type::none)
            {
                ICY_ERROR(udp->send(udp_addr, { reinterpret_cast<const uint8_t*>(&response), sizeof(response) }));
            }
        }
    }
    ICY_SCOPE_EXIT{ m_send_ready = false; };
    return error_type();
}