#include "icy_dcl_gui.hpp"

using namespace icy;

class dcl_gui_directory : public dcl_widget
{
public:
    dcl_gui_directory(dcl_application& app) noexcept : dcl_widget(app, icy::dcl_type::directory)
    {

    }
private:
    icy::error_type initialize(const icy::dcl_base& base) noexcept override;
    icy::error_type exec(const icy::event event) noexcept override;
private:
    struct action_type 
    {
        const dcl_gui_text* menu = nullptr;
        const dcl_gui_text* text = nullptr;
        icy::xgui_action action;
    };
    struct menu_type
    {
        icy::xgui_widget widget;
        icy::xgui_submenu base;
        icy::xgui_submenu patch;
        icy::xgui_submenu edit;
        icy::array<action_type> actions;
    };
    struct tools_type
    {
        icy::xgui_widget widget;
        icy::xgui_widget prev;
        icy::xgui_widget next;
        icy::xgui_widget list;
        icy::xgui_widget top;
        icy::xgui_widget path;
        icy::xgui_widget find;
    };
    struct navg_type
    {
        icy::gui_node root;
        icy::xgui_widget tree;
    };
    struct view_type
    {
        icy::xgui_model model;
        icy::xgui_widget grid;
    };
private:
    menu_type m_menu;
    tools_type m_tools;
    icy::xgui_widget m_main;
    navg_type m_navg;
    view_type m_view;
};

error_type dcl_gui_directory::initialize(const dcl_base& base) noexcept
{
    ICY_ERROR(m_window.initialize(gui_widget_type::window, {}, gui_widget_flag::layout_vbox));
    
    ICY_ERROR(m_menu.widget.initialize(gui_widget_type::menubar, m_window));
    ICY_ERROR(m_menu.base.initialize(dcl_gui_text::Database));
    ICY_ERROR(m_menu.patch.initialize(dcl_gui_text::Patch));
    ICY_ERROR(m_menu.edit.initialize(dcl_gui_text::Edit));
    ICY_ERROR(m_menu.widget.insert(m_menu.base.action));
    ICY_ERROR(m_menu.widget.insert(m_menu.patch.action));
    ICY_ERROR(m_menu.widget.insert(m_menu.edit.action));

    const auto func = [this](const dcl_gui_text& menu, const dcl_gui_text& text)
    {
        action_type action;
        action.text = &text;
        action.menu = &menu;
        ICY_ERROR(action.action.initialize(text));

        xgui_submenu* sub = nullptr;
        if (&menu == &dcl_gui_text::Database)
            sub = &m_menu.base;
        else if (&menu == &dcl_gui_text::Patch)
            sub = &m_menu.patch;
        else if (&menu == &dcl_gui_text::Edit)
            sub = &m_menu.edit;

        if (sub)
            ICY_ERROR(sub->widget.insert(action.action));

        ICY_ERROR(m_menu.actions.push_back(std::move(action)));
        return error_type();
    };
    ICY_ERROR(func(dcl_gui_text::Database, dcl_gui_text::New));
    ICY_ERROR(func(dcl_gui_text::Database, dcl_gui_text::Open));
    ICY_ERROR(func(dcl_gui_text::Database, dcl_gui_text::Close));
    ICY_ERROR(func(dcl_gui_text::Database, dcl_gui_text::SaveAs));

    ICY_ERROR(func(dcl_gui_text::Patch, dcl_gui_text::New));
    ICY_ERROR(func(dcl_gui_text::Patch, dcl_gui_text::Open));
    ICY_ERROR(func(dcl_gui_text::Patch, dcl_gui_text::Close));
    ICY_ERROR(func(dcl_gui_text::Patch, dcl_gui_text::SaveAs));
    ICY_ERROR(func(dcl_gui_text::Patch, dcl_gui_text::Apply));

    ICY_ERROR(func(dcl_gui_text::Edit, dcl_gui_text::Undo));
    ICY_ERROR(func(dcl_gui_text::Edit, dcl_gui_text::Redo));
    ICY_ERROR(func(dcl_gui_text::Edit, dcl_gui_text::Actions));

    ICY_ERROR(m_tools.widget.initialize(gui_widget_type::none, m_window, gui_widget_flag::layout_hbox | gui_widget_flag::auto_insert));
    ICY_ERROR(m_tools.prev.initialize(gui_widget_type::tool_button, m_tools.widget));
    ICY_ERROR(m_tools.next.initialize(gui_widget_type::tool_button, m_tools.widget));
    ICY_ERROR(m_tools.list.initialize(gui_widget_type::tool_button, m_tools.widget));
    ICY_ERROR(m_tools.top.initialize(gui_widget_type::tool_button, m_tools.widget));
    ICY_ERROR(m_tools.path.initialize(gui_widget_type::line_edit, m_tools.widget));
    ICY_ERROR(m_tools.find.initialize(gui_widget_type::line_edit, m_tools.widget));

    ICY_ERROR(m_main.initialize(gui_widget_type::hsplitter, m_window));

    ICY_ERROR(m_navg.tree.initialize(gui_widget_type::tree_view, m_main));
    ICY_ERROR(m_navg.tree.bind(m_app.root()));
    
    ICY_ERROR(m_view.model.initialize());
    ICY_ERROR(m_view.grid.initialize(gui_widget_type::grid_view, m_main));
    ICY_ERROR(m_view.grid.bind(m_view.model));
    
    ICY_ERROR(m_window.show(true));

    m_base = base;

    return {};
}
error_type dcl_gui_directory::exec(const event event) noexcept
{
    if (event->type == event_type::gui_action)
    {
        const auto& event_data = event->data<gui_event>();
        gui_action action;
        action.index = event_data.data.as_uinteger();
        
        for (auto&& tuple : m_menu.actions)
        {
            if (tuple.action == action)
            {
                if (const auto error = m_app.menu(*tuple.menu, *tuple.text))
                {
                    string str;
                    ICY_ERROR(str.appendf("%1.%2"_s, string_view(*tuple.menu), string_view(*tuple.text)));
                    ICY_ERROR(show_error(error, str));
                }
                break;
            }
        }
    }   
    return {};
}
static const auto create_widget = dcl_widget::factory(dcl_type::directory, [](dcl_application& app, 
    icy::unique_ptr<dcl_widget>& widget){ widget = make_unique<dcl_gui_directory>(app); });