#pragma once

#include "icy_event.hpp"
#include "icy_color.hpp"
#include "icy_key.hpp"
#include "icy_string.hpp"

namespace icy
{
    class console_system;
    class console_event
    {
        friend console_system;
    public:
        string text;
        color color;
        key key = key::none;
    private:
        bool internal = false;
    };
    class console_system : public event_system
    {
        struct tag { };
    public:
        friend error_type create_event_system(shared_ptr<console_system>& system) noexcept;
    public:
        console_system(tag) noexcept { }
        ~console_system() noexcept;
        error_type exec() noexcept override;
        error_type signal(const event_data& event) noexcept override;
        error_type write(const string_view str, const color foreground = color(colors::white, 0x00)) noexcept
        {
            console_event event;
            ICY_ERROR(copy(str, event.text));
            event.color = foreground;
            event.internal = true;
            return post(nullptr, event_type::console_write, std::move(event));
        }
        error_type read_line() noexcept
        {
            console_event event;
            event.internal = true;
            return post(nullptr, event_type::console_read_line, std::move(event));
        }
        error_type read_key() noexcept
        {
            console_event event;
            event.internal = true;
            return post(nullptr, event_type::console_read_key, std::move(event));
        }
    private:
        mutex m_mutex;
        cvar m_cvar;
    };
}