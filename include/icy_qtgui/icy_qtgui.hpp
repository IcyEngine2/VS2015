#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_atomic.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>

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

#define ICY_GUI_VERSION 0x0006'0002

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
        guid        =   0x05,
        sstring     =   0x06,
        lstring     =   0x07,
        array       =   0x08,
        node        =   0x09,
        user        =   0x0A,
    };
    enum class gui_check_state : uint32_t
    {
        unchecked,
        partial,
        checked,
    };
    enum class gui_widget_flag : uint32_t
    {
        none,

        layout_hbox = 0x01,
        layout_vbox = 0x02,
        layout_grid = 0x03,
        auto_insert = 0x04,
    };
    enum class gui_widget_type : uint32_t
    {
        none,

        window,
        dialog,
        vsplitter,
        hsplitter,
        tabs,
        frame,

        //  -- text --

        label,
        line_edit,
        text_edit,

        //  -- menu --

        menu,
        menubar,

        //  -- model view --

        combo_box,
        list_view,
        tree_view,
        grid_view,

        //  -- utility --

        message,
        progress,

        //  --  buttons --

        text_button,
        tool_button,

        //  --  dialogs --

        dialog_open_file,
        dialog_save_file,
        dialog_select_directory,
        dialog_select_color,
        dialog_select_font,
        dialog_input_line,
        dialog_input_text,
        dialog_input_integer,
        dialog_input_double,
    };
    
    class gui_variant;
    class gui_node : public detail::rel_ops<gui_node>
    {
    public:
        struct data_type
        {
            virtual void add_ref() noexcept = 0;
            virtual void release() noexcept = 0;
            virtual gui_node parent() const noexcept = 0;
            virtual int32_t row() const noexcept = 0;
            virtual int32_t col() const noexcept = 0;
            virtual uint64_t model() const noexcept = 0;
            virtual gui_variant udata() const noexcept = 0;
        };
    public:
        gui_node() noexcept = default;
        gui_node(const gui_node& rhs) noexcept : _ptr(rhs._ptr)
        {
            if (_ptr)
                _ptr->add_ref();
        }
        ~gui_node() noexcept
        {
            if (_ptr)
                _ptr->release();
        }
        ICY_DEFAULT_COPY_ASSIGN(gui_node);
        explicit operator bool() const noexcept
        {
            return !!_ptr;
        }
        gui_node parent() const noexcept
        {
            if (_ptr)
                return _ptr->parent();
            return gui_node();
        }
        size_t row() const noexcept
        {
            if (_ptr)
                return size_t(_ptr->row());
            else
                return SIZE_MAX;
        }
        size_t col() const noexcept
        {
            if (_ptr)
                return size_t(_ptr->col());
            else
                return SIZE_MAX;
        }
        int compare(const gui_node& rhs) const noexcept
        {
            return icy::compare(_ptr, rhs._ptr);
        }
        uint64_t model() const noexcept
        {
            if (_ptr)
                return _ptr->model();
            return 0;
        }
        gui_variant udata() const noexcept;
    public:
        data_type* _ptr = nullptr;
    };

    class gui_variant
    {
    public:
        using boolean_type = bool;
        using sinteger_type = int64_t;
        using uinteger_type = uint64_t;
        using floating_type = double;
        using guid_type = guid;
        using node_type = gui_node;
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
        static constexpr size_t max_length = 22;
        gui_variant() noexcept : m_data{}, m_type(uint8_t(gui_variant_type::none)), m_size(0)
        {

        }
        gui_variant(const boolean_type value) noexcept : m_type(uint8_t(gui_variant_type::boolean)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const sinteger_type value) noexcept : m_type(uint8_t(gui_variant_type::sinteger)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const uinteger_type value) noexcept : m_type(uint8_t(gui_variant_type::uinteger)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const floating_type value) noexcept : m_type(uint8_t(gui_variant_type::floating)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const guid_type value) noexcept : m_type(uint8_t(gui_variant_type::guid)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const gui_node value) noexcept : m_type(uint8_t(gui_variant_type::node)), m_size(0)
        {
            new (m_data) decltype(value)(value);
        }
        gui_variant(const string_view str) noexcept : m_data{}, m_type(uint8_t(gui_variant_type::sstring)), m_size(0)
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
            memcpy(m_data, beg, m_size = static_cast<uint8_t>(ptr - beg));
        }
        gui_variant(const gui_variant& rhs) noexcept
        {
            memcpy(this, &rhs, sizeof(rhs));
            switch (type())
            {
            case gui_variant_type::node:
                if (m_node)
                    m_node._ptr->add_ref();
                break;

            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                if (m_buffer)
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
            case gui_variant_type::node:
                m_node.~gui_node();
                break;

            case gui_variant_type::lstring:
            case gui_variant_type::array:
            case gui_variant_type::user:
                if (m_buffer && m_buffer->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
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
            else if (type() == gui_variant_type::lstring && m_buffer)
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
                return m_data && *m_data != 0;
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
        uinteger_type as_uinteger() const noexcept
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
        guid as_guid() const noexcept
        {
            if (type() == gui_variant_type::guid)
                return m_guid;
            else
                return {};
        }
        gui_node as_node() const noexcept
        {
            if (type() == gui_variant_type::node)
                return m_node;
            else
                return {};            
        }
        template<typename T> const T* as_user() const noexcept
        {
            if (type() == gui_variant_type::user && m_buffer)
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
                return m_buffer ? m_buffer + 1 : nullptr;
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
                return m_buffer ? m_buffer->size : 0;
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
                uint8_t m_type;
                uint8_t m_size;
            };
            boolean_type m_boolean;
            sinteger_type m_sinteger;
            uinteger_type m_uinteger;
            floating_type m_floating;
            guid_type m_guid;
            node_type m_node;
            buffer_type* m_buffer;
        };
    };
    inline error_type make_variant(gui_variant& var, const realloc_func alloc, void* const user, const char* const data, const size_t size) noexcept
    {
        ICY_GUI_ERROR(var.initialize(alloc, user, data, size, gui_variant_type::array));
    }
    inline error_type make_variant(gui_variant& var, const realloc_func alloc, void* const user, const string_view str) noexcept
    {
        ICY_GUI_ERROR(var.initialize(alloc, user, 
            str.bytes().data(), str.bytes().size(), gui_variant_type::lstring));
    }
    inline error_type make_variant(gui_variant& var, const string_view str) noexcept
    {
        if (str.bytes().size() > gui_variant::max_length)
        {
            ICY_GUI_ERROR(var.initialize(detail::global_heap.realloc, detail::global_heap.user,
                str.bytes().data(), str.bytes().size(), gui_variant_type::lstring));
        }
        else
        {
            var = str;
        }
        return {};
    }
    template<typename T, typename = std::enable_if_t<std::is_trivially_destructible<T>::value>>
    inline uint32_t make_variant(gui_variant& var, const realloc_func alloc, void* const user, const T& data) noexcept
    {
        static_assert(std::is_trivially_destructible<T>::value, "INVALID STRUCT VARIANT TYPE");
        ICY_GUI_ERROR(var.initialize(alloc, user, &data, sizeof(data), gui_variant_type::user));
    }
    
    inline gui_variant gui_node::udata() const noexcept
    {
        if (_ptr)
            return _ptr->udata();
        return gui_variant();
    }

    inline gui_widget_flag operator|(const gui_widget_flag lhs, const gui_widget_flag rhs) noexcept
    {
        return gui_widget_flag(uint32_t(lhs) | uint32_t(rhs));
    }
    inline bool operator&(const gui_widget_flag lhs, const gui_widget_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
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
    struct gui_image : detail::rel_ops<gui_image>
    {
        int compare(const gui_image rhs) const noexcept
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
    struct gui_insert
    {
        gui_insert() noexcept = default;
        gui_insert(const uint32_t x, const uint32_t y) noexcept : x(x), y(y)
        {

        }
        gui_insert(const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept :
            x(x), y(y), dx(dx), dy(dy)
        {

        }
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t dx = 1;
        uint32_t dy = 1;
    };
    struct gui_widget_args_keys
    {
        static constexpr string_view layout = "Layout"_s;
        static constexpr string_view stretch = "Stretch"_s;
        static constexpr string_view row = "Row"_s;
        static constexpr string_view col = "Col"_s;
    };

    class gui_system
    {
    public:
        gui_system() noexcept = default;
        gui_system(const gui_system&) noexcept = delete;
        virtual void release() noexcept = 0;
        virtual uint32_t wake() noexcept = 0;
        virtual uint32_t loop(event_type& type, gui_event& args) noexcept = 0;
        virtual uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept = 0;
        virtual uint32_t create(gui_action& action, const string_view text) noexcept = 0;
        virtual uint32_t create(gui_node& model_root) noexcept = 0;
        virtual uint32_t create(gui_image& icon, const const_matrix_view<color> colors) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const gui_insert args) noexcept = 0;
        virtual uint32_t insert(const gui_widget widget, const gui_action action) noexcept = 0;
        virtual uint32_t insert_rows(const gui_node parent, const size_t offset, const size_t count) noexcept = 0;
        virtual uint32_t insert_cols(const gui_node parent, const size_t offset, const size_t count) noexcept = 0;
        virtual uint32_t remove_rows(const gui_node parent, const size_t offset, const size_t count) noexcept = 0;
        virtual uint32_t remove_cols(const gui_node parent, const size_t offset, const size_t count) noexcept = 0;
        virtual uint32_t move_rows(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept = 0;
        virtual uint32_t move_cols(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept = 0;
        virtual gui_node node(const gui_node parent, const size_t row, const size_t col) noexcept = 0;
        virtual uint32_t show(const gui_widget widget, const bool value) noexcept = 0;
        virtual uint32_t text(const gui_widget widget, const string_view text) noexcept = 0;
        virtual uint32_t text(const gui_widget tabs, const gui_widget widget, const string_view text) noexcept = 0;
        virtual uint32_t text(const gui_node node, const string_view text) noexcept = 0;
        virtual uint32_t icon(const gui_node node, const gui_image icon) noexcept = 0;
        virtual uint32_t icon(const gui_widget widget, const gui_image icon) noexcept = 0;
        virtual uint32_t udata(const gui_node node, const gui_variant& var) noexcept = 0;
        virtual uint32_t bind(const gui_action action, const gui_widget menu) noexcept = 0;
        virtual uint32_t bind(const gui_widget widget, const gui_node node) noexcept = 0;
        virtual uint32_t vheader(const gui_node root, const uint32_t index, const string_view text) noexcept = 0;
        virtual uint32_t hheader(const gui_node root, const uint32_t index, const string_view text) noexcept = 0;
        virtual uint32_t enable(const gui_action action, const bool value) noexcept = 0;
        virtual uint32_t enable(const gui_widget widget, const bool value) noexcept = 0;
        virtual uint32_t modify(const gui_widget widget, const string_view args) noexcept = 0;
        virtual uint32_t destroy(const gui_widget widget) noexcept = 0;
        virtual uint32_t destroy(const gui_action action) noexcept = 0;
        virtual uint32_t destroy(const gui_node root) noexcept = 0;
        virtual uint32_t destroy(const gui_image image) noexcept = 0;
        virtual uint32_t clear(const gui_node root) noexcept = 0;
        virtual uint32_t scroll(const gui_widget widget, const gui_node node) noexcept = 0;
        virtual uint32_t scroll(const gui_widget tabs, const gui_widget widget) noexcept = 0;
    protected:
        ~gui_system() noexcept = default;
    };


}

#ifndef ICY_QTGUI_BUILD
namespace icy
{
    struct gui_widget_args
    {
        error_type insert(const string_view key, gui_widget_args*& val) noexcept
        {
            string str;
            auto it = map.end();
            ICY_ERROR(to_string(key, str));
            ICY_ERROR(map.insert(std::move(str), gui_widget_args(), &it));
            val = &it->value;
            return {};
        }
        error_type insert(const string_view key, const string_view value) noexcept
        {
            string str;
            gui_widget_args val;
            ICY_ERROR(to_string(key, str));
            ICY_ERROR(to_string(value, val.value));
            ICY_ERROR(map.insert(std::move(str), std::move(val)));
            return {};
        }
        string value;
        map<string, gui_widget_args> map;
    };
    inline error_type to_string(const gui_widget_args& args, string& str) noexcept
    {
        if (args.map.empty())
        {
            ICY_ERROR(str.appendf("\"%1\"", string_view(args.value)));
        }
        else
        {
            ICY_ERROR(str.append("{"_s));
            auto first = true;
            for (auto&& pair : args.map)
            {
                string val;
                ICY_ERROR(to_string(pair.value, val));
                ICY_ERROR(str.appendf("%1\"%2\": %3", first ? ""_s : ","_s, string_view(pair.key), string_view(val)));
                first = false;
            }
            ICY_ERROR(str.append("}"_s));
        }
        return {};
    }

    class gui_queue;
    //inline error_type create_gui(shared_ptr<gui_queue>& queue) noexcept;

    class gui_queue : public event_queue
    {
        friend error_type create_gui(shared_ptr<gui_queue>& queue) noexcept;
        enum { tag };
        error_type initialize() noexcept;
    public:
        gui_queue(decltype(tag)) noexcept
        {
            event_queue::filter(event_type::global_quit);
        }
        ~gui_queue() noexcept override
        {
            filter(0);
            if (m_system)
                m_system->release();
#if ICY_QTGUI_STATIC
            ;
#else
            m_library.shutdown();
#endif
        }
        error_type signal(const event_data&) noexcept override
        {
            ICY_GUI_ERROR(m_system->wake());
        }
        error_type loop() noexcept
        {
            while (true)
            {
                while (auto event = pop())
                {
                    if (event->type == event_type::global_quit)
                        return {};
                }

                auto type = event_type::none;
                auto args = gui_event();
                if (const auto error = m_system->loop(type, args))
                    return make_stdlib_error(static_cast<std::errc>(error));

                if (type == event_type::window_close || (type & event_type::gui_any))
                {
                    ICY_ERROR(event::post(this, type, args));
                }
            }
        }
        error_type create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept
        {
            ICY_GUI_ERROR(m_system->create(widget, type, parent, flags));
        }
        error_type create(gui_action& action, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->create(action, text));
        }
        error_type create(gui_node& root) noexcept
        {
            ICY_GUI_ERROR(m_system->create(root));
        }
        error_type create(gui_image& image, const const_matrix_view<color> colors) noexcept
        {
            ICY_GUI_ERROR(m_system->create(image, colors));
        }
        error_type insert(const gui_widget widget, const gui_insert args = {}) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, args));
        }
        error_type insert(const gui_widget widget, const gui_action action) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, action));
        }
        error_type insert_rows(const gui_node parent, const size_t offset, const size_t count) noexcept
        {
            ICY_GUI_ERROR(m_system->insert_rows(parent, offset, count));
        }
        error_type insert_cols(const gui_node parent, const size_t offset, const size_t count) noexcept
        {
            ICY_GUI_ERROR(m_system->insert_cols(parent, offset, count));
        }
        error_type remove_rows(const gui_node parent, const size_t offset, const size_t count) noexcept
        {
            ICY_GUI_ERROR(m_system->remove_rows(parent, offset, count));
        }
        error_type remove_cols(const gui_node parent, const size_t offset, const size_t count) noexcept
        {
            ICY_GUI_ERROR(m_system->remove_cols(parent, offset, count));
        }
        error_type move_rows(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept
        {
            ICY_GUI_ERROR(m_system->move_rows(node_src, offset_src, count, node_dst, offset_dst));
        }
        error_type move_cols(const gui_node node_src, const size_t offset_src, const size_t count, const gui_node node_dst, const size_t offset_dst) noexcept
        {
            ICY_GUI_ERROR(m_system->move_cols(node_src, offset_src, count, node_dst, offset_dst));
        }
        gui_node node(const gui_node parent, const size_t row, const size_t col) noexcept
        {
            return m_system->node(parent, row, col);
        }
        error_type show(const gui_widget widget, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->show(widget, value));
        }
        error_type text(const gui_widget widget, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->text(widget, text));
        }
        error_type text(const gui_widget tabs, const gui_widget widget, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->text(tabs, widget, text));
        }
        error_type text(const gui_node node, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->text(node, text));
        }
        error_type udata(const gui_node node, const gui_variant& var) noexcept
        {
            ICY_GUI_ERROR(m_system->udata(node, var));
        }
        error_type icon(const gui_node node, const gui_image image) noexcept
        {
            ICY_GUI_ERROR(m_system->icon(node, image));
        }
        error_type icon(const gui_widget widget, const gui_image image) noexcept
        {
            ICY_GUI_ERROR(m_system->icon(widget, image));
        }
        error_type enable(const gui_action action, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->enable(action, value));
        }
        error_type enable(const gui_widget widget, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->enable(widget, value));
        }
        error_type bind(const gui_action action, const gui_widget menu) noexcept
        {
            ICY_GUI_ERROR(m_system->bind(action, menu));
        }
        error_type bind(const gui_widget widget, const gui_node node) noexcept
        {
            ICY_GUI_ERROR(m_system->bind(widget, node));
        }
        error_type vheader(const gui_node root, const uint32_t index, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->vheader(root, index, text));
        }
        error_type hheader(const gui_node root, const uint32_t index, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->hheader(root, index, text));
        }
        error_type modify(const gui_widget widget, const string_view args) noexcept
        {
            ICY_GUI_ERROR(m_system->modify(widget, args));
        }
        error_type exec(const gui_widget widget, gui_action& action) noexcept
        {
            ICY_ERROR(show(widget, true));

            shared_ptr<event_loop> loop;
            ICY_ERROR(event_loop::create(loop, event_type::gui_action));

            auto timeout = max_timeout;
            while (true)
            {
                event event;
                if (const auto error = loop->loop(event, timeout))
                {
                    if (error == make_stdlib_error(std::errc::timed_out))
                        break;
                    return error;
                }

                if (event->type == event_type::global_quit)
                    break;

                if (event->type == event_type::gui_action)
                {
                    const auto& event_data = event->data<gui_event>();
                    if (const auto index = event_data.data.as_uinteger())
                    {
                        action.index = index;
                        break;
                    }
                    else if (event_data.widget == widget)
                    {
                        timeout = {};
                    }
                }
            }
            return {};
        }
        error_type destroy(const gui_widget widget) noexcept
        {
            ICY_GUI_ERROR(m_system->destroy(widget));
        }
        error_type destroy(const gui_action action) noexcept
        {
            ICY_GUI_ERROR(m_system->destroy(action));
        }
        error_type destroy(const gui_node root) noexcept
        {
            ICY_GUI_ERROR(m_system->destroy(root));
        }
        error_type destroy(const gui_image image) noexcept
        {
            ICY_GUI_ERROR(m_system->destroy(image));
        }
        error_type clear(const gui_node root) noexcept
        {
            ICY_GUI_ERROR(m_system->clear(root));
        }
        error_type scroll(const gui_widget widget, const gui_node node) noexcept
        {
            ICY_GUI_ERROR(m_system->scroll(widget, node));
        }
        error_type scroll(const gui_widget tabs, const gui_widget widget) noexcept
        {
            ICY_GUI_ERROR(m_system->scroll(tabs, widget));
        }
    private:
