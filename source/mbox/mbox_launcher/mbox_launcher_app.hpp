#pragma once

#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_remote_window.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_qtgui/icy_xqtgui.hpp>
#include "mbox_launcher_config.hpp"
#include "../mbox_script_system.hpp"

class mbox_launcher_app : public icy::thread
{
public:
    icy::error_type initialize() noexcept;
private:
    struct menu_type
    {
        icy::error_type initialize(icy::gui_widget window) noexcept;
        icy::xgui_widget menu;
        icy::xgui_action config;
    };
    struct library_type
    {
        icy::error_type initialize(icy::gui_widget window) noexcept;
        icy::xgui_widget label;
        icy::xgui_widget view;
        icy::xgui_widget button;
        mbox::mbox_array data;
    };
    struct party_type
    {
        icy::error_type initialize(icy::gui_widget window) noexcept;
        icy::xgui_model model;
        icy::xgui_widget label;
        icy::xgui_widget combo;
        icy::xgui_widget button;
    };
    struct games_type
    {
        icy::error_type initialize(icy::gui_widget window) noexcept;
        icy::xgui_model model;
        icy::xgui_widget label;
        icy::xgui_widget combo;
    };
    struct character_type
    {
        mbox::mbox_index index;
        uint32_t slot = 0;
        icy::string name;
        bool paused = false;
        icy::remote_window window;
    };
    struct data_type
    {
        icy::error_type initialize(icy::gui_widget window) noexcept;
        icy::library lib =
#if _DEBUG
            icy::library("mbox_dlld");
#else
            icy::library("mbox_dll");
#endif
        icy::shared_ptr<mbox::mbox_system> system;
        icy::shared_ptr<icy::remote_window_system> remote;
        icy::array<character_type> characters;
        icy::xgui_model model;
        icy::xgui_widget view;
    };
private:
    icy::error_type run() noexcept override;
    icy::error_type reset_library(const icy::string_view name) noexcept;
    icy::error_type reset_games(const icy::string_view select) noexcept;
    icy::error_type reset_data() noexcept;
    icy::error_type on_launch() noexcept;
    icy::error_type on_stop() noexcept;
    icy::error_type on_context(character_type& chr) noexcept;
    icy::error_type on_select_party(const mbox::mbox_index select) noexcept;
    icy::error_type find_window(icy::remote_window& new_window) noexcept;
private:
    icy::shared_ptr<icy::gui_queue> m_gui;
    mbox_config_type m_config;
    icy::shared_ptr<icy::window_system> m_local;
    icy::window m_overlay;
    icy::xgui_widget m_window;
    menu_type m_menu;
    library_type m_library;
    party_type m_party;
    games_type m_games;
    data_type m_data;
};