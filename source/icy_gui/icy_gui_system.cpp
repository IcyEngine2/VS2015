#include <icy_gui/icy_gui.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include "icy_gui_system.hpp"
#include "icy_gui_render.hpp"
#include "../libs/css/libcss/include/libcss/libcss.h"
#include "../libs/css/libwapcaplet/include/libwapcaplet/libwapcaplet.h"
#include "default_style.hpp"
using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct gui_system_event_type
{
    enum
    {
        none,
        create_style,
        create_model,
        create_window,
        create_bind,
    } type = none;
    weak_ptr<gui_model_data_usr> model_usr;
    weak_ptr<gui_window_data_usr> window_usr;
    gui_model_data_sys model_sys;
    gui_window_data_sys window_sys;
    gui_style_data style;
    gui_bind_data bind;
};
ICY_STATIC_NAMESPACE_END

gui_system_data::~gui_system_data() noexcept
{
    m_windows.clear();
    m_styles.clear();
    m_style = gui_style_data();
    if (m_ctx)
        lwc_shutdown(m_ctx);
    filter(0);
}
error_type gui_system_data::initialize(const adapter adapter) noexcept
{
    ICY_ERROR(m_lock.initialize());
    m_ctx = lwc_initialise(global_realloc, nullptr);
    if (!m_ctx)
        return make_stdlib_error(std::errc::not_enough_memory);

    ICY_ERROR(m_style.initialize(m_ctx, "default"_s, 
        string_view(default_css_style, strlen(default_css_style))));

    ICY_ERROR(make_shared(m_render_system, adapter));
    ICY_ERROR(m_render_system->initialize());
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    for (auto k = 0u; k < uint32_t(window_cursor::type::_count); ++k)
    {
        shared_ptr<window_cursor> new_cursor;
        ICY_ERROR(create_window_cursor(new_cursor, window_cursor::type(k)));
        ICY_ERROR(m_cursors.push_back(std::move(new_cursor)));
    }

    const uint64_t event_types = event_type::global_quit
        | event_type::system_internal 
        | event_type::window_input;

    filter(event_types);
    return error_type();
}
error_type gui_system_data::exec() noexcept
{
    while (true)
    {
        for (auto it = m_models.begin(); it != m_models.end();)
        {
            if (auto usr = shared_ptr<gui_model_data_usr>(it->value.user))
            {
                while (auto event = usr->next())
                {
                    ICY_SCOPE_EXIT{ gui_model_event_type::clear(event); };
                    ICY_ERROR(it->value.system.process(*event));
                }
                ++it;
            }
            else
            {
                it = m_models.erase(it);
            }
        }
        for (auto it = m_windows.begin(); it != m_windows.end(); )
        {
            if (auto usr = shared_ptr<gui_window_data_usr>(it->value.user))
            {
                while (auto event = usr->next())
                {
                    ICY_SCOPE_EXIT{ gui_window_event_type::clear(event); };
                    ICY_ERROR(it->value.system.process(*event));

                    if (event->type == gui_window_event_type::render)
                    {
                        auto query = 0u;
                        if (event->val.get(query))
                        {
                            gui_event new_event;
                            new_event.query = query;
                            new_event.window = usr;
                            new_event.widget.index = event->index;
                            ICY_ERROR(render(it->value.system, new_event));
                            ICY_ERROR(event::post(this, event_type::gui_render, std::move(new_event)));
                        }
                    }
                    else if (event->type == gui_window_event_type::resize)
                    {
                        window_size size;
                        if (event->val.get(size))
                        {
                            ICY_ERROR(it->value.system.resize(size));
                        }
                    }

                }
                ++it;
            }
            else
            {
                it = m_windows.erase(it);
            }
        }
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();

            if (event->type == event_type::window_resize || event->type == event_type::window_input)
            {
                const auto& event_data = event->data<window_message>();
                const auto event_window = shared_ptr<window>(event_data.window);
                auto find = m_windows.find(event_window ? event_window->handle() : nullptr);
                if (find != m_windows.end())
                {
                    if (auto usr_window = shared_ptr<gui_window_data_usr>(find->value.user))
                    {
                        if (event->type == event_type::window_input)
                        {
                            ICY_ERROR(find->value.system.input(event_data.input));
                        }
                    }
                    else
                    {
                        m_windows.erase(find);
                    }
                }
            }
            else if (event->type == event_type::system_internal)
            {
                auto& event_data = event->data<gui_system_event_type>();
                switch (event_data.type)
                {
                case gui_system_event_type::create_bind:
                {
                    ICY_ERROR(m_binds.push_back(std::move(event_data.bind)));
                    break;
                }
                case gui_system_event_type::create_style:
                {
                    string name;
                    ICY_ERROR(copy(event_data.style.name(), name));
                    ICY_ERROR(m_styles.insert(std::move(name), std::move(event_data.style)));
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
                        ICY_ERROR(m_windows.insert(pair.system.handle(), std::move(pair)));
                    }
                    break;
                }
                }

            }
        }
        ICY_ERROR(m_cvar.wait(m_lock));
    }
    return error_type();
}
error_type gui_system_data::create_bind(unique_ptr<gui_bind>&& bind, gui_model& model, gui_window& window, const gui_node node, const gui_widget widget) noexcept
{
    if (!bind)
        return make_stdlib_error(std::errc::invalid_argument);

    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_bind;
    new_event.bind.func = std::move(bind);
    new_event.bind.model = make_shared_from_this(&model);
    new_event.bind.window = make_shared_from_this(&window);
    new_event.bind.node = node;
    new_event.bind.widget = widget;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    return error_type();
}
error_type gui_system_data::create_style(const string_view name, const string_view css) noexcept
{
    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_style;
    ICY_ERROR(new_event.style.initialize(m_ctx, name, css));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    return error_type();
}
error_type gui_system_data::create_model(shared_ptr<gui_model>& model) noexcept
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
error_type gui_system_data::create_window(shared_ptr<gui_window>& window, shared_ptr<icy::window> handle) noexcept
{
    gui_system_event_type new_event;
    new_event.type = gui_system_event_type::create_window;

    shared_ptr<gui_window_data_usr> new_window;
    ICY_ERROR(make_shared(new_window, make_shared_from_this(this)));
    ICY_ERROR(new_window->initialize());
    
    new_event.window_usr = new_window;
    new_event.window_sys = gui_window_data_sys(this);
    ICY_ERROR(new_event.window_sys.initialize(handle));

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
        shared_ptr<gui_texture> new_texture;
        ICY_ERROR(m_render_system->create_texture(new_texture, { uint32_t(size.x), uint32_t(size.y) }, render_flags::none));
        ICY_ERROR(window.render(*new_texture));
        new_event.texture = std::move(new_texture);
    }
    return error_type();
}

error_type icy::create_gui_system(shared_ptr<gui_system>& system, const adapter adapter) noexcept
{
    shared_ptr<gui_system_data> new_system;
    ICY_ERROR(make_shared(new_system));
    ICY_ERROR(new_system->initialize(adapter));
    system = std::move(new_system);
    return error_type();
}