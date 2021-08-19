#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/utility/icy_imgui.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_chat/icy_chat.hpp>
#include <array>

struct chat_client_application;

struct chat_client_gui_thread : public icy::thread
{
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
    chat_client_application* app = nullptr;
};
struct chat_client_network_thread : public icy::thread
{
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
    chat_client_application* app = nullptr;
};

struct chat_client_application
{
public:
    ~chat_client_application() noexcept
    {
        if (gui_thread) gui_thread->wait();
    }
    icy::error_type initialize() noexcept;
    icy::error_type run() noexcept;
    icy::error_type run_gui() noexcept;
    icy::error_type run_network() noexcept;
public:
    icy::shared_ptr<icy::window_system> window_system;
    icy::shared_ptr<icy::window> window;
    icy::shared_ptr<icy::imgui_system> imgui_system;
    icy::shared_ptr<icy::imgui_display> imgui_display;
    icy::shared_ptr<icy::display_system> display_system;
    icy::adapter gpu;
    icy::shared_ptr<icy::event_queue> loop;
    icy::shared_ptr<chat_client_gui_thread> gui_thread;
    icy::shared_ptr<icy::network_system_http_client> http_client;
    std::array<uint32_t, 26> widgets = {};
    icy::set<icy::guid> users;
    icy::set<icy::guid> rooms;
};