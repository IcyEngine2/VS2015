#pragma once

#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/image/icy_image.hpp>
#include "icy_mbox_server_config.hpp"
#include "icy_mbox_server_proc.hpp"
#include "../icy_mbox.hpp"

static constexpr auto event_type_user_connect = icy::event_type(icy::event_type::user << 0x01);
static constexpr auto event_type_user_disconnect = icy::event_type(icy::event_type::user << 0x02);
static constexpr auto event_type_user_recv_input = icy::event_type(icy::event_type::user << 0x03);
static constexpr auto event_type_user_recv_image = icy::event_type(icy::event_type::user << 0x04);

struct event_user_connect 
{
    mbox::info info;
    icy::string address;
};
struct event_user_disconnect
{
    mbox::key key;
};
struct event_user_recv_input
{
    mbox::key key;
    icy::array<icy::input_message> data;    
};
struct event_user_recv_image
{
    mbox::key key;
    icy::array<uint8_t> png;
    icy::matrix<icy::color> colors;
};

class mbox_server_network : public icy::thread
{
    static constexpr auto network_code_update = 1u;
public:
    struct value_type
    {
        value_type() noexcept
        {
        }
        value_type(value_type&& rhs) noexcept
        {
            std::terminate();
        }
        icy::network_tcp_connection conn = icy::network_connection_type::none;
        mbox::key key = {};
        icy::array<uint8_t> send;
        bool sending = false;
        mbox::command cmd;
    };
public:
    icy::error_type initialize(const mbox_config& config)
    {
        using namespace icy;
        const auto delim = config.ipaddr.find(":"_s);
        uint16_t port = 0;
        if (delim == config.ipaddr.end() || string_view(delim + 1, config.ipaddr.end()).to_value(port))
            return make_stdlib_error(std::errc::bad_address);
        if (!config.maxconn)
            return make_stdlib_error(std::errc::invalid_argument);
        
        ICY_ERROR(m_system.initialize());
        ICY_ERROR(m_system.launch(port, network_address_type::ip_v4, 5));
        ICY_ERROR(m_data.reserve(config.maxconn));
        for (auto k = 0u; k < config.maxconn; ++k)
        {
            ICY_ERROR(m_data.emplace_back());
            auto& val = m_data.back();
            val.conn.timeout(max_timeout);
            ICY_ERROR(m_system.accept(val.conn));
        }
        return {};
    }
    icy::error_type run() noexcept
    {
        using namespace icy;
        ICY_SCOPE_EXIT{ event::post(nullptr, event_type::global_quit); };

        const auto reset = [this](value_type& val)
        {
            val.conn._close();
            ICY_ERROR(m_system.accept(val.conn));
            val.send.clear();
            val.sending = false;
            val.cmd = {};
            if (val.key.hash)
            {
                event_user_disconnect msg;
                msg.key = val.key;
                ICY_ERROR(event::post(nullptr, event_type_user_disconnect, std::move(msg)));
            }
            val.key = {};
            return error_type();
        };

        while (true)
        {
            auto code = 0u;
            network_tcp_reply reply;
            ICY_ERROR(m_system.loop(reply, code));
            if (code == network_code_exit)
                break;
            else if (code == network_code_update)
            {
                ICY_LOCK_GUARD(m_lock);
                for (auto&& val : m_data)
                {
                    if (!val.sending && !val.send.empty())
                    {
                        if (m_system.send(val.conn, val.send))
                        {
                            ICY_ERROR(reset(val));
                        }
                        else
                        {
                            val.send.clear();
                            val.sending = true;
                        }
                    }
                }
            }
            const auto val = reinterpret_cast<value_type*>(reply.conn);

            if (!reply.error && reply.conn)
            {
                if (reply.type == network_request_type::accept)
                {
                    if (m_system.recv(*reply.conn, sizeof(mbox::info)) == error_type())
                        continue;
                }
                else if (reply.type == network_request_type::recv)
                {
                    if (!val->key.hash)
                    {
                        if (reply.bytes.size() == sizeof(mbox::info))
                        {
                            const auto info = reinterpret_cast<const mbox::info*>(reply.bytes.data());
                            if (info->version == mbox::protocol_version)
                            {
                                val->key = info->key;
                                if (m_system.recv(val->conn, sizeof(mbox::command)) == error_type())
                                {
                                    event_user_connect msg;
                                    msg.info = *info;
                                    ICY_ERROR(to_string(val->conn.address(), msg.address));
                                    ICY_ERROR(event::post(nullptr, event_type_user_connect, std::move(msg)));
                                    continue;
                                }
                            }
                        }
                    }
                    else if (reply.bytes.size() == val->cmd.size)
                    {
                        if (val->cmd.type == mbox::command_type::input)
                        {
                            if (m_system.recv(val->conn, sizeof(mbox::command)) == error_type())
                            {
                                const auto ptr = reinterpret_cast<const input_message*>(reply.bytes.data());
                                event_user_recv_input msg;
                                msg.key = val->key;
                                ICY_ERROR(msg.data.append(ptr, ptr + reply.bytes.size() / sizeof(input_message)));
                                ICY_ERROR(event::post(nullptr, event_type_user_recv_input, std::move(msg)));
                                continue;
                            }
                        }
                        else if (val->cmd.type == mbox::command_type::image)
                        {
                            if (m_system.recv(val->conn, sizeof(mbox::command)) == error_type())
                            {
                                event_user_recv_image msg;
                                image img;
                                if (img.load(detail::global_heap, reply.bytes, image_type::png) == error_type())
                                {
                                    msg.colors = matrix<color>(img.size().y, img.size().x);
                                    if (msg.colors.empty())
                                        return make_stdlib_error(std::errc::not_enough_memory);
                                    msg.png = std::move(reply.bytes);
                                    ICY_ERROR(img.view({}, msg.colors));
                                    ICY_ERROR(event::post(nullptr, event_type_user_recv_image, std::move(msg)));
                                    continue;
                                }
                            }
                        }
                    }
                    else if (reply.bytes.size() == sizeof(mbox::command))
                    {
                        const auto cmd = reinterpret_cast<const mbox::command*>(reply.bytes.data());
                        if (cmd->version == mbox::protocol_version && cmd->size)
                        {
                            if (cmd->type == mbox::command_type::input || cmd->type == mbox::command_type::image)
                            {
                                val->cmd = *cmd;
                                if (m_system.recv(val->conn, cmd->size) == error_type())
                                    continue;
                            }
                        }
                    }
                }
                else if (reply.type == network_request_type::send)
                {
                    ICY_LOCK_GUARD(m_lock);
                    val->sending = false;

                    if (val->send.empty())
                        continue;
                    
                    const auto error = m_system.send(val->conn, val->send);
                    if (!error)
                    {
                        val->send.clear();
                        val->sending = true;
                        continue;
                    }
                }
            }

            if (val)
            {
                ICY_LOCK_GUARD(m_lock);
                ICY_ERROR(reset(*val));
            }
        }
        return {};
    }
    icy::error_type send(const mbox::key key, const icy::const_array_view<icy::input_message> input) noexcept
    {
        return send(key, input.data(), input.size() * sizeof(input[0]), mbox::command_type::input);
    }
    icy::error_type send(const mbox::key key, const icy::const_array_view<icy::rectangle> image) noexcept
    {
        return send(key, image.data(), image.size() * sizeof(image[0]), mbox::command_type::image);
    }
    icy::error_type disconnect(const mbox::key key) noexcept
    {
        return send(key, nullptr, 0, mbox::command_type::exit);
    }
    void cancel() noexcept override
    {
        m_system.stop(icy::network_code_exit);
    }
private:
    icy::error_type send(const mbox::key key, const void* data, const size_t size, const mbox::command_type type) noexcept
    {
        using namespace icy;
        ICY_LOCK_GUARD(m_lock);
        for (auto&& val : m_data)
        {
            if (val.key == key)
            {
                mbox::command cmd;
                cmd.type = type;
                cmd.size = uint32_t(size);
                ICY_ERROR(val.send.append(const_array_view<uint8_t>(reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd))));
                ICY_ERROR(val.send.append(const_array_view<uint8_t>(static_cast<const uint8_t*>(data), size)));
            }
        }
        ICY_ERROR(this->m_system.stop(network_code_update));
        return {};
    }        
private:
    icy::network_system_tcp m_system;
    icy::detail::spin_lock<> m_lock;
    icy::array<value_type> m_data;
};