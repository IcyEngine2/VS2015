#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <typeinfo>

namespace icy
{
    const auto mm_to_point = 2.83465;
    struct gui_system;
    struct input_message;

    enum class gui_widget_type : uint32_t
    {
        none,
        window,
        line_edit,
        text_edit,
    };
    enum class gui_align : int32_t
    {
        min = -1,
        mid =  0,
        max = +1,
    };
    enum class gui_style_prop : uint32_t
    {
        none,
        bkcolor,        //  color
        border_lhs,     //  length
        border_top,     //  length
        border_rhs,     //  length
        border_bot,     //  length
        border_bkcolor, //  color

        font_stretch,   //  integer 0-9
        font_weight,    //  integer 0-1000
        font_size,      //  integer
        font_italic,    //  boolean
        font_oblique,   //  boolean
        font_strike,    //  boolean
        font_underline, //  boolean
        font_spacing,   //  integer
        font_align_text,//  gui_align
        font_align_par, //  gui_align
        font_trimming,  //  integer 0, 1, 2 (none | char | word)
        font_name,      //  string
        font_color,     //  color

        cursor,             //  window_cursor::type
        cursor_move,        //  window_cursor::type
        cursor_resize_x,    //  window_cursor::type
        cursor_resize_y,    //  window_cursor::type
        cursor_resize_diag0,//  window_cursor::type
        cursor_resize_diag1,//  window_cursor::type

        _count,
    };
    enum class gui_widget_prop : uint32_t
    {
        none,

        enabled,        //  boolean
        visible,        //  boolean
        style_default,  //  unsigned integer (reference)
        style_disabled, //  unsigned integer
        style_focused,  //  unsigned integer
        style_hovered,  //  unsigned integer
        bkcolor,        //  color
        size_x,         //  length
        size_min_x,     //  length
        size_max_x,     //  length
        size_y,         //  length
        size_min_y,     //  length
        size_max_y,     //  length
        offset_x,       //  length
        offset_y,       //  length
        layout,         //  boolean (0 - horizontal, 1 - vertical)
        anchor_x,       //  length (in-widget reference point: x)
        anchor_y,       //  length (in-widget reference point: y)
        weight,         //  float (default : 1)
        layer,          //  unsigned integer (0.. 255)
        resizeable,     //  boolean
        
        _count,
    };
    enum class gui_node_prop : uint32_t
    {
        none,

        enabled,
        visible,
        data,
        user,

        _count,

    };
    enum class gui_length_type : uint32_t
    {
        _default,
        pixel = _default,
        point,
        relative,
    };
    template<typename T>
    static constexpr uint32_t gui_variant_type() noexcept
    {
        return constexpr_hash(typeid(T).name());
    }
    struct window_message;

    struct gui_length
    {
        gui_length() noexcept : value(0), type(gui_length_type::_default)
        {

        }
        gui_length(const double value, const gui_length_type type) noexcept : value(value), type(type)
        {

        }
       /*gui_length(const gui_length& rhs) noexcept : value(rhs.value), type(rhs.type)
        {

        }
        gui_length(gui_length&& rhs) noexcept : value(rhs.value), type(rhs.type)
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(gui_length);
        ICY_DEFAULT_MOVE_ASSIGN(gui_length);*/
        double value;
        gui_length_type type;
    };

    namespace detail
    {
        template<typename T>
        inline error_type convert(const uint32_t type, const void* const data, const size_t size, T& value) noexcept;

        inline error_type convert(const uint32_t type, const void* const data, const size_t size, bool& value) noexcept
        {
            value = false;
            for (auto k = 0u; k < size; ++k)
            {
                if (static_cast<const uint8_t*>(data)[k] != 0)
                {
                    value = true;
                    break;
                }
            }
            return error_type();
        }
        error_type convert(const uint32_t type, const void* const data, const size_t size, gui_length& length) noexcept;      
        error_type convert(const uint32_t type, const void* const data, const size_t size, color& color) noexcept;

