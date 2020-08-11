#pragma once

#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/graphics/icy_window.hpp>

struct render_command_2d;
struct ID2D1Device;

namespace icy
{
    class render_factory;
}

struct window_repaint_type
{
    icy::weak_ptr<icy::render_factory> render;
    icy::map<float, icy::array<render_command_2d>> map;
    ID2D1Device* device = nullptr;
};

class window_system_data;

class icy::window::data_type
{
public:
    icy::error_type repaint(window_repaint_type&& repaint) noexcept;
public:
    std::atomic<uint32_t> ref = 1;
    uint32_t index = 0;
    icy::weak_ptr<window_system_data> system;
};