#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <typeinfo>

namespace icy
{
    template<typename T>
    static constexpr uint32_t gui_variant_type() noexcept
    {
        return constexpr_hash(typeid(remove_cvr<T>).raw_name());
    }

    class gui_variant
    {
    public:
        gui_variant() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        ~gui_variant() noexcept
        {
            if (m_flag)
            {
                if (m_dynamic->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    if (m_dynamic->destructor)
                        m_dynamic->destructor(m_dynamic->bytes);
                    allocator_type::destroy(m_dynamic);
                    allocator_type::deallocate(m_dynamic);
                }
            }
        }
        gui_variant(const gui_variant& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            if (m_flag)
                m_dynamic->ref.fetch_add(1, std::memory_order_release);
        }
        gui_variant(gui_variant&& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            memset(&rhs, 0, sizeof(rhs));
        }
        ICY_DEFAULT_COPY_ASSIGN(gui_variant);
        ICY_DEFAULT_MOVE_ASSIGN(gui_variant);
        template<typename T, typename = std::enable_if_t<!std::is_same<remove_cvr<T>, string>::value>>
        gui_variant(T&& rhs) noexcept : gui_variant()
        {
            using U = remove_cvr<T>;
            static_assert(!std::is_same<U, string>::value, "STRING NOT ALLOWED");
            if (std::is_trivially_destructible<U>::value)
            {
                if (sizeof(U) <= max_size)
                {
                    allocator_type::construct(reinterpret_cast<U*>(m_buffer), std::forward<T>(rhs));
                }
                else
                {
                    m_dynamic = reinterpret_cast<dynamic_data*>(allocator_type::allocate<char>(sizeof(dynamic_data) + sizeof(T) - 1));
                    if (!m_dynamic)
                        return;

                    allocator_type::construct(m_dynamic);
                    allocator_type::construct(reinterpret_cast<U*>(m_dynamic->bytes), std::forward<T>(rhs));
                    m_flag = 1;
                }
            }
            else
            {
                m_dynamic = reinterpret_cast<dynamic_data*>(allocator_type::allocate<char>(sizeof(dynamic_data) + sizeof(T) - 1));
                if (!m_dynamic)
                    return;

                allocator_type::construct(m_dynamic);
                m_dynamic->destructor = [](void* ptr) { allocator_type::destroy(static_cast<U*>(ptr)); };
                allocator_type::construct(reinterpret_cast<U*>(m_dynamic->bytes), std::forward<T>(rhs));
                m_flag = 1;
            }
            m_type = gui_variant_type<T>();
            m_size = sizeof(rhs);
        }
        gui_variant(const string& str) noexcept : gui_variant(static_cast<string_view>(str))
        {

        }
        gui_variant(const string_view str) noexcept : gui_variant()
        {
            if (str.bytes().size() >= max_size)
            {
                m_dynamic = reinterpret_cast<dynamic_data*>(allocator_type::allocate<char>(sizeof(dynamic_data) + str.bytes().size()));
                if (!m_dynamic)
                    return;

                allocator_type::construct(m_dynamic);
                memcpy(m_dynamic->bytes, str.bytes().data(), str.bytes().size());
                m_dynamic->bytes[str.bytes().size()] = 0;
                m_flag = 1;
            }
            else
            {
                memcpy(m_buffer, str.bytes().data(), str.bytes().size());
            }
            m_size = str.bytes().size();
            m_type = gui_variant_type<string_view>();
        }
        explicit operator bool() const noexcept
        {
            return m_type != 0;
        }
        uint32_t type() const noexcept
        {
            return m_type;
        }
        uint32_t size() const noexcept
        {
            return m_size;
        }
        void* data() noexcept
        {
            return m_flag ? m_dynamic->bytes : m_buffer;
        }
        const void* data() const noexcept
        {
            return m_flag ? m_dynamic->bytes : m_buffer;
        }
        template<typename T> const T* get() const noexcept
        {
            return m_type == gui_variant_type<remove_cvr<T>>() ? static_cast<const T*>(data()) : nullptr;
        }
        template<typename T> bool get(T& value) const noexcept
        {
            if (auto ptr = get<T>())
            {
                value = *ptr;
                return true;
            }
            return false;
        }
        template<typename T> T* get() noexcept
        {
            return const_cast<T*>(static_cast<const gui_variant*>(this)->get<T>());
        }
        string_view str() const noexcept
        {
            return type() == gui_variant_type<string_view>() ?
                string_view(static_cast<const char*>(data()), size()) : string_view();
        }
    private:
        enum { max_size = 24 };
        struct dynamic_data
        {
            std::atomic<uint32_t> ref = 1;
            void(*destructor)(void*) = nullptr;
            char bytes[1];
        };
    private:
        uint32_t m_type;
        struct
        {
            uint32_t m_size : 0x1F;
            uint32_t m_flag : 0x01;
        };
        union
        {
            char m_buffer[24];
            dynamic_data* m_dynamic;
        };
    };
    inline error_type to_string(const gui_variant& var, string& str) noexcept
    {
        if (var.type() == gui_variant_type<string_view>())
        {
            return copy(var.str(), str);
        }
        const auto func = [&var, &str](auto& x, error_type& error)
        {
            if (var.get(x))
            {
                error = to_string(x, str); 
                return true;
            }
            else
            {
                return false;
            }            
        };
        error_type error;
        union
        {
            bool as_boolean;
            uint8_t as_uint8;
            uint16_t as_uint16;
            uint32_t as_uint32;
            uint64_t as_uint64;
            int8_t as_int8;
            int16_t as_int16;
            int32_t as_int32;
            int64_t as_int64;
            float as_float32;
            double as_float64;
        };
        if (func(as_boolean, error)) return error;
        if (func(as_uint8, error)) return error;
        if (func(as_uint16, error)) return error;
        if (func(as_uint32, error)) return error;
        if (func(as_uint64, error)) return error;
        if (func(as_int8, error)) return error;
        if (func(as_int16, error)) return error;
        if (func(as_int32, error)) return error;
        if (func(as_int64, error)) return error;
        if (func(as_float32, error)) return error;
        if (func(as_float64, error)) return error;
        return make_stdlib_error(std::errc::invalid_argument);
    }
    inline bool to_value(const gui_variant& var, color& output) noexcept
    {
        if (var.type() == gui_variant_type<color>())
        {
            return var.get(output);
        }
        else if (var.type() == gui_variant_type<colors>())
        {
            colors value;
            if (var.get(value))
            {
                output = value;
                return true;
            }
        }
        else if (var.type() == gui_variant_type<uint32_t>())
        {
            uint32_t value = 0;
            if (var.get(value))
            {
                output = color::from_rgba(value);
                return true;
            }
        }
        else if (var.type() == gui_variant_type<string_view>())
        {
            auto str = var.str();
            auto sym = 0u;
            output = color::from_rgba(0xFF000000);
            for (auto it = str.begin(); it != str.end(); ++it, ++sym)
            {
                char32_t chr = 0;
                if (it.to_char(chr))
                    return false;
                
                uint32_t num = 0x10;
                switch (chr)
                {
                case '0': 
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    num = chr - '0';
                    break;
                case 'a':
                case 'A':
                    num = 0x0A;
                    break;

                case 'b':
                case 'B':
                    num = 0x0B;
                    break;

                case 'c':
                case 'C':
                    num = 0x0C;
                    break;

                case 'd':
                case 'D':
                    num = 0x0D;
                    break;

                case 'e':
                case 'E':
                    num = 0x0E;
                    break;

                case 'f':
                case 'F':
                    num = 0x0F;
                    break;
                }
                if (num >= 0x10)
                    break;

                switch (sym)
                {
                case 0:
                    output.r = num * 0x10;
                    break;
                case 1:
                    output.r += num;
                    break;
                case 2:
                    output.g = num * 0x10;
                    break;
                case 3:
                    output.g += num;
                    break;
                case 4:
                    output.b = num * 0x10;
                    break;
                case 5:
                    output.b += num;
                    break;
                case 6:
                    output.a = num * 0x10;
                    break;
                case 7:
                    output.a += num;
                    break;
                }
            }
            if (sym == 6 || sym == 8)
            {
                return true;
            }
            else
            {
                output = color();
                for (auto k = 1u; k < uint32_t(colors::_count); ++k)
                {
                    const auto color_str = to_string(colors(k));
                    if (color_str == str)
                    {
                        output = color(colors(k));
                        return true;
                    }
                }
            }
        }
        return false;
    }
    template<typename T>
    inline bool to_value_numeric(const gui_variant& var, T& output) noexcept
    {
        if (var.type() == gui_variant_type<T>())
        {
            return var.get(output);
        }

        const auto func = [&var, &output](auto& x)
        {
            if (var.get(x))
            {
                output = T(x);
                return true;
            }
            else
            {
                return false;
            }
        };
        union
        {
            bool as_boolean;
            uint8_t as_uint8;
            uint16_t as_uint16;
            uint32_t as_uint32;
            uint64_t as_uint64;
            int8_t as_int8;
            int16_t as_int16;
            int32_t as_int32;
            int64_t as_int64;
            float as_float32;
            double as_float64;
        };
        if (func(as_boolean)) return true;
        if (func(as_uint8)) return true;
        if (func(as_uint16)) return true;
        if (func(as_uint32)) return true;
        if (func(as_uint64)) return true;
        if (func(as_int8)) return true;
        if (func(as_int16)) return true;
        if (func(as_int32)) return true;
        if (func(as_int64)) return true;
        if (func(as_float32)) return true;
        if (func(as_float64)) return true;
        return false;
    }
    inline bool to_value(const gui_variant& var, float& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, double& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, uint8_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, uint16_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, uint32_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, uint64_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, int8_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, int16_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, int32_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, int64_t& output) noexcept
    {
        return to_value_numeric(var, output);
    }
    inline bool to_value(const gui_variant& var, bool& output) noexcept
    {
        if (var.type() == gui_variant_type<bool>())
            return var.get(output);

        if (var.type() == gui_variant_type<string_view>())
        {
            switch (hash(var.str()))
            {
            case "0"_hash:
            case "false"_hash:
            case "False"_hash:
            case "FALSE"_hash:
                output = false;
                return true;
            case "1"_hash:
            case "true"_hash:
            case "True"_hash:
            case "TRUE"_hash:
                output = true;
                return true;
            default:
                return false;
            }
        }
        uint64_t value = 0;
        if (to_value(var, value))
        {
            if (value == 0)
            {
                output = false;
                return true;
            }
            else if (value == 1)
            {
                output = true;
                return true;
            }
        }
        return false;
    }

