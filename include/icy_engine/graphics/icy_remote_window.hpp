#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_event.hpp>
#include "icy_render_math.hpp"

struct HWND__;

namespace icy
{
    class remote_window
    {
    public:
        static error_type enumerate(array<remote_window>& vec, const uint32_t thread = 0) noexcept;
        static error_type unhook(const char* const lib) noexcept;
        remote_window() noexcept = default;
        remote_window(const remote_window& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(remote_window);
        ~remote_window() noexcept;
    public:
        explicit operator bool() const noexcept
        {
            return handle();
        }
        error_type name(string& str) const noexcept;
        uint32_t thread() const noexcept;
        uint32_t process() const noexcept;
        HWND__* handle() const noexcept;
        error_type screen(const icy::render_d2d_rectangle_u& rect, matrix_view<color>& colors) const noexcept;
        error_type hook(const char* const lib, const char* func) noexcept;
        error_type send(const input_message& msg, const duration_type timeout = max_timeout) noexcept;
        //error_type post(const uint32_t type, const size_t wparam, const ptrdiff_t lparam) noexcept;
        error_type rename(const string_view name) noexcept;
        error_type activate() noexcept;
    private:
        class data_type;
        data_type* data = nullptr;
    };

    class thread;
    extern const event_type remote_window_event_type;

    class remote_window_system : public event_system
    {
    public:
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const remote_window_system*>(this)->thread());
        }
        virtual error_type notify_on_close(const remote_window window) noexcept = 0;
    };
    error_type create_event_system(shared_ptr<remote_window_system>& system) noexcept;

    template<>
    inline int compare<HWND__*>(HWND__* const& lhs, HWND__* const& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
}