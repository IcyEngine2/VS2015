#pragma once

#include "icy_event.hpp"
#include "icy_color.hpp"
#include "icy_key.hpp"
#include "icy_string.hpp"

namespace icy
{
    class thread;
    class console_system;
    struct console_event
    {
        string text;
        color color;
        key key = key::none;
        event_type _type = event_type::none;
    };
    class console_system : public event_system
    {
        struct tag { };
    public:
        friend error_type create_console_system(shared_ptr<console_system>& system) noexcept;
    public:
        console_system(tag) noexcept { }
        ~console_system() noexcept;
        virtual const icy::thread& thread() const noexcept
        {
            return *m_thread;
        }
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const console_system*>(this)->thread());
        }
        error_type write(const string_view str, const color foreground = color(colors::white, 0x00)) noexcept
        {
            console_event event;
            ICY_ERROR(copy(str, event.text));
            event.color = foreground;
            event._type = event_type::console_write;
            return event_system::post(nullptr, event_type::system_internal, std::move(event));
        }
        error_type read_line() noexcept
        {
            console_event event;
            event._type = event_type::console_read_line;
            return event_system::post(nullptr, event_type::system_internal, std::move(event));
        }
        error_type read_key() noexcept
        {
            console_event event;
            event._type = event_type::console_read_key;
            return event_system::post(nullptr, event_type::system_internal, std::move(event));
        }
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data* event) noexcept override;
    private:
        sync_handle m_sync;
        shared_ptr<icy::thread> m_thread;
    };
}