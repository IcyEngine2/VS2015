#pragma once

#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_gui/icy_gui.hpp>
#include "icy_gui_render.hpp"
#include "icy_gui_attr.hpp"
#include <cfloat>

namespace icy
{
    class json;
}
class gui_texture;
class gui_system_data;
class gui_model_proxy_read;
class gui_model_proxy_write;

struct IUnknown;
struct am_Var;
struct am_Solver;
enum class gui_widget_item_type : uint32_t
{
    none,
    text,
    row_header,
    col_header,
    vsplitter,
    hsplitter,
    _scroll_beg,
    vscroll_bk = _scroll_beg,
    vscroll_min,
    vscroll_max,
    vscroll_val,
    hscroll_bk,
    hscroll_min,
    hscroll_max,
    hscroll_val,
    _scroll_end
};
enum class gui_node_state : uint32_t;
struct gui_widget_item
{
    gui_widget_item(const gui_widget_item_type type = gui_widget_item_type::none) noexcept : type(type)
    {

    }
    gui_widget_item_type type;
    gui_node_state state = gui_node_state(0);
    icy::gui_variant value;
    icy::color color = icy::colors::white;
    gui_text text;
    gui_image image;
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    uint32_t node = 0;
};
enum class gui_reset_reason : uint32_t
{
    update_render_list,
    update_text_caret,
    update_text_select,
    update_splitter,
};
enum class gui_text_keybind : uint32_t
{
    text,
    paste,
    cut,
    copy,
    select_all,
    left,
    right,
    up,
    down,
    del,
    back,
    ctrl_left,
    ctrl_right,
    ctrl_up,
    ctrl_down,
    shift_left,
    shift_right,
    shift_up,
    shift_down,
    ctrl_shift_left,
    ctrl_shift_right,
    ctrl_del,
    ctrl_back,
    undo,
    redo,
};
struct gui_text_action
{
    uint32_t item = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    icy::string value;
};
struct gui_widget_data
{
public:
    struct insert_args
    {
        uint32_t parent = 0;
        uint32_t index = 0;
        uint32_t offset = 0;
        icy::gui_widget_type type = icy::gui_widget_type::none;
        icy::gui_widget_layout layout = icy::gui_widget_layout::none;
    };
    struct modify_args
    {
        uint32_t index = 0;
        icy::string key;
        icy::gui_variant val;
    };
    struct border_type
    {
        float size = 0;
        icy::color color;
    };
    struct offset_type
    {
        float size = 0;
    };
    struct margin_type
    {
        float size = 0;
    };
    struct scroll_type
    {
        void clamp() noexcept
        {
            val = std::max(0.0f, std::min(std::max(0.0f, max - view_size), val));
            exp = std::max(0.0f, std::min(std::max(0.0f, max - view_size), exp));
        }
        float step = 0;
        float val = 0;
        float exp = 0;
        float max = 0;
        float area_size = 0;
        float view_size = 0;
    };
    static icy::error_type initialize(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const uint32_t index, const icy::json& json);
    static icy::error_type insert(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const insert_args& args) noexcept;
    static icy::error_type modify(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const modify_args& args) noexcept;
    static bool erase(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const uint32_t index) noexcept;
public:
    gui_widget_data(const gui_widget_data* const parent = nullptr, const uint32_t index = 0) noexcept : 
        parent(parent), index(index)
    {
        static_assert(std::is_nothrow_constructible<gui_widget_data>::value, "");
    }
public:
    const uint32_t index;
    const gui_widget_data* parent;
    mutable icy::array<gui_widget_data*> children;
    icy::gui_widget_type type = icy::gui_widget_type::none;
    icy::gui_widget_layout layout = icy::gui_widget_layout::none;
    icy::gui_widget_state state = icy::gui_widget_state::_default;
    icy::array<gui_widget_item> items;
    icy::map<gui_widget_attr, icy::gui_variant> attr;
public:
    //float font_size = 0;
    border_type borders[4];
    margin_type margins[4];
    offset_type padding[4];
    am_Var* min_x = nullptr;
    am_Var* min_y = nullptr;
    am_Var* max_x = nullptr;
    am_Var* max_y = nullptr;
    float size_x = 0;
    float size_y = 0;
    float weight_x = 0;
    float weight_y = 0;
    scroll_type scroll_x;
    scroll_type scroll_y;
    float row_header = 0;
    float col_header = 0;
    icy::array<gui_text_action> actions;
    uint32_t action = 0;
};
struct gui_window_event_type
{
    void* _unused = nullptr;
    enum { none, create, destroy, modify, render, resize } type = none;
    uint32_t index = 0;
    icy::gui_variant val;
    const uint32_t thread = icy::thread::this_index();
    static icy::error_type make(gui_window_event_type*& new_event, const decltype(none) type) noexcept
    {
        new_event = icy::allocator_type::allocate<gui_window_event_type>(1);
        if (!new_event)
            return icy::make_stdlib_error(std::errc::not_enough_memory);
        icy::allocator_type::construct(new_event);
        new_event->type = type;
        return icy::error_type();
    }
    static void clear(gui_window_event_type*& new_event) noexcept
    {
        if (new_event)
        {
            icy::allocator_type::destroy(new_event);
            icy::allocator_type::deallocate(new_event);
        }
        new_event = nullptr;
    }
};
class gui_window_data_usr : public icy::gui_window
{
public:
    gui_window_data_usr(const icy::weak_ptr<icy::gui_system> system) noexcept : m_system(system)
    {

    }
    ~gui_window_data_usr() noexcept;
    gui_window_event_type* next() noexcept
    {
        return static_cast<gui_window_event_type*>(m_events.pop());
    }
    icy::error_type initialize(const icy::json& json) noexcept
    {
        icy::unique_ptr<gui_widget_data> root;
        ICY_ERROR(icy::make_unique(gui_widget_data(), root));
        ICY_ERROR(m_data.insert(0u, std::move(root)));
        ICY_ERROR(gui_widget_data::initialize(m_data, 0u, json));
        m_index = uint32_t(m_data.size());
        return icy::error_type();
    }
private:
    void notify(gui_window_event_type*& event) const noexcept;
    icy::error_type render(const icy::gui_widget widget, uint32_t& query) const noexcept override;
    icy::error_type resize(const icy::window_size size) noexcept override;
    icy::error_type update() noexcept override;
    icy::gui_widget parent(const icy::gui_widget widget) const noexcept override;
    icy::gui_widget child(const icy::gui_widget widget, size_t offset) const noexcept override;
    size_t offset(const icy::gui_widget widget) const noexcept override;
    icy::gui_variant query(const icy::gui_widget widget, const icy::string_view prop) const noexcept override;
    icy::gui_widget_type type(const icy::gui_widget widget) const noexcept override;
    icy::gui_widget_layout layout(const icy::gui_widget widget) const noexcept override;
    icy::gui_widget_state state(const icy::gui_widget widget) const noexcept override;
    icy::error_type modify(const icy::gui_widget widget, const icy::string_view prop, const icy::gui_variant& value) noexcept override;
    icy::error_type insert(const icy::gui_widget parent, const size_t offset, const icy::gui_widget_type type, const icy::gui_widget_layout layout, icy::gui_widget& widget) noexcept override;
    icy::error_type destroy(const icy::gui_widget widget) noexcept override;
    icy::error_type find(const icy::string_view prop, const icy::gui_variant value, icy::array<icy::gui_widget>& list) const noexcept override;
private:
    icy::weak_ptr<icy::gui_system> m_system;
    mutable icy::detail::intrusive_mpsc_queue m_events;
    icy::map<uint32_t, icy::unique_ptr<gui_widget_data>> m_data;
    uint32_t m_index = 1;
};

