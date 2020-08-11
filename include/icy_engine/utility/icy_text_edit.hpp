#pragma once

#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_thread.hpp>

struct HWND__;

#include "../../../source/libs/scintilla/include/SciLexer.h"

namespace icy
{
    static const auto text_edit_max_keywords = 9;
    enum class text_edit_flag : uint32_t
    {
        none        =   0x00,
        italic      =   0x01,
        bold        =   0x02,
        underline   =   0x04,
        hidden      =   0x08,
        readonly    =   0x10,
        hotspot     =   0x20,
        case_upper  =   0x40,
        case_lower  =   0x80,
        case_camel  =   case_upper | case_lower,
    };
    inline text_edit_flag operator|(const text_edit_flag lhs, const text_edit_flag rhs) noexcept
    {
        return text_edit_flag(uint32_t(lhs) | uint32_t(rhs));
    }
    inline bool operator&(const text_edit_flag lhs, const text_edit_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }

    struct text_edit_style
    {
        color foreground;
        color background;
        string font;
        double size = 0;
        int weight = 0;
        text_edit_flag flags = text_edit_flag::none;
    };
    struct text_edit_lexer
    {
        error_type initialize(const string_view name) noexcept
        {
            ICY_ERROR(copy(name, this->name));
            return error_type();
        }
        error_type add_style(const int key, const color foreground, const text_edit_flag flags = text_edit_flag::none) noexcept
        {
            text_edit_style new_style;
            new_style.foreground = foreground;
            new_style.flags = flags;
            return styles.insert(key, std::move(new_style));
        }

        string name;
        string keywords[text_edit_max_keywords];
        map<int, text_edit_style> styles;
    };
    class text_edit_window
    {
    public:
        text_edit_window() noexcept = default;
        text_edit_window(const text_edit_window&) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(text_edit_window);
        ~text_edit_window() noexcept;
    public:
        HWND__* hwnd() const noexcept;
        uint32_t index() const noexcept;
        error_type show(const bool value) noexcept;
        error_type text(const string_view value) noexcept;
        error_type lexer(text_edit_lexer&& lexer) noexcept;
        error_type save() noexcept;
        error_type undo() noexcept;
        error_type redo() noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
    class text_edit_system : public event_system
    {
    public:
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const text_edit_system*>(this)->thread());
        }
        virtual error_type create(text_edit_window& window) const noexcept = 0;
    };
    error_type create_event_system(shared_ptr<text_edit_system>& system) noexcept;

    extern const event_type text_edit_event_type;
    struct text_edit_event
    {
        enum class type : uint32_t
        {
            none,
            close,
            insert,
            remove,
        };
        type type = type::none;
        uint32_t window = 0;
        uint32_t offset = 0;
        uint32_t length = 0;
        array<char> text;
        bool can_undo = false;
        bool can_redo = false;
    };
}