    enum class gui_node_prop : uint32_t
    {
        none,

        enabled,
        visible,
        data,
        user,
        checked,
        checkable,

        _count,
    };
    enum class gui_widget_type : uint32_t
    {
        none,

        window_main,
        window_popup,
        menu_main,
        menu_popup,
        view_combo,
        view_list,
        view_tree,
        view_table,
        edit_line,
        edit_text,

        _count,
    };
    struct gui_widget_state_enum
    {
        enum : uint32_t
        {
            none            =   0x0000,
            enabled         =   0x0001,
            visible         =   0x0002,

            is_hovered      =   0x0004,
            is_focused      =   0x0008,
            is_pressed      =   0x0010,

            vscroll         =   0x0020,
            vscroll_auto    =   0x0040,
            vscroll_inv     =   0x0080,
            hscroll         =   0x0100,
            hscroll_auto    =   0x0200,
            hscroll_inv     =   0x0400,

            has_vscroll     =   0x0800,
            has_hscroll     =   0x1000,

            _default        =   enabled | visible | vscroll_auto | hscroll_auto,
        };
    };
    using gui_widget_state = decltype(gui_widget_state_enum::none);

    enum class gui_widget_layout : uint32_t
    {
        none,
        vbox,
        hbox,
        grid,
        flex,
    };


