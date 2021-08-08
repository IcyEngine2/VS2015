#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_blob.hpp>

namespace icy
{
    struct window;
    struct window_system;
    struct window_cursor;
    struct window_render_item;
    struct render_surface;
    class thread;

    enum class window_style : uint32_t
    {
        windowed    =   0x00,
        popup       =   0x01,
        maximized   =   0x02,
    };
    enum class window_flags : uint32_t
    {
        none            =   0x00,
        quit_on_close   =   0x01,
        layered         =   0x02,
    };
    enum class window_action_type : uint32_t
    {
        none,
        undo,       //  ctrl + Z
        redo,       //  ctrl + Y
        cut,        //  ctrl + X
        copy,       //  ctrl + C
        paste,      //  ctrl + V
        save,       //  ctrl + S
        reload,     //  ctrl + R
        select_all, //  ctrl + A
    };

    static constexpr auto default_window_flags = window_flags::quit_on_close;

    inline constexpr bool operator&(const window_flags lhs, const window_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr window_flags operator|(const window_flags lhs, const window_flags rhs) noexcept
    {
        return window_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const window_style lhs, const window_style rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr window_style operator|(const window_style lhs, const window_style rhs) noexcept
    {
        return window_style(uint32_t(lhs) | uint32_t(rhs));
    }

    struct window
    {
        virtual ~window() noexcept = 0
        {

        }
        virtual shared_ptr<window_system> system() noexcept = 0;
        virtual uint32_t index() const noexcept = 0;
        virtual error_type restyle(const window_style style) noexcept = 0;
        virtual error_type rename(const string_view name) noexcept = 0;
        virtual error_type show(const bool value) noexcept = 0;
        virtual error_type cursor(const uint32_t priority, const shared_ptr<window_cursor> cursor) noexcept = 0;
        virtual error_type win_handle(void*& handle) const noexcept = 0;
        virtual window_flags flags() const noexcept = 0;
        virtual window_size size() const noexcept = 0;
        virtual uint32_t dpi() const noexcept = 0;
        virtual error_type repaint(const string_view tag, array<window_render_item>& items) noexcept = 0;
    };


    enum class window_state : uint32_t
    {
        none        =   0x00,
        closing     =   0x01,
        minimized   =   0x02,
        inactive    =   0x04,
        popup       =   0x08,
        maximized   =   0x10,
    };
    inline window_state operator|(const window_state lhs, const window_state rhs) noexcept
    {
        return window_state(uint32_t(lhs) | uint32_t(rhs));
    }

    struct window_message
    {
        uint32_t window = 0;
        window_state state = window_state::none;
        window_size size;
        input_message input;
    };
    struct window_copydata
    {
        uint32_t window = 0;
        uint64_t user = 0;
        array<uint8_t> bytes;
    };
    struct window_action
    {
        uint32_t window = 0;
        window_action_type type = window_action_type::none;
        string clipboard_text;
    };
    enum class window_render_item_type : uint32_t
    {
        none,
        clear,
        clip_push,
        clip_pop,
        rect,
        text,
        image,
    };
    struct window_render_item
    {
        window_render_item() noexcept = default;
        window_render_item(window_render_item&& rhs) noexcept : type(rhs.type),
            min_x(rhs.min_x), min_y(rhs.min_y), max_x(rhs.max_x), max_y(rhs.max_y),
            color(rhs.color), handle(rhs.handle), blob(std::move(rhs.blob))
        {
            rhs.handle = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(window_render_item);
        ~window_render_item() noexcept;
        window_render_item_type type = window_render_item_type::none;
        float min_x = 0;
        float min_y = 0;
        float max_x = 0;
        float max_y = 0;
        color color;
        void* handle = nullptr;
        blob blob;
    };
    struct window_cursor
    {
        enum class type : uint32_t
        {
            none,
            arrow,
            arrow_wait,
            wait,
            cross,
            hand,
            help,
            ibeam,
            no,
            size,
            size_x,
            size_y,
            size_diag0,
            size_diag1,
            _count,
        };
        virtual ~window_cursor() noexcept = 0
        {

        }
    };
    struct window_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        virtual icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const window_system*>(this)->thread());
        }
        virtual error_type create(shared_ptr<window>& window, const window_flags flags = default_window_flags) noexcept = 0;
    };

    error_type create_window_cursor(shared_ptr<window_cursor>& cursor, const window_cursor::type type) noexcept;
    error_type create_window_cursor(shared_ptr<window_cursor>& cursor, const const_array_view<uint8_t> bytes) noexcept;
    error_type create_window_system(shared_ptr<window_system>& system) noexcept;
}