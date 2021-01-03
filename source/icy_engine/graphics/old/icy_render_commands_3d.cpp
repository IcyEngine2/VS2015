#include <d3d11.h>
#include <d3d12.h>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_map.hpp>
#include "icy_render_factory.hpp"
#include "icy_adapter.hpp"
#include "icy_texture.hpp"
#include "icy_render_commands.hpp"

using namespace icy;

class render_factory_data;
class render_commands_d3d11;
class render_commands_d3d12;

render_commands_3d::render_commands_3d(const render_commands_3d& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_commands_3d::~render_commands_3d() noexcept
{
    if (data)
        data->destroy();
}
error_type render_commands_3d::clear(const color color) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    render_command_3d cmd;
    cmd.type = render_command_3d::type::clear;
    cmd.color = color;
    return data->queue.push(std::move(cmd));
}
error_type render_commands_3d::camera(const render_camera& camera) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    render_command_3d cmd;
    cmd.type = render_command_3d::type::camera;
    cmd.camera = camera;
    return data->queue.push(std::move(cmd));
}
error_type render_commands_3d::draw(const render_d2d_rectangle rect, const render_texture texture) noexcept
{
    if (!data || !rect.w || !rect.h || !texture.data)
        return make_stdlib_error(std::errc::invalid_argument);

    render_command_3d cmd;
    cmd.type = render_command_3d::type::draw_texture;
    cmd.texture = texture;
    cmd.rect = rect;
    return data->queue.push(std::move(cmd));
}
error_type render_commands_3d::close(const window_size size, render_texture& texture) noexcept
{
    if (!data || size.x == 0 || size.y == 0)
        return make_stdlib_error(std::errc::invalid_argument);

    auto render = shared_ptr<render_factory_data>(data->factory);
    if (!render)
        return make_stdlib_error(std::errc::invalid_argument);

    if (!data->data)
    {
        const auto flags = render->adapter();
        if (flags & adapter_flags::d3d12)
        {
            ICY_ERROR(render_commands_3d_data::create_d3d12(data->data, *render));
        }
        else if (flags & (adapter_flags::d3d11 | adapter_flags::d3d10))
        {
            ICY_ERROR(render_commands_3d_data::create_d3d11(data->data, *render));
        }
        else
        {
            return make_stdlib_error(std::errc::invalid_argument);
        }
    }
    return data->data->exec(*data, size, texture);
}

render_commands_3d::data_type::~data_type() noexcept
{
    allocator_type::destroy(data);
    allocator_type::deallocate(data);
}
void render_commands_3d::data_type::destroy() noexcept
{
    if (ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (auto render = shared_ptr<render_factory_data>(factory))
        {
            render->destroy(*this);
        }
        else
        {
            allocator_type::destroy(this);
            allocator_type::deallocate(this);
        }
    }
}

error_type render_factory_data::open_commands_3d(render_commands_3d& commands) noexcept
{
    if (!commands.data)
    {
        commands.data = static_cast<render_commands_3d::data_type*>(m_cmd_3d.pop());
        if (commands.data)
            commands.data->ref.fetch_add(1, std::memory_order_release);
    }
    if (!commands.data)
    {
        commands.data = allocator_type::allocate<render_commands_3d::data_type>(1);
        if (!commands.data)
            return make_stdlib_error(std::errc::invalid_argument);
        allocator_type::construct(commands.data);
        commands.data->factory = make_shared_from_this(this);
    }
    return error_type();
}

