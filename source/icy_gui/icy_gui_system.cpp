#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include "icy_gui_system.hpp"
#include "icy_gui_render.hpp"
#include "icons/icons.hpp"
using namespace icy;

#if _DEBUG
#pragma comment(lib, "icy_engine_cored")
#pragma comment(lib, "icy_engine_imaged")
#else
#pragma comment(lib, "icy_engine_image")
#pragma comment(lib, "icy_engine_core")
#endif

#include <d2d1.h>

ICY_STATIC_NAMESPACE_BEG
struct gui_system_event_type
{
    enum
    {
        none,
        create_model,
        create_window,
        create_bind,
    } type = none;
    weak_ptr<gui_model_data_usr> model_usr;
    weak_ptr<gui_window_data_usr> window_usr;
    gui_model_data_sys model_sys;
    gui_window_data_sys window_sys;
    icy::unique_ptr<icy::gui_data_bind> bind;
    icy::gui_node node;
    icy::gui_widget widget;
};
ICY_STATIC_NAMESPACE_END

gui_system_data::~gui_system_data() noexcept
{
    if (m_thread)
        m_thread->wait();
    m_windows.clear();
    filter(0);
}
error_type gui_system_data::initialize() noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(make_shared(m_render_system));
    ICY_ERROR(m_render_system->initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    for (auto k = 1u; k < uint32_t(window_cursor::type::_count); ++k)
    {
        shared_ptr<window_cursor> new_cursor;
        ICY_ERROR(create_window_cursor(new_cursor, window_cursor::type(k)));
        ICY_ERROR(m_cursors.insert(k, std::move(new_cursor)));
    }

    m_vscroll.size_x = 21;
    m_vscroll.size_y = 17;

    m_hscroll.size_x = 17;
    m_hscroll.size_y = 21;
    
    const auto type = "image.png"_s;
    ICY_ERROR(blob_add(icon_vscroll_background, type, m_vscroll.background));
    ICY_ERROR(blob_add(icon_vscroll_max_default, type, m_vscroll.max_default));
    ICY_ERROR(blob_add(icon_vscroll_max_hovered, type, m_vscroll.max_hovered));
    ICY_ERROR(blob_add(icon_vscroll_max_focused, type, m_vscroll.max_focused));
    ICY_ERROR(blob_add(icon_vscroll_min_default, type, m_vscroll.min_default));
    ICY_ERROR(blob_add(icon_vscroll_min_hovered, type, m_vscroll.min_hovered));
    ICY_ERROR(blob_add(icon_vscroll_min_focused, type, m_vscroll.min_focused));
    ICY_ERROR(blob_add(icon_vscroll_val_default, type, m_vscroll.val_default));
    ICY_ERROR(blob_add(icon_vscroll_val_hovered, type, m_vscroll.val_hovered));
    ICY_ERROR(blob_add(icon_vscroll_val_focused, type, m_vscroll.val_focused));

    ICY_ERROR(blob_add(icon_hscroll_background, type, m_hscroll.background));
    ICY_ERROR(blob_add(icon_hscroll_max_default, type, m_hscroll.max_default));
    ICY_ERROR(blob_add(icon_hscroll_max_hovered, type, m_hscroll.max_hovered));
    ICY_ERROR(blob_add(icon_hscroll_max_focused, type, m_hscroll.max_focused));
    ICY_ERROR(blob_add(icon_hscroll_min_default, type, m_hscroll.min_default));
    ICY_ERROR(blob_add(icon_hscroll_min_hovered, type, m_hscroll.min_hovered));
    ICY_ERROR(blob_add(icon_hscroll_min_focused, type, m_hscroll.min_focused));
    ICY_ERROR(blob_add(icon_hscroll_val_default, type, m_hscroll.val_default));
    ICY_ERROR(blob_add(icon_hscroll_val_hovered, type, m_hscroll.val_hovered));
    ICY_ERROR(blob_add(icon_hscroll_val_focused, type, m_hscroll.val_focused));

    const uint64_t event_types = 0
        | event_type::system_internal 
        | event_type::window_input
        //| event_type::window_resize
        | event_type::window_action
        | event_type::global_timer;

    filter(event_types);
    return error_type();
}
error_type gui_system_data::exec() noexcept
{
    while (*this)
    {
        map<uint32_t, map<gui_window_data_usr*, array<gui_event>>> render_events;

        const auto erase_window = [this](decltype(m_windows.begin()) it)
        {
            return m_windows.erase(it);
        };
        const auto erase_model = [this](decltype(m_models.begin()) it)
        {
            return m_models.erase(it);
        };

        const auto update_model = [this](model_pair& pair, bool& erase)
        {
            if (auto usr = shared_ptr<gui_model_data_usr>(pair.user))
            {
                set<uint32_t> nodes;
                while (auto event = usr->next())
                {
                    ICY_SCOPE_EXIT{ gui_model_event_type::clear(event); };
                    ICY_ERROR(pair.system.process(*event, nodes));
                }
                ICY_ERROR(process_binds(pair.user, nodes));
            }
            else
            {
                erase = true;
            }
            return error_type();
        };
        const auto update_window = [this, &render_events](window_pair& pair, bool& erase)
        {
            if (auto usr = shared_ptr<gui_window_data_usr>(pair.user))
            {
                while (auto event = usr->next())
                {
                    ICY_SCOPE_EXIT{ gui_window_event_type::clear(event); };
                    ICY_ERROR(pair.system.process(*event));

                    if (event->type == gui_window_event_type::render)
                    {
                        auto query = 0u;
                        if (event->val.get(query))
                        {
                            gui_event new_event;
                            new_event.query = query;
                            new_event.window = usr;
                            new_event.widget.index = event->index;
                            auto thread = render_events.find(event->thread);
                            if (thread == render_events.end())
                            {
                                ICY_ERROR(render_events.insert(event->thread, map<gui_window_data_usr*, array<gui_event>>(), &thread));
                            }
                            auto map = thread->value.find(usr.get());
                            if (map == thread->value.end())
                            {
                                ICY_ERROR(thread->value.insert(usr.get(), array<gui_event>(), &map));
                            }
                            ICY_ERROR(map->value.push_back(std::move(new_event)));
                        }
                    }
                    else if (event->type == gui_window_event_type::resize)
                    {
                        window_size size;
                        if (event->val.get(size))
                        {
                            ICY_ERROR(pair.system.resize(size));
                        }
                    }
                    else
                    {
                        ICY_ERROR(process_binds(pair));
                    }
                }
            }
            else
            {
                erase = true;
            }
            return error_type();
        };

        for (auto it = m_models.begin(); it != m_models.end();)
        {
            auto erase = false;
            ICY_ERROR(update_model(it->value, erase));
            if (erase)
                it = erase_model(it);
            else
                ++it;
        }
        for (auto it = m_windows.begin(); it != m_windows.end(); )
        {
            auto erase = false;
            ICY_ERROR(update_window(it->value, erase));
            if (erase)
                it = erase_window(it);
            else
                ++it;
        }

        while (auto event = pop())
        {
            if (event->type == event_type::window_resize)
            {
                const auto& event_data = event->data<window_message>();
                for (auto it = m_windows.begin(); it != m_windows.end();)
                {
                    if (it->value.system.window() == event_data.window)
                    {
                        auto erase = false;
                        ICY_ERROR(update_window(it->value, erase));
                        if (erase)
                        {
                            it = erase_window(it);
                        }
                        else
                        {
                            ICY_ERROR(it->value.system.resize(event_data.size));
                            ++it;
                        }
                    }
                }
            }
            else if (event->type == event_type::window_input)
            {
                const auto& event_data = event->data<window_message>();
                for (auto it = m_windows.begin(); it != m_windows.end();)
                {
                    if (it->value.system.window() == event_data.window)
                    {
                        auto erase = false;
                        ICY_ERROR(update_window(it->value, erase));
                        if (erase)
                        {
                            it = erase_window(it);
                        }
                        else
                        {
                            ICY_ERROR(it->value.system.input(event_data.input));
                            ICY_ERROR(process_binds(it->value));
                            ++it;
                        }
                    }
                }
            }
            else if (event->type == event_type::window_action)
            {
                const auto& event_data = event->data<window_action>();
                for (auto it = m_windows.begin(); it != m_windows.end();)
                {
                    if (it->value.system.window() == event_data.window)
                    {
                        auto erase = false;
                        ICY_ERROR(update_window(it->value, erase));
                        if (erase)
                        {
                            it = erase_window(it);
                        }
                        else
                        {
                            ICY_ERROR(it->value.system.action(event_data));
                            ICY_ERROR(process_binds(it->value));
                            
                            ++it;
                        }
                    }
                }
            }
            else if (event->type == event_type::global_timer)
            {
                auto& event_data = event->data<timer::pair>();
                for (auto&& pair : m_windows)
                {
                    ICY_ERROR(pair.value.system.timer(event_data));
                }
            }
            else if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<gui_system_event_type>();
                switch (event_data.type)
                {
                case gui_system_event_type::create_bind:
                {
                    bind_tuple bind;
                    bind.func = std::move(event_data.bind);
                    bind.window = event_data.window_usr;
                    bind.widget = event_data.widget;
                    bind.node = event_data.node;
                    bind.model = event_data.model_usr;
                    auto erase = false;

                    auto it = m_windows.find(shared_ptr<gui_window_data_usr>(event_data.window_usr).get());
                    auto jt = m_models.find(shared_ptr<gui_model_data_usr>(event_data.model_usr).get());
                    if (it != m_windows.end() && jt != m_models.end())
                    {
                        auto do_erase_window = false;
                        auto do_erase_model = false;
                        ICY_ERROR(update_window(it->value, do_erase_window));
                        ICY_ERROR(update_model(jt->value, do_erase_model));
                        if (do_erase_window)
                        {
                            erase_window(it);
                            break;
                        }
                        if (do_erase_model)
                        {
                            erase_model(jt);
                            break;
                        }
                        ICY_ERROR(process_bind(bind, gui_bind_type::model_to_window, erase));
                        if (!erase)
                        {
                            ICY_ERROR(m_binds.insert(std::move(bind)));
                        }
                    }

                    break;
                }
                case gui_system_event_type::create_model:
                {
                    if (auto usr = shared_ptr<gui_model_data_usr>(event_data.model_usr))
                    {
                        model_pair pair;
                        pair.user = usr;
                        pair.system = std::move(event_data.model_sys);
                        ICY_ERROR(m_models.insert(usr.get(), std::move(pair)));                        
                    }
                    break;
                }
                case gui_system_event_type::create_window:
                {
                    if (auto usr = shared_ptr<gui_window_data_usr>(event_data.window_usr))
                    {
                        window_pair pair;
                        pair.user = usr;
                        pair.system = std::move(event_data.window_sys);
                        ICY_ERROR(m_windows.insert(usr.get(), std::move(pair)));
                    }
                    break;
                }
                }
            }
        }
        for (auto&& pair : m_windows)
        {
            if (m_update.try_find(&pair.value.system))
            {
                gui_event event;
                if (event.window = shared_ptr<gui_window_data_usr>(pair.value.user))
                {
                    ICY_ERROR(event::post(this, event_type::gui_update, std::move(event)));
                }
            }
            if (auto select = m_select.try_find(&pair.value.system))
            {
                gui_event event;
                if (event.window = shared_ptr<gui_window_data_usr>(pair.value.user))
                {
                    for (auto&& bind : m_binds)
                    {
                        if (bind.window == event.window)
                        {
                            if (bind.widget.index != select->first)
                                continue;

                            event.widget = bind.widget;
                            event.model = shared_ptr<gui_data_write_model>(bind.model);
                            auto sys_model = m_models.try_find(event.model.get());
                            if (event.model && sys_model)
                            {
                                event.node = select->second ? gui_node { select->second } : bind.node;
                                event.data = sys_model->system.query(event.node, gui_node_prop::user);
                                ICY_ERROR(event::post(this, event_type::gui_select, std::move(event)));                                
                            }
                        }
                    }
                }
            }
            if (auto context = m_context.try_find(&pair.value.system))
            {
                gui_event event;
                if (event.window = shared_ptr<gui_window_data_usr>(pair.value.user))
                {
                    for (auto&& bind : m_binds)
                    {
                        if (bind.window == event.window)
                        {
                            if (bind.widget.index != context->first)
                                continue;

                            event.widget = bind.widget;
                            event.model = shared_ptr<gui_data_write_model>(bind.model);
                            auto sys_model = m_models.try_find(event.model.get());
                            if (event.model && sys_model)
                            {
                                event.node = context->second ? gui_node { context->second } : bind.node;
                                event.data = sys_model->system.query(event.node, gui_node_prop::user);
                                ICY_ERROR(event::post(this, event_type::gui_context, std::move(event)));
                            }
                        }
                    }
                }
            }
        }
        m_update.clear();
        m_select.clear();
        m_context.clear();

        for (auto&& pair_thread_map : render_events)
        {
            for (auto&& pair_window_array : pair_thread_map.value)
            {
                auto& vec = pair_window_array.value;
                auto usr = m_windows.find(pair_window_array.key);
                if (usr != m_windows.end())
                {
                    ICY_ERROR(render(usr->value.system, vec.back()));
                }
                for (auto&& event : vec)
                {
                    ICY_ERROR(event::post(this, event_type::gui_render, std::move(event)));
                }
            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type gui_system_data::create_bind(unique_ptr<gui_data_bind>&& bind, gui_data_write_model& model, gui_window& window, const gui_node node, const gui_widget widget) noexcept
{
    if (!bind)
        return make_stdlib_error(std::errc::invalid_argument);

    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_bind;
    new_event.bind = std::move(bind);
    new_event.model_usr = make_shared_from_this(&model);
    new_event.window_usr = make_shared_from_this(&window);
    new_event.node = node;
    new_event.widget = widget;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    return error_type();
}
error_type gui_system_data::create_model(shared_ptr<gui_data_write_model>& model) noexcept
{
    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_model;

    shared_ptr<gui_model_data_usr> new_model;
    ICY_ERROR(make_shared(new_model, make_shared_from_this(this)));
    ICY_ERROR(new_model->initialize());

    new_event.model_usr = new_model;
    ICY_ERROR(new_event.model_sys.initialize());

    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    model = std::move(new_model);
    return error_type();
}
error_type gui_system_data::create_window(shared_ptr<gui_window>& window, shared_ptr<icy::window> handle, const string_view json_str) noexcept
{
    icy::json json;
    if (!json_str.empty())
    {
        ICY_ERROR(to_value(json_str, json));
    }

    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_window;

    shared_ptr<gui_window_data_usr> new_window;
    ICY_ERROR(make_shared(new_window, make_shared_from_this(this)));
    ICY_ERROR(new_window->initialize(json));
    
    new_event.window_usr = new_window;
    new_event.window_sys = gui_window_data_sys(this);
    ICY_ERROR(new_event.window_sys.initialize(handle, json));

    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    window = std::move(new_window);
    return error_type();
}
error_type gui_system_data::enum_font_names(array<string>& fonts) const noexcept
{
    return m_render_system->enum_font_names(fonts);
}
error_type gui_system_data::render(gui_window_data_sys& window, icy::gui_event& new_event) noexcept
{
    ICY_ERROR(window.update());
    const auto size = window.size(new_event.widget.index);
    if (size.x > 0 && size.y > 0)
    {
        ICY_ERROR(window.render(new_event.render));
    }
    return error_type();
}
error_type gui_system_data::process_binds(weak_ptr<gui_model_data_usr> model, const_array_view<uint32_t> nodes) noexcept
{
    for (auto&& node : nodes)
    {
        for (auto it = m_binds.begin(); it != m_binds.end();)
        {
            if (it->model == model && it->node.index == node)
            {
                auto erase = false;
                ICY_ERROR(process_bind(*it, gui_bind_type::model_to_window, erase));
                if (erase)
                {
                    it = m_binds.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
    return error_type();
}
error_type gui_system_data::process_binds(const window_pair& window) noexcept
{
    const auto list = m_change.find(&window.system);
    if (list == m_change.end())
        return error_type();

    for (auto it = m_binds.begin(); it != m_binds.end();)
    {
        if (it->window == window.user && list->value.try_find(it->widget.index))
        {
            auto erase = false;
            ICY_ERROR(process_bind(*it, gui_bind_type::window_to_model, erase));
            if (erase)
            {
                it = m_binds.erase(it);
                continue;
            }
        }
        ++it;
    }
    m_change.erase(list);
    return error_type();

}
error_type gui_system_data::process_bind(const bind_tuple& bind, const gui_bind_type type, bool& erase) noexcept
{
    auto usr_model = shared_ptr<gui_model_data_usr>(bind.model);
    auto usr_window = shared_ptr<gui_window_data_usr>(bind.window);
    if (!usr_model || !usr_window)
    {
        erase = true;
        return error_type();
    }
    auto sys_model = m_models.find(usr_model.get());
    auto sys_window = m_windows.find(usr_window.get());
    if (sys_model == m_models.end() || sys_window == m_windows.end())
    {
        erase = true;
        return error_type();
    }

    auto& model = sys_model->value.system;
    auto& window = sys_window->value.system;

    if (type == gui_bind_type::model_to_window)
    {
        ICY_ERROR(model.send_data(window, bind.widget, bind.node, *bind.func, erase));
    }
    else if (type == gui_bind_type::window_to_model)
    {
        ICY_ERROR(window.send_data(model, bind.widget, bind.node, *bind.func, erase));
    }

    return error_type();
}

error_type icy::create_gui_system(shared_ptr<gui_system>& system) noexcept
{
    shared_ptr<gui_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize());
    system = std::move(new_system);
    return error_type();
}