        template<typename T>
        inline error_type convert(const uint32_t type, const void* const data, const size_t size, T& value, std::true_type) noexcept
        {
            if (type == gui_variant_type<bool>())
                value = T(*static_cast<const bool*>(data));
            else if (type == gui_variant_type<uint8_t>())
                value = T(*static_cast<const uint8_t*>(data));
            else if (type == gui_variant_type<int8_t>())
                value = T(*static_cast<const int8_t*>(data));
            else if (type == gui_variant_type<uint16_t>())
                value = T(*static_cast<const uint16_t*>(data));
            else if (type == gui_variant_type<int16_t>())
                value = T(*static_cast<const uint16_t*>(data));
            else if (type == gui_variant_type<uint32_t>())
                value = T(*static_cast<const uint32_t*>(data));
            else if (type == gui_variant_type<int32_t>())
                value = T(*static_cast<const int32_t*>(data));
            else if (type == gui_variant_type<uint64_t>())
                value = T(*static_cast<const uint64_t*>(data));
            else if (type == gui_variant_type<int64_t>())
                value = T(*static_cast<const int64_t*>(data));
            else if (type == gui_variant_type<float>())
                value = T(*static_cast<const float*>(data));
            else if (type == gui_variant_type<double>())
                value = T(*static_cast<const double*>(data));
            else if (type == gui_variant_type<string_view>())
            {
                const string_view str(static_cast<const char*>(data), size);
                return to_value(str, value);
            }
            else
                return make_stdlib_error(std::errc::invalid_argument);

            return error_type();
        }
        template<typename T>
        inline error_type convert(const uint32_t type, const void* const data, const size_t size, T& value, std::false_type) noexcept
        {
            return make_stdlib_error(std::errc::invalid_argument);
        }
        template<typename T>
        inline error_type convert(const uint32_t type, const void* const data, const size_t size, T& value) noexcept
        {
            const auto is_numeric = 
                std::is_integral<remove_cvr<T>>::value || 
                std::is_floating_point<remove_cvr<T>>::value;
            return convert(type, data, size, value, std::bool_constant<is_numeric>());
        }
    }

