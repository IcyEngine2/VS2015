#pragma once

#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_event.hpp>

class input_thread : public icy::thread
{
public:
    static icy::shared_ptr<input_thread> instance;
    static icy::error_type initialize() noexcept
    {
        using namespace icy;
        ICY_ERROR(make_shared(input_thread::instance));
        ICY_ERROR(create_event_system(input_thread::instance->m_loop, event_type::network_any));
        ICY_ERROR(input_thread::instance->launch());
        ICY_ERROR(input_thread::instance->thread::rename("Input Thread"_s));
        return error_type();
    }
    static bool is_ready() noexcept
    {
        return instance && instance->m_send_ready;
    }
    static bool is_done() noexcept
    {
        return instance && instance->state() == icy::thread_state::done;
    }
    icy::error_type run() noexcept override;
    bool recv(icy::input_message& msg) noexcept
    {
        return m_recv_queue.pop(msg);
    }
    icy::error_type send(icy::input_message&& msg) noexcept
    {
        if (m_send_ready)
            ICY_ERROR(m_loop->post(nullptr, icy::event_type::system_internal, std::move(msg)));
        return icy::error_type();
    }
private:
    std::atomic<bool> m_send_ready = false;
    icy::mpsc_queue<icy::input_message> m_recv_queue;
    icy::shared_ptr<icy::event_queue> m_loop;
};