    class thread;
    class adapter;
    struct window;

    struct gui_node
    {
        uint32_t index = 0;
    };
    struct gui_widget
    {
        uint32_t index = 0;
    };
    struct gui_data_model
    {
        virtual ~gui_data_model() noexcept = 0
        {

        }
        virtual error_type update() noexcept = 0;
        virtual gui_node parent(const gui_node node) const noexcept = 0;
        virtual uint32_t row(const gui_node node) const noexcept = 0;
        virtual uint32_t col(const gui_node node) const noexcept = 0;
        virtual gui_variant query(const gui_node node, const gui_node_prop prop) const noexcept = 0;
        virtual error_type modify(const gui_node node, const gui_node_prop prop, const gui_variant& value) noexcept = 0;
        virtual error_type insert(const gui_node parent, const uint32_t row, const uint32_t col, gui_node& node) noexcept = 0;
        virtual error_type destroy(const gui_node node) noexcept = 0;
    };
    struct texture;
    struct gui_window
    {
        virtual ~gui_window() noexcept = 0
        {

        }
        virtual error_type render(const gui_widget widget, uint32_t& query) const noexcept = 0;
        virtual error_type resize(const window_size size) noexcept = 0;
        virtual error_type update() noexcept = 0;
        virtual gui_widget parent(const gui_widget widget) const noexcept = 0;
        virtual gui_widget child(const gui_widget widget, const size_t offset) const noexcept = 0;
        virtual size_t offset(const gui_widget widget) const noexcept = 0;
        virtual gui_variant query(const gui_widget widget, const string_view prop) const noexcept = 0;
        virtual gui_widget_type type(const gui_widget widget) const noexcept = 0;
        virtual gui_widget_layout layout(const gui_widget widget) const noexcept = 0;
        virtual gui_widget_state state(const gui_widget widget) const noexcept = 0;
        virtual error_type modify(const gui_widget widget, const string_view prop, const gui_variant& value) noexcept = 0;
        virtual error_type insert(const gui_widget parent, const size_t offset, const gui_widget_type type, const gui_widget_layout layout, gui_widget& widget) noexcept = 0;
        virtual error_type destroy(const gui_widget widget) noexcept = 0;
        virtual error_type find(const string_view prop, const gui_variant value, array<gui_widget>& list) const noexcept = 0;
    };
    struct gui_system;
    struct gui_data_bind
    {
        virtual ~gui_data_bind() noexcept = 0
        {

        }
        virtual int compare(const gui_data_model& model, const gui_node lhs, const gui_node rhs) const noexcept
        {
            return icy::compare(lhs.index, rhs.index);
        }
        virtual bool filter(const gui_data_model& model, const gui_node node) const noexcept
        {
            return true;
        }
    };
    struct gui_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const gui_system*>(this)->thread());
        }
        virtual error_type create_bind(unique_ptr<gui_data_bind>&& bind, gui_data_model& model, gui_window& window, const gui_node node, const gui_widget widget) noexcept = 0;
        virtual error_type create_model(shared_ptr<gui_data_model>& model) noexcept = 0;
        virtual error_type create_window(shared_ptr<gui_window>& window, shared_ptr<icy::window> handle, icy::string_view json) noexcept = 0;
        virtual error_type enum_font_names(array<string>& fonts) const noexcept = 0;
    };
    error_type create_gui_system(shared_ptr<gui_system>& system, const adapter adapter) noexcept;

    struct gui_event
    {
        uint32_t query = 0;
        shared_ptr<gui_window> window;
        gui_widget widget;
        icy::shared_ptr<texture> texture;
    };
}