#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_atomic.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/core/icy_input.hpp>

#define ICY_GUI_ERROR(X) if (const auto error = (X)) return make_stdlib_error(static_cast<std::errc>(error)); return error_type();

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

struct HWND__;

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
        node        =   0x06,
        input       =   0x07,
        sstring     =   0x08,
        lstring     =   0x09,
        array       =   0x0A,
        user        =   0x0B,
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
    enum class gui_shortcut : uint32_t
    {
        none,
        help_contents,
        whats_this,
        open,
        close,
        save,
        create, //  new
        remove, //  delete
        cut,
        copy,
        paste,
        undo,
        redo,
        back,
        forward,
        refresh,
        zoom_in,
        zoom_out,
        print,
        add_tab,
        next_child,
        previous_child,
        find,
        find_next,
        find_previous,
        replace,
        selectAll,
        bold,
        italic,
        underline,
        move_to_next_char,
        move_to_previous_char,
        move_to_next_word,
        move_to_previous_word,
        move_to_next_line,
        move_to_previous_line,
        move_to_next_page,
        move_to_previous_page,
        move_to_start_of_line,
        move_to_end_of_line,
        move_to_start_of_block,
        move_to_end_of_block,
        move_to_start_of_document,
        move_to_end_of_document,
        select_next_char,
        select_previous_char,
        select_next_word,
        select_previous_word,
        select_next_line,
        select_previous_line,
        select_next_page,
        select_previous_page,
        select_start_of_line,
        select_end_of_line,
        select_start_of_block,
        select_end_of_block,
        select_start_of_document,
        select_end_of_document,
        delete_start_of_word,
        delete_end_of_word,
        delete_end_of_line,
        insert_paragraph_separator,
        insert_line_separator,
        save_as,
        preferences,
        quit,
        full_screen,
        deselect,
        delete_complete_line,
        backspace,
        cancel
    };
    

    class gui_variant;
    class gui_node
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
        friend int compare(const gui_node& lhs, const gui_node& rhs) noexcept;
    public:
        rel_ops(gui_node);
        gui_node() noexcept = default;
        gui_node(const gui_node& rhs) noexcept : _ptr(rhs._ptr)
        {
            if (_ptr)
                _ptr->add_ref();
        }
        gui_node(gui_node&& rhs) noexcept : _ptr(rhs._ptr)
        {
            rhs._ptr = nullptr;
        }
        ~gui_node() noexcept
        {
            if (_ptr)
                _ptr->release();
        }
        ICY_DEFAULT_MOVE_ASSIGN(gui_node);
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
    inline int compare(const gui_node& lhs, const gui_node& rhs) noexcept
    {
        return icy::compare(lhs._ptr, rhs._ptr);
    }

    class gui_variant
    {
    public:
        using boolean_type = bool;
        using sinteger_type = int64_t;
        using uinteger_type = uint64_t;
        using floating_type = double;
        using guid_type = guid;
        using node_type = gui_node;
        using input_type = input_message;
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
            new (m_data + sizeof(gui_node) * 0) gui_node(value);
            new (m_data + sizeof(gui_node) * 1) gui_node;
        }
        gui_variant(const gui_node src, const gui_node dst) noexcept : m_type(uint8_t(gui_variant_type::node)), m_size(0)
        {
            new (m_data + sizeof(gui_node) * 0) gui_node(src);
            new (m_data + sizeof(gui_node) * 1) gui_node(dst);
        }
        gui_variant(const input_message value) noexcept : m_type(uint8_t(gui_variant_type::input)), m_size(0)
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
                for (auto&& node : m_node)
                    if (node._ptr)
                        node._ptr->add_ref();
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
                m_node[0].~gui_node();
                m_node[1].~gui_node();
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
                return string_view();
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
                return guid();
        }
        gui_node as_node(const size_t index = 0) const noexcept
        {
            if (type() == gui_variant_type::node && index < 2)
                return m_node[index];
            else
                return gui_node();            
        }
        input_message as_input() const noexcept
        {
            if (type() == gui_variant_type::input)
                return m_input;
            else
                return input_message();
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
            return const_cast<void*>(static_cast<const gui_variant*>(this)->as_user());
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
            return 0;
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
            node_type m_node[2];
            input_message m_input;
            buffer_type* m_buffer;
        };
    };

#if ICY_QTGUI_BUILD


#else
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
        return error_type();
    }
