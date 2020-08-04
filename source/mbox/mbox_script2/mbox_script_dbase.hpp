#pragma once

#include <icy_engine/utility/icy_database.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "../mbox_script_system.hpp"

namespace mbox
{
    class mbox_database : public mbox_array
    {
    public:
        mbox_database(const icy::gui_widget window) noexcept : m_window(window)
        {

        }
        mbox_database(const mbox_database&) = delete;
        icy::error_type initialize() noexcept;
        icy::error_type exec(const icy::event_type type, const icy::gui_event& event) noexcept;
        icy::error_type close(bool& cancel) noexcept;
    private:
        enum class open_mode
        {
            open,
            create,
        };
        using validate_func = icy::error_type(*)(void* user, const mbox_object& object, icy::gui_node node, bool& success);
        struct menu_type
        {
            icy::xgui_widget menu_main;
            icy::xgui_submenu menu_file;
            icy::xgui_submenu menu_edit;
            icy::xgui_action action_create;
            icy::xgui_action action_open;
            icy::xgui_action action_save;
            icy::xgui_action action_close;
            icy::xgui_action action_undo;
            icy::xgui_action action_redo;
        };
        enum class tool_type
        {
            none,
            save,
            undo,
            redo,
            compile,
        };
        struct window_type
        {
            window_type(mbox_database* self = nullptr) noexcept : self(*self)
            {

            }
            window_type(window_type&&) noexcept = default;
            ICY_DEFAULT_MOVE_ASSIGN(window_type);

            icy::error_type initialize(const mbox_object& object) noexcept;
            icy::error_type remodel() noexcept;
            icy::error_type on_edit(const icy::string_view key) noexcept;
            icy::error_type context(const size_t node) noexcept;
            icy::error_type update() noexcept;
            icy::error_type save() noexcept;
            icy::error_type undo() noexcept;
            icy::error_type redo() noexcept;
            icy::error_type push_refs(mbox_object&& actions) noexcept;
            icy::error_type execute(mbox_object& action) noexcept;
            icy::error_type close(bool& cancel) noexcept;
            icy::error_type compile() noexcept;
            icy::error_type on_focus(const icy::gui_widget new_focus) noexcept;

            mbox_database& self;
            mbox_object object;
            icy::array<mbox_object> refs_actions;
            size_t refs_action = 0;

            icy::xgui_model refs_model;
            icy::xgui_widget window;
            icy::xgui_widget toolbar;
            icy::xgui_widget tabs;
            icy::xgui_text_edit tab_text;
            icy::xgui_widget tab_refs;
            icy::xgui_text_edit tab_debug;
            icy::gui_widget focus;

            icy::array<std::pair<tool_type, icy::xgui_widget>> tools;            
        };
    private:
        icy::error_type reset(const icy::string_view file_path, open_mode mode) noexcept;
        icy::error_type launch(const mbox_index& party) noexcept;
        icy::error_type on_move(mbox_object& object) noexcept;
        icy::error_type on_delete(mbox_object& object) noexcept;
        icy::error_type on_edit(const mbox_object& object, const bool always_open) noexcept;
        icy::error_type context(const mbox_index& guid) noexcept;
        icy::error_type update() noexcept;
        icy::error_type save() noexcept;
        icy::error_type undo() noexcept;
        icy::error_type redo() noexcept;
        icy::error_type push(icy::array<mbox_object>&& actions) noexcept;
        icy::error_type execute(mbox_object& action) noexcept;        
    private:
        const icy::gui_widget m_window;
        icy::map<mbox_type, icy::xgui_image> m_icons;
        icy::xgui_model m_model;
        icy::xgui_widget m_tree;
        menu_type m_menu;
        icy::string m_path;
        icy::array<icy::array<mbox_object>> m_actions;
        size_t m_action = 0;
        icy::map<mbox_index, window_type> m_windows;
    };
}