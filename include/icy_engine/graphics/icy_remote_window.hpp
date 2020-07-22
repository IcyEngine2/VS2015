#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_input.hpp>
#include "icy_render_math.hpp"

struct HWND__;

namespace icy
{
    class remote_window
    {
    public:
        static error_type enumerate(array<remote_window>& vec) noexcept;
        remote_window() noexcept = default;
        remote_window(const remote_window& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(remote_window);
        ~remote_window() noexcept;
    public:
        error_type name(string& str) const noexcept;
        uint32_t thread() const noexcept;
        uint32_t process() const noexcept;
        HWND__* handle() const noexcept;
        error_type screen(const icy::render_d2d_rectangle_u& rect, matrix_view<color>& colors) const noexcept;
        error_type hook(const library& lib, const char* func) noexcept;
        error_type send(const input_message& msg, const duration_type timeout = max_timeout) noexcept;
        error_type post(const uint32_t type, const size_t wparam, const ptrdiff_t lparam) noexcept;
        error_type rename(const string_view name) noexcept;
    private:
        class data_type;
        data_type* data = nullptr;
    };
}