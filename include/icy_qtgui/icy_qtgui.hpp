#pragma once

#include "icy_qtgui_system.hpp"
#include "icy_qtgui_model.hpp"

namespace icy
{
    class gui_queue;
    class gui_model
    {
        friend gui_queue;
    public: 
        virtual int compare(const gui_node lhs, const gui_node rhs) const noexcept
        {
            return icy::compare(lhs.idx, rhs.idx);
        }
        virtual bool filter(const gui_node) const noexcept
        {
            return true;
        }
        virtual uint32_t data(const gui_node node, const gui_role role, gui_variant& value) const noexcept = 0;
        error_type init(gui_node& node) noexcept
        {
            if (!m_vtbl.init)
                return make_stdlib_error(std::errc::function_not_supported);
            ICY_GUI_ERROR(m_vtbl.init(m_view, node));
        }
        error_type sort(const gui_node node) noexcept
        {
            if (!m_vtbl.sort)
                return make_stdlib_error(std::errc::function_not_supported);
            ICY_GUI_ERROR(m_vtbl.sort(m_view, node));
        }
        error_type bind(const gui_node node, const gui_widget& widget) noexcept
        {
            if (!m_vtbl.bind)
                return make_stdlib_error(std::errc::function_not_supported);
            ICY_GUI_ERROR(m_vtbl.bind(m_view, node, widget));
        }
        error_type update(const gui_node node) noexcept
        {
            if (!m_vtbl.update)
                return make_stdlib_error(std::errc::function_not_supported);
            ICY_GUI_ERROR(m_vtbl.update(m_view, node));
        }
    private:
        class base_type : public gui_model_base
        {
        public:
            base_type() noexcept = default;
            base_type(const base_type&) = delete;
            gui_model* self() noexcept
            {
                return reinterpret_cast<gui_model*>(reinterpret_cast<char*>(this) - offsetof(gui_model, m_base));
            }
            const gui_model* self() const noexcept
            {
                return const_cast<base_type*>(this)->self();
            }
            void assign(const vtbl_type& vtbl) noexcept override
            {
                self()->m_vtbl = vtbl;
            }
            void add_ref() noexcept override
            {
                char buffer[sizeof(shared_ptr<gui_model>)];
                new (buffer) shared_ptr<gui_model>(make_shared_from_this(self()));
            }
            void release() noexcept
            {
                make_shared_from_this(self()).~shared_ptr();
            }
            int compare(const gui_node lhs, const gui_node rhs) const noexcept override
            {
                return self()->compare(lhs, rhs);
            }
            bool filter(const gui_node node) const noexcept override
            {
                return self()->filter(node);
            }
            uint32_t data(const gui_node node, const gui_role role, gui_variant& value) const noexcept override
            {
                return self()->data(node, role, value);
            }
        };
        gui_model_base::vtbl_type m_vtbl;
        gui_model_view m_view;
        base_type m_base;
    };
    
    class gui_queue : public event_queue
    {
        enum { tag };
        error_type initialize() noexcept;
    public:
        gui_queue(decltype(tag)) noexcept
        {
            event_queue::filter(event_type::global_quit);
        }
        gui_queue(decltype(tag), gui_system& gui) noexcept : m_system(&gui)
        {
            event_queue::filter(event_type::global_quit);
        }
        ~gui_queue() noexcept
        {
            if (m_system)
                m_system->release();
#if ICY_QTGUI_STATIC
            ;
#else
            m_library.shutdown();
#endif
        }
        static error_type create(shared_ptr<gui_queue>& queue, gui_system& gui) noexcept
        {
            return make_shared(queue, tag, gui);
        }
        static error_type create(shared_ptr<gui_queue>& queue) noexcept
        {
            ICY_ERROR(make_shared(queue, tag));
            if (const auto error = queue->initialize())
            {
                queue = nullptr;
                return error;
            }
            return {};
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

                if (type == event_type::window_close 
                    || type == event_type::gui_action 
                    || type == event_type::gui_update)
                {
                    ICY_ERROR(event::post(this, type, args));
                }
            }
        }
        error_type create(gui_widget& widget, const gui_widget_type type, const gui_layout layout = gui_layout::none, const gui_widget parent = {}) noexcept
        {
            ICY_GUI_ERROR(m_system->create(widget, type, layout, parent));
        }
        error_type create(gui_action& action, const string_view text = {}) noexcept
        {
            ICY_GUI_ERROR(m_system->create(action, text));
        }
        error_type initialize(shared_ptr<gui_model> model) noexcept
        {
            if (!model)
                return make_stdlib_error(std::errc::invalid_argument);                
            ICY_GUI_ERROR(m_system->initialize(model->m_view, model->m_base));
        }
        error_type insert(const gui_widget widget, const uint32_t x, const uint32_t y, const uint32_t dx, const uint32_t dy) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, x, y, dx, dy));
        }
        error_type insert(const gui_widget widget, const gui_action action) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, action));
        }
        error_type insert(const gui_widget widget, const uint32_t x, const uint32_t y) noexcept
        {
            return insert(widget, x, y, 1, 1);
        }
        error_type insert(const gui_widget widget) noexcept
        {
            return insert(widget, 0, 0, 1, 1);
        }
        error_type show(const gui_widget widget, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->show(widget, value));
        }
        error_type text(const gui_widget widget, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->text(widget, text));
        }
        error_type bind(const gui_action action, const gui_widget menu) noexcept
        {
            ICY_GUI_ERROR(m_system->bind(action, menu));
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
 }

inline icy::error_type icy::gui_queue::initialize() noexcept
{
    if (m_system)
        m_system->release();
    m_system = nullptr;

#if ICY_QTGUI_STATIC
    ICY_GUI_ERROR(icy_gui_system_create(ICY_GUI_VERSION, &m_system));    
#else
    ICY_ERROR(m_library.initialize());
    if (const auto func = ICY_FIND_FUNC(m_library, icy_gui_system_create))
    {
        const auto err = func(ICY_GUI_VERSION, &m_system);
        ICY_GUI_ERROR(err);
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);
#endif
    return{};
}