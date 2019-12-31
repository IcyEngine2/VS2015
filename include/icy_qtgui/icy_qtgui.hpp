#pragma once

#include "icy_qtgui_system.hpp"
#include "icy_qtgui_model.hpp"
#include <icy_engine/core/icy_map.hpp>

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
        error_type clear() noexcept
        {
            if (!m_vtbl.clear)
                return make_stdlib_error(std::errc::function_not_supported);
            ICY_GUI_ERROR(m_vtbl.clear(m_view));
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
        error_type insert(const gui_widget widget, const gui_insert args = {}) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, args));
        }
        error_type insert(const gui_widget widget, const gui_action action) noexcept
        {
            ICY_GUI_ERROR(m_system->insert(widget, action));
        }
        error_type show(const gui_widget widget, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->show(widget, value));
        }
        error_type text(const gui_widget widget, const string_view text) noexcept
        {
            ICY_GUI_ERROR(m_system->text(widget, text));
        }
        error_type enable(const gui_action action, const bool value) noexcept
        {
            ICY_GUI_ERROR(m_system->enable(action, value));
        }
        error_type bind(const gui_action action, const gui_widget menu) noexcept
        {
            ICY_GUI_ERROR(m_system->bind(action, menu));
        }
        error_type modify(const gui_widget widget, const string_view args) noexcept
        {
            ICY_GUI_ERROR(m_system->modify(widget, args));
        }
        error_type exec(const gui_widget widget, gui_action& action) noexcept
        {
            ICY_ERROR(show(widget, true));
            
            shared_ptr<event_loop> loop;
            ICY_ERROR(event_loop::create(loop, event_type::gui_action | event_type::window_close));
            
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