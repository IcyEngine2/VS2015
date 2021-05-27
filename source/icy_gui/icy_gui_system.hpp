#pragma once

#include <icy_engine/core/icy_set.hpp>
#include <icy_gui/icy_gui.hpp>
#include "icy_gui_model.hpp"
#include "icy_gui_window.hpp"

class gui_render_system;

class gui_thread_data : public icy::thread
{
public:
    icy::gui_system* system = nullptr;
    void cancel() noexcept override
    {
        icy::post_quit_event();
    }
    icy::error_type run() noexcept override
    {
        if (auto error = system->exec())
        {
            cancel();
            return error;
        }
        return icy::error_type();
    }
};
class gui_system_data : public icy::gui_system
{
public:
    struct bind_tuple
    {
        static int compare(const bind_tuple& lhs, const bind_tuple& rhs) noexcept;
        rel_ops(bind_tuple);
        icy::unique_ptr<icy::gui_data_bind> func;
        icy::weak_ptr<gui_window_data_usr> window;
        icy::weak_ptr<gui_model_data_usr> model;
        icy::gui_node node;
        icy::gui_widget widget;
    };
    struct scroll_type
    {
        float size_x = 0;
        float size_y = 0;
        icy::blob background;
        icy::blob val_default;
        icy::blob val_hovered;
        icy::blob val_focused;
        icy::blob min_default;
        icy::blob min_hovered;
        icy::blob min_focused;
        icy::blob max_default;
        icy::blob max_hovered;
        icy::blob max_focused;
    };
public:
    ~gui_system_data();
    icy::error_type exec() noexcept override;
    icy::error_type initialize() noexcept;
    icy::error_type signal(const icy::event_data* event) noexcept override
    {
        return m_sync.wake();
    }
    icy::error_type wake() noexcept
    {
        return m_sync.wake();
    }
    uint32_t next_query() noexcept
    {
        return m_query.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    const gui_render_system& render() const noexcept
    {
        return *m_render_system;
    }
    const scroll_type& vscroll() const noexcept
    {
        return m_vscroll;
    }
    const scroll_type& hscroll() const noexcept
    {
        return m_hscroll;
    }
    icy::error_type post_update(gui_window_data_sys& window)
    {
        return m_update.try_insert(&window);
    }
    icy::error_type post_change(gui_window_data_sys& window, const icy::gui_widget widget) noexcept
    {
        auto it = m_change.find(&window);
        if (it == m_change.end())
        {
            ICY_ERROR(m_change.insert(&window, icy::set<uint32_t>(), &it));
        }
        return it->value.try_insert(widget.index);
    }
    icy::error_type post_select(gui_window_data_sys& window, const uint32_t widget, const uint32_t node) noexcept
    {
        auto it = m_select.find(&window);
        if (it == m_select.end())
        {
            ICY_ERROR(m_select.insert(&window, std::pair<uint32_t, uint32_t>(), &it));
        }
        it->value = { widget, node };
        return icy::error_type();
    }
    icy::error_type post_context(gui_window_data_sys& window, const uint32_t widget, const uint32_t node) noexcept
    {
        auto it = m_context.find(&window);
        if (it == m_context.end())
        {
            ICY_ERROR(m_context.insert(&window, std::pair<uint32_t, uint32_t>(), &it));
        }
        it->value = { widget, node };
        return icy::error_type();
    }
    icy::duration_type caret_blink_time() const noexcept
    {
        return std::chrono::milliseconds(400);
    }
    icy::duration_type scroll_update_freq() const noexcept
    {
        return std::chrono::milliseconds(30);
    }
    icy::shared_ptr<icy::window_cursor> cursor(const icy::window_cursor::type type) const noexcept
    {
        if (auto ptr = m_cursors.try_find(uint32_t(type)))
            return *ptr;
        return nullptr;
    }
    gui_model_data_sys* model(icy::weak_ptr<icy::gui_data_write_model> ptr) noexcept
    {
        for (auto&& pair : m_models)
        {
            if (pair.value.user == ptr)
                return &pair.value.system;
        }
        return nullptr;
    }
    icy::shared_ptr<icy::gui_window> window(gui_window_data_sys& sys) noexcept
    {
        for (auto&& pair : m_windows)
        {
            if (&pair.value.system == &sys)
                return icy::shared_ptr<gui_window_data_usr>(pair.value.user);
        }
        return nullptr;
    }
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
    enum class gui_bind_type
    {
        window_to_model,
        model_to_window,
    };
private:
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    icy::error_type create_bind(icy::unique_ptr<icy::gui_data_bind>&& bind, icy::gui_data_write_model& model, icy::gui_window& window, const icy::gui_node node, const icy::gui_widget widget) noexcept override;
    icy::error_type create_model(icy::shared_ptr<icy::gui_data_write_model>& model) noexcept override;
    icy::error_type create_window(icy::shared_ptr<icy::gui_window>& window, icy::shared_ptr<icy::window> handle, const icy::string_view json) noexcept override;
    icy::error_type enum_font_names(icy::array<icy::string>& fonts) const noexcept override;
    icy::error_type render(gui_window_data_sys& window, icy::gui_event& new_event) noexcept; 
    icy::error_type process_binds(icy::weak_ptr<gui_model_data_usr> model, icy::const_array_view<uint32_t> nodes) noexcept;
    icy::error_type process_binds(const window_pair& window) noexcept;
    icy::error_type process_bind(const bind_tuple& bind, const gui_bind_type type, bool& erase) noexcept;
private:
    icy::sync_handle m_sync;
    icy::shared_ptr<gui_render_system> m_render_system;
    icy::shared_ptr<gui_thread_data> m_thread;
    icy::map<uint32_t, icy::shared_ptr<icy::window_cursor>> m_cursors;
    icy::map<gui_model_data_usr*, model_pair> m_models;
    icy::map<gui_window_data_usr*, window_pair> m_windows;
    icy::set<bind_tuple> m_binds;
    std::atomic<uint32_t> m_query = 0;
    scroll_type m_vscroll;
    scroll_type m_hscroll;
    icy::set<gui_window_data_sys*> m_update;
    icy::map<gui_window_data_sys*, icy::set<uint32_t>> m_change;
    icy::map<gui_window_data_sys*, std::pair<uint32_t, uint32_t>> m_select;
    icy::map<gui_window_data_sys*, std::pair<uint32_t, uint32_t>> m_context;
};
namespace icy
{
    template<> inline int compare(const gui_system_data::bind_tuple& lhs, const gui_system_data::bind_tuple& rhs) noexcept
    {
        return icy::compare(lhs.func.get(), rhs.func.get());
    }
}