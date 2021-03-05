#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <typeinfo>

namespace icy
{
    template<typename T>
    static constexpr uint32_t gui_variant_type() noexcept
    {
        return constexpr_hash(typeid(remove_cvr<T>).name());
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
            if (m_dynamic)
                m_dynamic->ref.fetch_add(1, std::memory_order_release);
        }
        gui_variant(gui_variant&& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            memset(&rhs, 0, sizeof(rhs));
        }
        ICY_DEFAULT_COPY_ASSIGN(gui_variant);
        ICY_DEFAULT_MOVE_ASSIGN(gui_variant);
        template<typename T>
        gui_variant(T&& rhs) noexcept : gui_variant()
        {
            using U = remove_cvr<T>;
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

    enum class gui_node_prop : uint32_t
    {
        none,

        enabled,
        visible,
        data,
        user,

        _count,

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
    struct gui_model
    {
        virtual ~gui_model() noexcept = 0
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
    struct gui_layout
    {
        error_type initialize(const string_view html) noexcept;
        string type;
        string text;
        array<gui_layout> children;
        map<string, string> attributes;
        uint32_t widget = 0;
    };
    struct texture;
    struct gui_window
    {
        virtual ~gui_window() noexcept = 0
        {

        }  
        virtual gui_widget child(const gui_widget parent, const size_t offset) noexcept = 0;
        virtual error_type insert(const gui_widget parent, const size_t offset, const string_view type, gui_widget& widget) noexcept = 0;
        virtual error_type erase(const gui_widget widget) noexcept = 0;
        virtual error_type layout(const gui_widget widget, gui_layout& layout) noexcept = 0;
        virtual error_type render(const gui_widget widget, uint32_t& query) const noexcept = 0;
        virtual error_type resize(const window_size size) noexcept = 0;
    };
    struct gui_system;
    struct gui_bind
    {
        virtual ~gui_bind() noexcept = 0
        {

        }
        virtual int compare(const gui_model& model, const gui_node lhs, const gui_node rhs) noexcept
        {
            return icy::compare(lhs.index, rhs.index);
        }
        virtual bool filter(const gui_model& model, const gui_node node) noexcept
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
        virtual error_type create_bind(unique_ptr<gui_bind>&& bind, gui_model& model, gui_window& window, const gui_node node, const gui_widget widget) noexcept = 0;
        virtual error_type create_style(const string_view name, const string_view css) noexcept = 0;
        virtual error_type create_model(shared_ptr<gui_model>& model) noexcept = 0;
        virtual error_type create_window(shared_ptr<gui_window>& window, shared_ptr<icy::window> handle) noexcept = 0;
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

    inline error_type copy(const gui_layout& src, gui_layout& dst) noexcept
    {
        ICY_ERROR(copy(src.attributes, dst.attributes));
        ICY_ERROR(copy(src.children, dst.children));
        ICY_ERROR(copy(src.text, dst.text));
        ICY_ERROR(copy(src.type, dst.type));
        dst.widget = src.widget;
        return error_type();
    }

    extern const error_source error_source_css;
}