#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_atomic.hpp>
#include <icy_engine/core/icy_memory.hpp>

#define ICY_GUI_ERROR(X) if (const auto error = (X)) return make_stdlib_error(static_cast<std::errc>(error)); return {};

#if ICY_QTGUI_STATIC
#define ICY_QTGUI_API
#else
#ifdef ICY_QTGUI_EXPORTS
#define ICY_QTGUI_API __declspec(dllexport)
#else
#define ICY_QTGUI_API __declspec(dllimport)
#endif
#endif

#define ICY_GUI_VERSION 0x0005'0002

#pragma warning(push)
#pragma warning(disable:4201)

namespace icy
{
    enum class gui_variant_type : uint8_t
    {
        none        =   0x00,
        boolean     =   0x01,
        sinteger    =   0x02,
        uinteger    =   0x03,
        floating    =   0x04,
        sstring     =   0x05,
        lstring     =   0x06,
        array       =   0x07,
        user        =   0x08,
    };
    
    class gui_variant
    {
    public:
        using boolean_type = bool;
        using sinteger_type = int64_t;
        using uinteger_type = uint64_t;
        using floating_type = double;
        struct buffer_type
        {
            buffer_type(icy::realloc_func alloc, void* user, size_t size) noexcept : alloc(alloc), user(user), size(size), ref(1)
            {

            }
            const realloc_func alloc;
            void* const user;
            const size_t size;
            std::atomic<uint32_t> ref;
        };
    public:
        static constexpr size_t max_length = 15;
        gui_variant() noexcept : m_data{}
        {

        }
        gui_variant(const boolean_type value) noexcept : m_type(static_cast<char>(gui_variant_type::boolean))
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const sinteger_type value) noexcept : m_type(static_cast<char>(gui_variant_type::sinteger))
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const uinteger_type value) noexcept : m_type(static_cast<char>(gui_variant_type::uinteger))
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const floating_type value) noexcept : m_type(static_cast<char>(gui_variant_type::floating))
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const string_view str) noexcept : m_data(), m_size(0), m_type(static_cast<char>(gui_variant_type::sstring))
        {
            const char* const beg = str.bytes().data();
            auto ptr = beg;
            auto it = str.begin();
            while (true)
            {
                ptr = static_cast<const char*>(it);
                if (it == str.end())
                    break;
                if (ptr - beg >= sizeof(m_data))
                    break;
                ++it;
            }
            memcpy(m_data, beg, m_size = ptr - beg);
        }
        gui_variant(const gui_variant& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            switch (type())
            {
            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                m_buffer->ref.fetch_add(1, std::memory_order_release);
                break;
            }
        }
        gui_variant(gui_variant&& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            memset(&rhs, 0, sizeof(rhs));
        }
        ICY_DEFAULT_COPY_ASSIGN(gui_variant);
        ICY_DEFAULT_MOVE_ASSIGN(gui_variant);
        ~gui_variant() noexcept
        {
            switch (type())
            {
            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                if (m_buffer->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    m_buffer->alloc(m_buffer, 0, m_buffer->user);
                break;
            }
        }
        gui_variant_type type() const noexcept
        {
            return static_cast<gui_variant_type>(m_type);
        }
        string_view as_string() const noexcept
        {
            if (type() == gui_variant_type::sstring)
                return string_view(reinterpret_cast<const char*>(m_data), m_size);
            else if (type() == gui_variant_type::lstring)
                return string_view(reinterpret_cast<const char*>(m_buffer + 1), m_buffer->size);
            else
                return {};
        }
        boolean_type as_boolean() const noexcept
        {
            switch (type())
            {
            case gui_variant_type::none:
                return false;

            case gui_variant_type::boolean:
                return m_boolean;

            case gui_variant_type::sinteger:
                return m_sinteger != 0;

            case gui_variant_type::uinteger:
                return m_uinteger != 0;

            case gui_variant_type::floating:
                return m_floating != 0;

            case gui_variant_type::sstring:
                return *m_data != 0;
            }
            return true;
        }
        sinteger_type as_sinteger() const noexcept
        {
            switch (type())
            {
            case gui_variant_type::boolean:
                return m_boolean;

            case gui_variant_type::sinteger:
                return m_sinteger;

            case gui_variant_type::uinteger:
                return static_cast<sinteger_type>(m_uinteger);

            case gui_variant_type::floating:
                return static_cast<sinteger_type>(llround(m_floating));

            case gui_variant_type::sstring:
            case gui_variant_type::lstring:
            {
                auto str = as_string();
                sinteger_type value = 0;
                _snscanf_s(str.bytes().data(), str.bytes().size(), "%lli", &value);
                return value;
            }
            }
            return 0;
        }
        sinteger_type as_uinteger() const noexcept
        {
            return static_cast<uinteger_type>(as_sinteger());
        }
        floating_type as_floating() const noexcept
        {
            switch (type())
            {
            case gui_variant_type::boolean:
                return m_boolean ? 1.0 : 0.0;

            case gui_variant_type::sinteger:
                return static_cast<floating_type>(m_sinteger);

            case gui_variant_type::uinteger:
                return static_cast<floating_type>(m_uinteger);

            case gui_variant_type::floating:
                return m_floating;

            case gui_variant_type::sstring:
            case gui_variant_type::lstring:
            {
                auto str = as_string();
                floating_type value = 0;
                _snscanf_s(str.bytes().data(), str.bytes().size(), "%lf", &value);
                return value;
            }
            }
            return 0;
        }
        template<typename T> const T* as_user() const noexcept
        {
            if (type() == gui_variant_type::user)
            {
                if (m_buffer->size == sizeof(T))
                    return reinterpret_cast<T*>(m_buffer + 1);
            }
            return nullptr;
        }
        template<typename T> T* as_user() noexcept
        {
            return const_cast<void*>(static_cast<const gui_variant*>(this)->user());
        }
        const void* data() const noexcept
        {
            switch (type())
            {
            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                return m_buffer + 1;
            }
            return nullptr;
        }
        void* data() noexcept
        {
            return const_cast<void*>(static_cast<const gui_variant*>(this)->data());
        }
        size_t size() const noexcept
        {
            switch (type())
            {
            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                return m_buffer->size;
            }
            return 0;
        }
        uint32_t initialize(const realloc_func alloc, void* const user, const char* const data, const size_t size, const gui_variant_type type) noexcept
        {
            switch (type)
            {
            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                break;
            default:
                return static_cast<uint32_t>(std::errc::invalid_argument);
            };

            if (!alloc)
                return static_cast<uint32_t>(std::errc::invalid_argument);

            union
            {
                void* memory;
                char* bytes;
            };
            memory = alloc(nullptr, sizeof(buffer_type) + size, user);
            if (!memory)
                return static_cast<uint32_t>(std::errc::not_enough_memory);

            this->~gui_variant();
            m_buffer = new (memory) buffer_type(alloc, user, size);
            memcpy(bytes + sizeof(buffer_type), data, size);
            m_type = static_cast<char>(type);
            m_size = 0;
            return {};
        }
    private:
        union
        {
            struct
            {
                uint8_t m_data[max_length];
                uint8_t m_type : 0x04;
                uint8_t m_size : 0x04;
            };
            boolean_type m_boolean;
            sinteger_type m_sinteger;
            uinteger_type m_uinteger;
            floating_type m_floating;
            buffer_type* m_buffer;
        };
    };
    inline error_type make_variant(gui_variant& var, const realloc_func alloc, void* const user, const char* const data, const size_t size) noexcept
    {
        ICY_GUI_ERROR(var.initialize(alloc, user, data, size, gui_variant_type::array));
    }
    inline error_type make_variant(gui_variant& var, const realloc_func alloc, void* const user, const string_view str) noexcept
    {
        ICY_GUI_ERROR(var.initialize(alloc, user, str.bytes().data(), str.bytes().size(), gui_variant_type::lstring));
    }
    template<typename T, typename = std::enable_if_t<std::is_trivially_destructible<T>::value>>
    inline uint32_t make_variant(gui_variant& var, const realloc_func alloc, void* const user, const T& data) noexcept
    {
        static_assert(std::is_trivially_destructible<T>::value, "INVALID STRUCT VARIANT TYPE");
        ICY_GUI_ERROR(var.initialize(alloc, user, &data, sizeof(data), gui_variant_type::user));
    }
    struct gui_widget : detail::rel_ops<gui_widget>
    {
        int compare(const gui_widget rhs) const noexcept
        {
            return icy::compare(index, rhs.index);
        }
        uint64_t index = 0;
    };
    struct gui_action : detail::rel_ops<gui_action>
    {
        int compare(const gui_action rhs) const noexcept
        {
            return icy::compare(index, rhs.index);
        }
        uint64_t index = 0;
    };
    struct gui_event
    {
        gui_widget widget;
        gui_variant data;
    };
}

#pragma warning(pop) 