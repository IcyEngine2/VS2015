#include "icy_render.hpp"
#include "icy_system.hpp"
#include "icy_render_svg_core.hpp"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11_4.h>
#include <d3d10_1.h>

error_type render_list_data::clear(const color color) noexcept
{
    render_element elem(render_element_type::clear);
    elem.clear = color;
    return queue.push(std::move(elem));
}
error_type render_list_data::draw(const render_svg_geometry geometry, const render_d2d_matrix& transform, const float layer) noexcept
{
    const auto& vertices = geometry.vertices();
    if (vertices.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    render_element elem(render_element_type::svg);
    ICY_ERROR(elem.svg.resize(vertices.size()));
    for (auto k = 0_z; k < vertices.size(); ++k)
    {
        auto vec = transform * render_d2d_vector(vertices[k].coord.x, vertices[k].coord.y);
        auto& coord = elem.svg[k].coord;
        coord.x = vec.x;
        coord.y = vec.y;
        coord.z = vertices[k].coord.z + layer;
        elem.svg[k].color = vertices[k].color;
    }
    return queue.push(std::move(elem));
}

render_frame::render_frame(const render_frame& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_frame::~render_frame() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (data->d3d11) destroy_frame(*data->d3d11);
        if (data->d3d12) destroy_frame(*data->d3d12);
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}
size_t render_frame::index() const noexcept
{
    return data ? data->index : 0;
}
duration_type render_frame::time_cpu() const noexcept
{
    return data ? data->time_cpu : duration_type();
}
duration_type render_frame::time_gpu() const noexcept
{
    return data ? data->time_gpu : duration_type();
}
window_size render_frame::size() const noexcept
{
    return data ? data->size : window_size();
}

error_type icy::create_render_list(shared_ptr<render_list>& list) noexcept
{
    shared_ptr<render_list_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    list = new_ptr;
    return error_type();
}

error_type icy::create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    if (!adapter.data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (adapter.data->flags() & adapter_flags::d3d12)
    {
        return create_d3d12(system, adapter, flags);
    }
    else if (adapter.data->flags() & (adapter_flags::d3d11 | adapter_flags::d3d10))
    {
        return create_d3d11(system, adapter, flags);
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
}
error_type icy::create_event_system(shared_ptr<display_system>& display, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    if (!adapter.data || !hwnd)
        return make_stdlib_error(std::errc::invalid_argument);

    if (adapter.data->flags() & adapter_flags::d3d12)
    {
        return create_d3d12(display, adapter, hwnd, flags);
    }
    else if (adapter.data->flags() & (adapter_flags::d3d11 | adapter_flags::d3d10))
    {
        return create_d3d11(display, adapter, hwnd, flags);
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);        
    }
}