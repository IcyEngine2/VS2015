#pragma once

#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_gui/icy_gui.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include "icy_gui_style.hpp"
#include "icy_gui_render.hpp"
#include <cfloat>

class gui_texture;
struct IUnknown;

enum gui_widget_state : uint32_t
{
    gui_widget_state_none       =   0x0000,
    gui_widget_state_enabled    =   0x0001,
    gui_widget_state_visible    =   0x0002,
    gui_widget_state_link       =   0x0004,
    gui_widget_state_visited    =   0x0008,
    gui_widget_state_hovered    =   0x0010,
    gui_widget_state_active     =   0x0020,
    gui_widget_state_focused    =   0x0040,
    gui_widget_state_checked    =   0x0080,
    gui_widget_state_target     =   0x0100,
    gui_widget_state_collapse   =   0x0200,
    gui_widget_state_has_border =   0x0400,
};
struct gui_widget_text
{
    icy::gui_variant value;
    icy::color color;
    gui_text layout;
};
struct gui_widget_data
{
public:
    struct insert_args
    {
        uint32_t parent = 0;
        uint32_t index = 0;
        uint32_t offset = 0;
        icy::string type;
        icy::string text;
        icy::map<icy::string, icy::string> attr;
    };
    struct border_type
    {
        int32_t size = 0;
    };
    struct offset_type
    {
        //float size = 0;
    };
    struct margin_type
    {
        //float size = 0;
    };
    static icy::error_type insert(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const insert_args& args) noexcept;
    static icy::error_type modify(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const insert_args& args) noexcept;
    static bool erase(icy::map<uint32_t, icy::unique_ptr<gui_widget_data>>& map, const uint32_t index) noexcept;
public:
    gui_widget_data(const gui_widget_data* const parent = nullptr, const uint32_t index = 0) noexcept : 
        parent(parent), index(index)
    {
        state = (gui_widget_state_enabled | gui_widget_state_visible);
        display = 0;
        position = 0;
        static_assert(std::is_nothrow_constructible<gui_widget_data>::value, "");
    }
    /*void initialize(am_Solver& solver) noexcept
    {
        for (auto&& ref : size)
        {
            ref.var = am_newvariable(&solver);
            ref.con = am_newconstraint(&solver, AM_WEAK);
            am_addterm(ref.con, ref.var, 1);
            am_setrelation(ref.con, AM_GREATEQUAL);
            am_addconstant(ref.con, 0);
            am_add(ref.con);
        }
        dx.var_min = am_newvariable(&solver);
        dx.var_val = am_newvariable(&solver);
        dx.var_max = am_newvariable(&solver);

        dy.var_min = am_newvariable(&solver);
        dy.var_val = am_newvariable(&solver);
        dy.var_max  = am_newvariable(&solver);

        dx.con_size = am_newconstraint(&solver, AM_MEDIUM);
        dx.con_min = am_newconstraint(&solver, AM_MEDIUM);
        dx.con_val = am_newconstraint(&solver, AM_MEDIUM);
        dx.con_max = am_newconstraint(&solver, AM_MEDIUM);

        dy.con_size = am_newconstraint(&solver, AM_MEDIUM);
        dy.con_min = am_newconstraint(&solver, AM_MEDIUM);
        dy.con_val = am_newconstraint(&solver, AM_MEDIUM);
        dy.con_max = am_newconstraint(&solver, AM_MEDIUM);

        am_suggest(dx.var_min, 0);
        am_suggest(dx.var_val, 0);
        am_suggest(dx.var_max, 0);

        am_suggest(dy.var_min, 0);
        am_suggest(dy.var_val, 0);
        am_suggest(dy.var_max, 0);

        {
            auto con = dx.con_size;
            am_addterm(con, size[0].var, -1);
            am_addterm(con, size[2].var, +1);
            am_setrelation(con, AM_EQUAL);
            am_addterm(con, dx.var_val, 1);
            am_add(con);
        }
        {
            auto con = dy.con_size;
            am_addterm(con, size[1].var, -1);
            am_addterm(con, size[3].var, +1);
            am_setrelation(con, AM_EQUAL);
            am_addterm(con, dy.var_val, 1);
            am_add(con);
        }
        {
            auto con = dx.con_min;
            am_addterm(con, dx.var_val, 1);
            am_setrelation(con, AM_GREATEQUAL);
            am_addterm(con, dx.var_min, 1);
            am_add(con);
        }
        {
            auto con = dx.con_max;
            am_addterm(con, dx.var_val, 1);
            am_setrelation(con, AM_LESSEQUAL);
            am_addterm(con, dx.var_max, 1);
            am_add(con);
        }
        {
            auto con = dy.con_min;
            am_addterm(con, dy.var_val, 1);
            am_setrelation(con, AM_GREATEQUAL);
            am_addterm(con, dy.var_min, 1);
            am_add(con);
        }
        {
            auto con = dy.con_max;
            am_addterm(con, dy.var_val, 1);
            am_setrelation(con, AM_LESSEQUAL);
            am_addterm(con, dy.var_max, 1);
            am_add(con);
        }
    }*/
public:
    const uint32_t index;
    const gui_widget_data* parent;
    mutable icy::array<gui_widget_data*> children;
    icy::string type;
    struct
    {
        uint32_t state      :   0x10;
        uint32_t display    :   0x05;
        uint32_t position   :   0x03;
    };
    gui_widget_text text;
    icy::map<icy::string, icy::gui_variant> attr;
    gui_style_data inline_style;
public:
    float font_size = 0;
    float line_size = 0;
    gui_select_output css;
    border_type borders[4];
    margin_type margins[4];
    offset_type padding[4];
    int32_t content_box[4] = {};
    icy::color bkcolor;
};
struct gui_window_event_type
{
    void* _unused = nullptr;
    enum { none, create, destroy, modify, render, resize } type = none;
    uint32_t index = 0;
    icy::gui_variant val;
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
    icy::error_type initialize() noexcept
    {
        icy::unique_ptr<gui_widget_data> root;
        ICY_ERROR(icy::make_unique(gui_widget_data(), root));
        ICY_ERROR(m_data.insert(0u, std::move(root)));
        return icy::error_type();
    }
private:
    void notify(gui_window_event_type*& event) const noexcept;
    icy::gui_widget child(const icy::gui_widget parent, const size_t offset) noexcept override;
    icy::error_type insert(const icy::gui_widget parent, const size_t offset, const icy::string_view type, icy::gui_widget& widget) noexcept override;
    icy::error_type erase(const icy::gui_widget widget) noexcept override;
    icy::error_type layout(const icy::gui_widget widget, icy::gui_layout& value) noexcept override;
    icy::error_type render(const icy::gui_widget widget, uint32_t& query) const noexcept override;
    icy::error_type resize(const icy::window_size size) noexcept override;
private:
    icy::weak_ptr<icy::gui_system> m_system;
    mutable icy::detail::intrusive_mpsc_queue m_events;
    icy::map<uint32_t, icy::unique_ptr<gui_widget_data>> m_data;
    uint32_t m_index = 1;
};
class gui_system_data;
class gui_window_data_sys
{
public:
    gui_window_data_sys(gui_system_data* system = nullptr) noexcept : m_system(system)
    {

    }
    gui_window_data_sys(const gui_window_data_sys&) = delete;
    gui_window_data_sys(gui_window_data_sys&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(gui_window_data_sys);
    icy::error_type initialize(const icy::shared_ptr<icy::window> window) noexcept;
    icy::error_type input(const icy::input_message& msg) noexcept;
    icy::error_type resize(const icy::window_size size) noexcept;
    icy::error_type process(const gui_window_event_type& event) noexcept;
    void* handle() const noexcept
    {
        return m_window->handle();
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
private:
    icy::error_type update(gui_select_system& css, gui_widget_data& widget) noexcept;
    icy::error_type input_key_press(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_key_hold(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_key_release(const icy::key key, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_move(const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_wheel(const int32_t px, const int32_t py, const icy::key_mod mods, const int32_t wheel) noexcept;
    icy::error_type input_mouse_release(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_press(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_hold(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_mouse_double(const icy::key key, const int32_t px, const int32_t py, const icy::key_mod mods) noexcept;
    icy::error_type input_text(const icy::string_view text) noexcept;
private:
    //struct jmp_type { jmp_buf buf; };
    /*struct solver_type
    {
        solver_type()noexcept = default;
        solver_type(solver_type&& rhs) noexcept : system(rhs.system)
        {
            rhs.system = nullptr;
        }
        ~solver_type() noexcept
        {
            if (system)
                am_delsolver(system);
        }
        am_Solver& operator*() const noexcept
        {
            return *system;
        }
        am_Solver* system = nullptr;
    };*/
    gui_system_data* m_system = nullptr;
    icy::shared_ptr<icy::window> m_window;
    icy::map<uint32_t, icy::unique_ptr<gui_widget_data>> m_data;
    //icy::unique_ptr<jmp_type> m_jmp;
    //solver_type m_solver;
    icy::array<gui_widget_data*> m_list;
    uint32_t m_state = 0;
    gui_font m_font;
    float m_dpi = 96;
    icy::window_size m_size;
};