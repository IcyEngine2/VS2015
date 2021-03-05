#pragma once

#include <icy_engine/core/icy_set.hpp>
#include <icy_gui/icy_gui.hpp>
#include "icy_gui_model.hpp"
#include "icy_gui_window.hpp"
#include "icy_gui_style.hpp"
#include "icy_gui_bind.hpp"

class gui_render_system;

class gui_thread_data : public icy::thread
{
public:
    icy::gui_system* system = nullptr;
    void cancel() noexcept override
    {
        system->post(nullptr, icy::event_type::global_quit);
    }
    icy::error_type run() noexcept override
    {
        if (auto error = system->exec())
            return icy::event::post(system, icy::event_type::system_error, std::move(error));
        return icy::error_type();
    }
};
class gui_system_data : public icy::gui_system
{
public:
    ~gui_system_data();
    icy::error_type initialize(const icy::adapter adapter) noexcept;
    icy::error_type signal(const icy::event_data& event) noexcept override
    {
        m_cvar.wake();
        return icy::error_type();
    }
    void wake()
    {
        return m_cvar.wake();
    }   
    uint32_t next_query() noexcept
    {
        return m_query.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    icy::error_type create_css(gui_select_system& system) noexcept
    {
        ICY_ERROR(system.initialize(m_ctx));
        ICY_ERROR(system.append(m_style));
        return icy::error_type();
    }
    const gui_render_system& render() const noexcept
    {
        return *m_render_system;
    }
private:
    icy::error_type exec() noexcept override;
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    icy::error_type create_bind(icy::unique_ptr<icy::gui_bind>&& bind, icy::gui_model& model, icy::gui_window& window, const icy::gui_node node, const icy::gui_widget widget) noexcept override;
    icy::error_type create_style(const icy::string_view name, const icy::string_view css) noexcept override;
    icy::error_type create_model(icy::shared_ptr<icy::gui_model>& model)noexcept override;
    icy::error_type create_window(icy::shared_ptr<icy::gui_window>& window, icy::shared_ptr<icy::window> handle)noexcept override;
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept override;
    icy::error_type render(gui_window_data_sys& window, icy::gui_event& new_event) noexcept;   
private:
    struct window_pair
    {
        gui_window_data_sys system;
        icy::weak_ptr<gui_window_data_usr> user;
    };
    struct model_pair
    {
        gui_model_data_sys system;
        icy::weak_ptr<gui_model_data_usr> user;
    };
    icy::mutex m_lock;
    icy::cvar m_cvar;
    lwc_context_s* m_ctx = nullptr;
    icy::shared_ptr<gui_render_system> m_render_system;
    icy::shared_ptr<gui_thread_data> m_thread;
    icy::array<icy::shared_ptr<icy::window_cursor>> m_cursors;
    gui_style_data m_style;
    icy::map<icy::string, gui_style_data> m_styles;
    icy::map<void*, model_pair> m_models;
    icy::map<void*, window_pair> m_windows;
    icy::array<gui_bind_data> m_binds;
    std::atomic<uint32_t> m_query = 0;
};