#endif
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


    struct gui_widget
    {
        friend int compare(const gui_widget& lhs, const gui_widget& rhs) noexcept;
        rel_ops(gui_widget);
        uint64_t index = 0;
    };
    struct gui_action
    {
        friend int compare(const gui_action& lhs, const gui_action& rhs) noexcept;
        rel_ops(gui_action);
        uint64_t index = 0;
    };
    struct gui_image
    {
        friend int compare(const gui_image& lhs, const gui_image& rhs) noexcept;
        rel_ops(gui_image);
        uint64_t index = 0;
    };
    inline int compare(const gui_widget& lhs, const gui_widget& rhs) noexcept
    {
        return icy::compare(lhs.index, rhs.index);
    }
    inline int compare(const gui_action& lhs, const gui_action& rhs) noexcept
    {
        return icy::compare(lhs.index, rhs.index);
    }
    inline int compare(const gui_image& lhs, const gui_image& rhs) noexcept
    {
        return icy::compare(lhs.index, rhs.index);
    }

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
    /*struct gui_widget_args_keys
    {
        static constexpr string_view layout = "Layout"_s;
        static constexpr string_view stretch = "Stretch"_s;
        static constexpr string_view row = "Row"_s;
        static constexpr string_view col = "Col"_s;
    };*/

    class gui_system
    {
    public:
        gui_system() noexcept = default;
        gui_system(const gui_system&) noexcept = delete;
        virtual void release() noexcept = 0;
        virtual uint32_t wake() noexcept = 0;
        virtual uint32_t loop(event_type& type, gui_event& args) noexcept = 0;
        virtual uint32_t create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept = 0;
        virtual uint32_t create(gui_widget& widget, HWND__* const win32, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept = 0;
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
        virtual uint32_t icon(const gui_action action, const gui_image icon) noexcept = 0;
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
        virtual uint32_t input(const gui_widget widget, const input_message& msg) noexcept = 0;
        virtual uint32_t keybind(const gui_action action, const input_message& input) noexcept = 0;
        virtual uint32_t keybind(const gui_action action, const gui_shortcut shortcut) noexcept = 0;
        virtual uint32_t resize_columns(const gui_widget widget) noexcept = 0;
        virtual uint32_t undo(const gui_widget widget) noexcept = 0;
        virtual uint32_t redo(const gui_widget widget) noexcept = 0;
        virtual uint32_t append(const gui_widget widget, const string_view text) noexcept = 0;
        virtual uint32_t readonly(const gui_widget widget, const bool value) noexcept = 0;
    protected:
        ~gui_system() noexcept = default;
    };
}

#ifndef ICY_QTGUI_BUILD
namespace icy
{  
    class gui_queue : public event_system
    {
        friend error_type create_event_system(shared_ptr<gui_queue>& queue) noexcept;
        enum { tag };
        error_type initialize() noexcept;
    public:
        gui_queue(decltype(tag)) noexcept
        {
            event_system::filter(event_type::global_quit);
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
        bool is_running() const noexcept
        {
            return m_running.load();
        }
        error_type signal(const event_data&) noexcept override
        {
            ICY_GUI_ERROR(m_system->wake());
        }
        error_type exec() noexcept override
        {
            m_running = true;
            ICY_SCOPE_EXIT{ m_running = false; };
            while (true)
            {
                while (auto event = pop())
                {
                    if (event->type == event_type::global_quit)
                        return error_type();
                }

                auto type = event_type::none;
                auto args = gui_event();
                if (const auto error = m_system->loop(type, args))
                    return make_stdlib_error(static_cast<std::errc>(error));

                if (type & event_type::gui_any)
                {
                    ICY_ERROR(event::post(this, type, args));
                }
            }
        }
        error_type create(gui_widget& widget, const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept
        {
            ICY_GUI_ERROR(m_system->create(widget, type, parent, flags));
        }
        error_type create(gui_widget& widget, HWND__* const win32, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::none) noexcept
        {
            ICY_GUI_ERROR(m_system->create(widget, win32, parent, flags));
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
        error_type icon(const gui_action action, const gui_image image) noexcept
        {
            ICY_GUI_ERROR(m_system->icon(action, image));
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

            shared_ptr<event_queue> loop;
            ICY_ERROR(create_event_system(loop, event_type::gui_action));

            auto timeout = max_timeout;
            while (true)
            {
                event event;
                if (const auto error = loop->pop(event, timeout))
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
            return error_type();
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
        error_type input(const gui_widget widget, const input_message& msg) noexcept
        {
            ICY_GUI_ERROR(m_system->input(widget, msg));
        }
        error_type keybind(const gui_action action, const input_message& input) noexcept
        {
            ICY_GUI_ERROR(m_system->keybind(action, input));
        }
        error_type keybind(const gui_action action, const gui_shortcut shortcut) noexcept
        {
            ICY_GUI_ERROR(m_system->keybind(action, shortcut));
        }
        error_type resize_columns(const gui_widget widget) noexcept
        {
            ICY_GUI_ERROR(m_system->resize_columns(widget));
        }
        error_type undo(const gui_widget widget) noexcept
        {
            ICY_GUI_ERROR(m_system->undo(widget));
        }
        error_type redo(const gui_widget widget) noexcept
        {
            ICY_GUI_ERROR(m_system->redo(widget));
        }
        error_type append(const gui_widget widget, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->append(widget, text));
        }
        error_type readonly(const gui_widget widget, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->readonly(widget, value));
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
        std::atomic<bool> m_running = false;
    };
    static error_type create_event_system(shared_ptr<gui_queue>& queue) noexcept
    {
        ICY_ERROR(make_shared(queue, gui_queue::tag));
        if (const auto error = queue->initialize())
        {
            queue = nullptr;
            return error;
        }
        return error_type();
    }
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