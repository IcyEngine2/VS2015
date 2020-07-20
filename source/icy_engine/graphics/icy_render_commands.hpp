#pragma once

#include <icy_engine/core/icy_queue.hpp>
#include "icy_render_factory.hpp"

struct IDWriteTextLayout;
struct ID3D11Device;
struct ID2D1Device;
struct ID2D1DeviceContext;

struct render_command_2d
{
    icy::render_d2d_matrix transform;
    float layer = 0;
    icy::render_svg_geometry geometry;
    icy::color color;
    icy::com_ptr<IDWriteTextLayout> text;
};

class icy::render_commands_2d::data_type
{
public:
    icy::error_type exec(const icy::window_size size, icy::render_texture& output) noexcept;
    void destroy() noexcept;
public:
    void* _unused = nullptr;
    std::atomic<uint32_t> ref = 1;
    icy::weak_ptr<render_factory_data> factory;
    icy::mpsc_queue<render_command_2d> queue;
    icy::com_ptr<ID3D11Device> d3d11;
    icy::com_ptr<ID2D1Device> device;
    icy::com_ptr<ID2D1DeviceContext> context;
};


struct render_command_3d
{
    enum class type : uint32_t
    {
        none,
        clear,
        camera,
        draw_texture,
    } type = type::none;
    icy::color color;
    icy::render_camera camera;
    icy::render_d2d_rectangle rect;
    icy::render_texture texture;
};
class render_commands_3d_data
{
public:
    static icy::error_type create_d3d11(render_commands_3d_data*& ptr, render_factory_data& factory) noexcept;
    static icy::error_type create_d3d12(render_commands_3d_data*& ptr, render_factory_data& factory) noexcept
    {
        return icy::make_stdlib_error(std::errc::function_not_supported);
    }
public:
    virtual ~render_commands_3d_data() noexcept = 0 { }
    virtual icy::error_type exec(icy::render_commands_3d::data_type& cmd, const icy::window_size size, icy::render_texture& output) noexcept = 0;
};

class icy::render_commands_3d::data_type
{
public:
    ~data_type() noexcept;
    void destroy() noexcept;
public:
    void* _unused = nullptr;
    std::atomic<uint32_t> ref = 1;
    icy::weak_ptr<render_factory_data> factory;
    icy::mpsc_queue<render_command_3d> queue;
    render_commands_3d_data* data = nullptr;
};