    class gui_variant
    {
        gui_variant(const uint32_t type, uint32_t size, const void* ptr) noexcept : m_type(type)
        {
            memset(m_buffer, 0, sizeof(m_buffer));
            uint32_t dynamic = size > max_buffer;
            if (dynamic)
            {
                auto data = reinterpret_cast<dynamic_type*>(allocator_type::allocate<uint8_t>(size + sizeof(std::atomic<uint32_t>)));
                if (!data)
                {
                    dynamic = 0;
                    size = 0;
                    m_type = 0;
                }
                else
                {
                    m_data = data;
                    new (&m_data->refs) std::atomic<uint32_t>(1);
                    memcpy(m_data->bytes, ptr, size);
                }
            }
            else
            {
                memcpy(m_buffer, ptr, size);
            }
            m_size = (size << shift_size) | (dynamic << shift_flag);
        }
    public:
        ~gui_variant() noexcept
        {
            if (is_dynamic())
            {
                if (m_data->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    allocator_type::deallocate(m_data);
            }
        }
        ICY_DEFAULT_COPY_ASSIGN(gui_variant);
        ICY_DEFAULT_MOVE_ASSIGN(gui_variant);
        template<typename T> 
        gui_variant(const T& rhs) noexcept : gui_variant(gui_variant_type<T>(), sizeof(T), &rhs)
        {
            static_assert(std::is_trivially_destructible<T>::value, "INVALID GUI TYPE");
        }
        template<typename T>
        gui_variant(const const_array_view<T> rhs) noexcept :
            gui_variant(gui_variant_type<const_array_view<T>>(), uint32_t(rhs.size()) * sizeof(T), rhs.data())
        {
            static_assert(std::is_trivially_destructible<T>::value, "INVALID GUI TYPE");
        }
        gui_variant(gui_variant&& rhs) noexcept : m_size(rhs.m_size), m_type(rhs.m_type)
        {
            rhs.m_size = rhs.m_type = 0;
            if (is_dynamic())
            {
                m_data = rhs.m_data;
                m_data->refs.fetch_add(1, std::memory_order_release);
            }
            else
            {
                memcpy(m_buffer, rhs.m_buffer, max_buffer);
            }
        }
        gui_variant(const gui_variant& rhs) noexcept : m_size(rhs.m_size), m_type(rhs.m_type)
        {
            if (is_dynamic())
            {
                m_data = rhs.m_data;
                m_data->refs.fetch_add(1, std::memory_order_release);
            }
            else
            {
                memcpy(m_buffer, rhs.m_buffer, size());
            }
        }
        gui_variant() noexcept : m_size(0), m_type(0)
        {
            memset(m_buffer, 0, sizeof(m_buffer));
        }
        gui_variant(const string_view str) noexcept : 
            gui_variant(gui_variant_type<string_view>(), uint32_t(str.bytes().size()), str.bytes().data())
        {

        }
        gui_variant(const char*) noexcept = delete;
        bool is_dynamic() const noexcept
        {
            return (m_size & mask_flag) >> shift_flag;
        }
        uint32_t type() const noexcept
        {
            return m_type;
        }
        size_t size() const noexcept
        {
            return (m_size & mask_size) >> shift_size;
        }
        template<typename T>
        error_type get(T& value) const noexcept
        {
            const void* data = is_dynamic() ? m_data->bytes : m_buffer;
            if (type() == gui_variant_type<T>())
            {
                return copy(*static_cast<const T*>(data), value);
            }
            else
            {
                return detail::convert(type(), data, size(), value);
            }
        }
        string_view to_string() const noexcept
        {
            const void* data = is_dynamic() ? m_data->bytes : m_buffer;
            const auto is_string = type() == gui_variant_type<string_view>();
            return is_string ? string_view(static_cast<const char*>(data), size()) : string_view();
        }
        template<typename T>
        T as() const noexcept
        {
            T value = {};
            get(value);
            return value;
        }
    private:
        enum : uint32_t
        {
            shift_size  =   0x00,
            shift_flag  =   0x1F,
            mask_size   =   0x7FFFFFFF,
            mask_flag   =   0x80000000,
            max_buffer  =   24,
        };
        struct dynamic_type
        {
            std::atomic<uint32_t> refs = 1;
            uint8_t bytes[1];
        };
        uint32_t m_type = 0;
        uint32_t m_size = 0;
        union
        {
            dynamic_type* m_data;
            uint8_t m_buffer[max_buffer];
        };
    };
    struct gui_model;
    struct gui_node
    {
        virtual ~gui_node() noexcept = 0
        {
             
        }
        virtual shared_ptr<gui_model> model() const noexcept = 0;
        virtual shared_ptr<gui_node> parent() const noexcept = 0;
        virtual uint32_t row() const noexcept = 0;
        virtual uint32_t col() const noexcept = 0;
        virtual shared_ptr<gui_node> child(const uint32_t row, const uint32_t col) = 0;
    };
    struct gui_model
    {
        virtual ~gui_model() noexcept = 0
        {

        }
        virtual shared_ptr<gui_node> root() const noexcept = 0;
    };
    struct gui_request
    {        
        virtual ~gui_request() noexcept = 0
        {

        }
        virtual error_type exec() noexcept = 0;
        virtual error_type create_widget(uint32_t& widget, const gui_widget_type type, const uint32_t parent = 0) noexcept = 0;
        virtual error_type create_style(uint32_t& style, const const_array_view<uint32_t> parents = const_array_view<uint32_t>()) noexcept = 0;
        virtual error_type destroy_widget(const uint32_t widget) noexcept = 0;
        virtual error_type destroy_style(const uint32_t style) noexcept = 0;
        virtual error_type destroy_node(const gui_node& node) noexcept = 0;
        virtual error_type modify_widget(const uint32_t widget, const gui_widget_prop prop, const gui_variant& var) noexcept = 0;
        virtual error_type modify_style(const uint32_t style, const gui_style_prop prop, const gui_variant& var) noexcept = 0;
        virtual error_type modify_node(const gui_node& node, const gui_node_prop prop, const gui_variant& var) noexcept = 0;
        virtual error_type send_input(const input_message& message) noexcept = 0;
        virtual error_type render(uint32_t& oper, const window_size size, const render_flags flags) noexcept = 0;
        virtual error_type query_widget(uint32_t& oper, const uint32_t widget, const gui_widget_prop prop) noexcept = 0;
        virtual error_type query_style(uint32_t& oper, const uint32_t style, const gui_style_prop prop) noexcept = 0;
        virtual error_type query_node(uint32_t& oper, const gui_node& node, const gui_node_prop prop) noexcept = 0;
        virtual error_type bind(const uint32_t widget, const shared_ptr<gui_node> node) noexcept = 0;
    };
    struct gui_message
    {
        uint32_t oper = 0;
        uint32_t index = 0;     //  widget or style
        uint32_t prop = 0;      //  gui_widget_prop or gui_style_prop
        gui_variant var;
        shared_ptr<texture> texture;
    };
    class thread;
    struct window;

    struct gui_system : public event_system
    {
        virtual const icy::thread& thread() const noexcept = 0;
        icy::thread& thread() noexcept
        {
            return const_cast<icy::thread&>(static_cast<const gui_system*>(this)->thread());
        }
        virtual error_type enum_font_names(array<string>& fonts) const noexcept = 0;
        virtual error_type enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept = 0;
        virtual error_type create_request(shared_ptr<gui_request>& data) noexcept = 0;
        virtual error_type create_model(shared_ptr<gui_model>& model) noexcept = 0;
    };
    error_type create_gui_system(shared_ptr<gui_system>& system, const adapter adapter, const shared_ptr<window> window) noexcept;
}