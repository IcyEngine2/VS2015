#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_set.hpp>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/network/icy_network.hpp>
#include <icy_engine/utility/icy_imgui.hpp>
#include <icy_engine/utility/icy_crypto.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_chat/icy_chat.hpp>
#include <array>

struct chat_server_application;

struct chat_server_gui_thread : public icy::thread
{
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
    chat_server_application* app = nullptr;
};
struct chat_server_network_thread : public icy::thread
{
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
    chat_server_application* app = nullptr;
}; 

struct chat_entry
{
    icy::chat_time time = {};
    uint32_t size = 0;
    uint32_t _padding = 0;
    icy::guid user;
    icy::guid room;
};/*
struct chat_action
{
    icy::chat_request_type type = icy::chat_request_type::none;
    icy::chat_time time = {};
    icy::guid user;
    icy::guid room;
    icy::string text;
};
struct chat_user
{
    icy::array<chat_entry> data;
    uint32_t size = 0;
    icy::mpsc_queue<chat_action> actions;
};
struct chat_room
{
    icy::set<icy::guid> users;
    icy::mpsc_queue<chat_action> actions;
};*/
class chat_database
{
public:
    icy::error_type initialize(const icy::string_view path, const size_t capacity) noexcept;
    icy::error_type exec(const icy::chat_request& request, icy::chat_response& response) noexcept;
    icy::crypto_key password = icy::crypto_random;
private:
    icy::sync_handle m_sync;
    icy::database_system_write m_base;
    icy::database_dbi m_dbi_users;
    icy::database_dbi m_dbi_rooms;
};

struct chat_server_application
{
public:
    ~chat_server_application() noexcept
    {
        if (network_thread) network_thread->wait();
        if (gui_thread) gui_thread->wait();
        if (http_thread) http_thread->wait();
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
    icy::shared_ptr<chat_server_gui_thread> gui_thread;
    icy::shared_ptr<chat_server_network_thread> network_thread;
    icy::shared_ptr<icy::event_thread> http_thread;
    icy::shared_ptr<icy::network_system_http_server> http_server;
    std::array<uint32_t, 24> widgets = {};
    chat_database database;
};