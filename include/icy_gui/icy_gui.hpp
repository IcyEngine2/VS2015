#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/graphics/icy_texture.hpp>
#include <typeinfo>

namespace icy
{
    const auto mm_to_point = 2.83465;
    struct gui_system;
    enum class gui_widget_type : uint32_t
    {
        none,
        window,
        font,
    };
    enum class gui_widget_prop : uint32_t
    {
        none,
        enabled,        //  boolean
        visible,        //  boolean
        parent,         //  integer (reference to widget)
        font,           //  integer (reference to font)
        
        size_width,     //  length
        size_min_width, //  length
        size_max_width, //  length
        size_height,    //  length
        size_min_height,//  length
        size_max_height,//  length
        bkcolor,        //  color
        offset_x,       //  length
        offset_y,       //  length
        border_lhs,     //  length
        border_top,     //  length
        border_rhs,     //  length
        border_bot,     //  length
        border_color,   //  color

        font_stretch,   //  integer 0-9
        font_weight,    //  integer 0-1000
        font_size,      //  integer
        font_italic,    //  boolean
        font_oblique,   //  boolean
        font_strike,    //  boolean
        font_underline, //  boolean
        font_spacing,   //  integer
        font_align_text,//  integer -1, 0, 1
        font_align_par, //  integer -1, 0, 1
        font_trimming,  //  integer 0, 1, 2 (none | char | word)
        font_name,      //  string

        
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
    class gui_action
    {       
    public:
        struct data_type
        {
            uint32_t widget;
            gui_widget_prop prop;
            gui_variant var;
        };
    public:
        gui_action(gui_system& system) : m_system(system)
        {

        }
        error_type exec() noexcept;
        error_type create(const gui_widget_type type, uint32_t& index) noexcept;
        error_type destroy(const uint32_t index) noexcept;
        error_type modify(const uint32_t index, const gui_widget_prop prop, const gui_variant& var) noexcept;
    private:
        gui_system& m_system;
        array<data_type> m_data;
    };
    struct render_texture;
    struct gui_system : public event_system
    {
        virtual render_flags flags() const noexcept = 0;
        virtual error_type enum_font_names(array<string>& fonts) const noexcept = 0;
        virtual error_type enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept = 0;
        virtual error_type update() noexcept = 0;
        virtual error_type process(const event& event) noexcept = 0;
        virtual gui_action action() noexcept = 0;        
        virtual gui_widget_type type(const uint32_t index) const noexcept = 0;
        virtual gui_variant query(const uint32_t index, const gui_widget_prop prop) const noexcept = 0;
        virtual error_type render(render_texture& texture) noexcept = 0;
    };
    error_type create_gui_system(shared_ptr<gui_system>& gui, const render_flags flags, const adapter adapter, void* const handle = nullptr) noexcept;
}