struct gui_node_data;
class gui_model_data_sys;
class gui_window_data_sys
{
public:
    gui_window_data_sys(gui_system_data* system = nullptr) noexcept : m_system(system)
    {

    }
    gui_window_data_sys(const gui_window_data_sys&) = delete;
    gui_window_data_sys(gui_window_data_sys&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(gui_window_data_sys);
    icy::error_type initialize(const icy::shared_ptr<icy::window> window, const icy::json& json) noexcept;
    icy::error_type input(const icy::input_message& msg) noexcept;
    icy::error_type action(const icy::window_action& msg) noexcept;
    icy::error_type resize(const icy::window_size size) noexcept;
    icy::error_type process(const gui_window_event_type& event) noexcept;
    uint32_t window() const noexcept
    {
        return m_window_index;
    }
    icy::error_type update() noexcept;
    icy::error_type render(gui_texture& texture) noexcept;
    const gui_system_data& system() const noexcept
    {
        return *m_system;
    }
    gui_font font() const noexcept
    {
        return m_font;
    }
    float dpi() const noexcept
    {
        return m_dpi;
    }
    icy::window_size size(const uint32_t widget) const noexcept;
    icy::error_type send_data(gui_model_data_sys& model, const icy::gui_widget widget, const icy::gui_node node, const icy::gui_data_bind& func, bool& erase) noexcept;
    icy::error_type recv_data(const gui_model_proxy_read& proxy, const gui_node_data& node, const icy::gui_widget widget, const icy::gui_data_bind& func, bool& erase) noexcept;
    icy::error_type timer(icy::timer::pair& pair) noexcept;
    icy::error_type reset(const gui_reset_reason reason) noexcept;
private:
    struct jmp_type { jmp_buf buf; };
    struct solver_type
    {
        solver_type()noexcept = default;
        solver_type(solver_type&& rhs) noexcept : system(rhs.system)
        {
            rhs.system = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(solver_type);
        ~solver_type() noexcept;
        am_Solver& operator*() const noexcept
        {
            return *system;
        }
        operator am_Solver* () const noexcept
        {
            return system;
        }
        am_Solver* system = nullptr;
    };
    struct hover_type 
    {
        explicit operator bool() const noexcept
        {
            return widget != 0;
        }
        uint32_t widget = 0;
        uint32_t item = 0;
        uint32_t dx = 0;
        uint32_t dy = 0;
    };
    using press_type = hover_type;
    struct focus_type 
    {
        explicit operator bool() const noexcept
        {
            return widget != 0;
        }
        uint32_t widget = 0;
        uint32_t item = 0;  
        uint32_t text_select_beg = 0;
        uint32_t text_select_end = 0;
        bool text_trail_beg = false;
        bool text_trail_end = false;
    };
    struct timer_type
    {
        icy::error_type reset(const icy::duration_type timeout) noexcept
        {
            using namespace icy;
            if (!timer)
            {
                timer = make_unique<icy::timer>();
                if (!timer)
                    return make_stdlib_error(std::errc::not_enough_memory);
            }
            point = clock_type::now();
            return timer->initialize(SIZE_MAX, timeout);
        }
        void cancel() noexcept
        {
            if (timer)
                timer->cancel();
        }
        bool check(icy::timer::pair& pair, float& dt) noexcept
        {
            if (pair.timer != timer.get())
                return false;

            const auto now = icy::clock_type::now();
            dt = float(std::chrono::duration_cast<std::chrono::nanoseconds>(now - point).count() / 1e9);
            point = now;
            return true;
        }
        icy::unique_ptr<icy::timer> timer;
        icy::clock_type::time_point point;
    };
    struct render_item
    {
        gui_widget_item* item;
        float x = 0;
        float y = 0;
    };
    struct render_widget
    {
        gui_widget_data* widget = nullptr;
        icy::array<render_item> items;
        size_t level = 0;
    };
private:
    icy::error_type update(gui_widget_data& widget, size_t& level) noexcept;
    icy::error_type input_key_press(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_key_hold(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_key_release(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_move(const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_wheel(const int32_t px, const int32_t py, const icy::key_mod mods, const int32_t wheel) noexcept;
    icy::error_type input_mouse_release(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_press(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_double(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_text(const icy::string_view text) noexcept;
    icy::error_type solve_item(gui_widget_data& widget, gui_widget_item& item, const gui_font& font) noexcept;
    icy::error_type make_view(const gui_model_proxy_read& proxy, const gui_node_data& node, const icy::gui_data_bind& func, gui_widget_data& widget, size_t level = 0) noexcept;
    icy::error_type click_widget_lmb(gui_widget_data& widget, gui_widget_item* item) noexcept;
    icy::error_type push_action(gui_widget_data& widget, gui_text_action& action) noexcept;
    icy::error_type exec_action(gui_widget_data& widget, gui_text_action& action) noexcept;
    icy::error_type text_action(gui_widget_data& widget, const uint32_t item, const gui_text_keybind type, const icy::string_view insert = icy::string_view()) noexcept;
private:
    gui_system_data* m_system = nullptr;
    icy::weak_ptr<icy::window> m_window;
    uint32_t m_window_index = 0;
    icy::map<uint32_t, icy::unique_ptr<gui_widget_data>> m_data;
    icy::unique_ptr<jmp_type> m_jmp;
    solver_type m_solver;
    uint32_t m_state = 0;
    gui_font m_font;
    float m_dpi = 96;
    icy::window_size m_size;
    hover_type m_hover;
    press_type m_press;
    focus_type m_focus;
    timer_type m_vscroll_val;
    timer_type m_hscroll_val;
    timer_type m_vscroll_min;
    timer_type m_vscroll_max;
    timer_type m_hscroll_min;
    timer_type m_hscroll_max;
    timer_type m_caret_timer;
    bool m_caret_visible = false;
    icy::array<render_widget> m_render_list;
    float m_splitter = 0;
    int32_t m_last_x = 0;
    int32_t m_last_y = 0;
};