#if ICY_QTGUI_STATIC

#else
#if _DEBUG
        library m_library = "icy_qtguid"_lib;
#else
        library m_library = "icy_qtgui"_lib;
#endif
#endif
        gui_system* m_system = nullptr;
    };
    static error_type create_gui(shared_ptr<gui_queue>& queue) noexcept
    {
        ICY_ERROR(make_shared(queue, gui_queue::tag));
        if (const auto error = queue->initialize())
        {
            queue = nullptr;
            return error;
        }
        return {};
    }

    struct gui_widget_scoped : gui_widget
    {
        gui_widget_scoped() noexcept = default;
        gui_widget_scoped(gui_queue& gui) noexcept : gui(&gui)
        {

        }
        gui_widget_scoped(const gui_widget_scoped&) = delete;
        ~gui_widget_scoped() noexcept
        {
            if (index && gui)
                gui->destroy(*this);
        }
        error_type initialize(
            const gui_widget parent = {}, 
            const gui_widget_type type = gui_widget_type::menu,
            const gui_widget_flag flags =gui_widget_flag::auto_insert) noexcept
        {
            return gui ? gui->create(*this, type, parent, flags) : make_stdlib_error(std::errc::invalid_argument);
        }
        gui_queue* gui = nullptr;
    };
    struct gui_action_scoped : gui_action
    {
        gui_action_scoped() noexcept = default;
        gui_action_scoped(gui_queue& gui) noexcept : gui(&gui)
        {

        }
        gui_action_scoped(const gui_action_scoped&) = delete;
        gui_action_scoped(gui_action_scoped&& rhs) noexcept
        {
            std::swap(index, rhs.index);
            std::swap(gui, rhs.gui);
        }
        ICY_DEFAULT_MOVE_ASSIGN(gui_action_scoped);
        ~gui_action_scoped() noexcept
        {
            if (index && gui)
                gui->destroy(*this);
        }
        error_type initialize(const string_view text) noexcept
        {
            return gui ? gui->create(*this, text) : make_stdlib_error(std::errc::invalid_argument);
        }
        gui_queue* gui = nullptr;
    };
    struct gui_model_scoped : gui_node
    {
        gui_model_scoped() noexcept = default;
        gui_model_scoped(gui_queue& gui) noexcept : gui(&gui)
        {

        }
        gui_model_scoped(const gui_model_scoped&) = delete;
        ~gui_model_scoped() noexcept
        {
            if (*this && gui)
                gui->destroy(*this);
        }
        error_type initialize() noexcept
        {
            return gui ? gui->create(*this) : make_stdlib_error(std::errc::invalid_argument);
        }
        gui_queue* gui = nullptr;
    };
}
#endif

extern "C" uint32_t ICY_QTGUI_API icy_gui_system_create(const int version, icy::gui_system*& system) noexcept;

#ifndef ICY_QTGUI_BUILD
inline icy::error_type icy::gui_queue::initialize() noexcept
{
    if (m_system)
        m_system->release();
    m_system = nullptr;

#if ICY_QTGUI_STATIC
    ICY_GUI_ERROR(icy_gui_system_create(ICY_GUI_VERSION, m_system));
#else
    ICY_ERROR(m_library.initialize());
    if (const auto func = ICY_FIND_FUNC(m_library, icy_gui_system_create))
    {
        const auto err = func(ICY_GUI_VERSION, m_system);
        ICY_GUI_ERROR(err);
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);
#endif
    return{};
}
#endif

#pragma warning(pop) 