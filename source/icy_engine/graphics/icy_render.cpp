#include "icy_directx.hpp"
#include "icy_vulkan.hpp"
#include <icy_engine/graphics/icy_render.hpp>

using namespace icy;

error_type icy::create_gpu_list(const gpu_flags filter, array<shared_ptr<gpu_device>>& list) noexcept
{
    if (filter & (gpu_flags::d3d10 | gpu_flags::d3d11 | gpu_flags::d3d12))
    {
        shared_ptr<directx_system> dx;
        ICY_ERROR(make_shared(dx, filter));
        ICY_ERROR(dx->initialize());
        ICY_ERROR(dx->create_gpu_list(list));
    }
    if (filter & gpu_flags::vulkan)
    {
        shared_ptr<vulkan_system> vk;
        ICY_ERROR(make_shared(vk, filter));
        ICY_ERROR(vk->initialize());
        ICY_ERROR(vk->create_gpu_list(list));
    }
    std::sort(list.begin(), list.end(), [](const shared_ptr<gpu_device>& lhs, const shared_ptr<gpu_device>& rhs)
        { return lhs->vid_mem() < rhs->vid_mem(); });
    std::reverse(list.begin(), list.end());
    return error_type();
}

error_type icy::create_display_system(shared_ptr<display_system>& system, const shared_ptr<window> window, const shared_ptr<gpu_device> gpu) noexcept
{
    if (!gpu || !window)
        return make_stdlib_error(std::errc::invalid_argument);

    if (gpu->flags() & gpu_flags::vulkan)
    {
        shared_ptr<vulkan_display_system> ptr;
        ICY_ERROR(make_shared(ptr, gpu, window));
        ICY_ERROR(ptr->initialize());
        system = std::move(ptr);
        return error_type();
    }
    else if (gpu->flags() & (gpu_flags::d3d10 | gpu_flags::d3d11))
    {
        shared_ptr<directx_display_system> ptr;
        ICY_ERROR(make_shared(ptr, gpu, window));
        ICY_ERROR(ptr->initialize());
        system = std::move(ptr);
        return error_type();
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }    
}

error_type icy::create_render_system(shared_ptr<render_system>& system, const shared_ptr<gpu_device> gpu) noexcept
{
    if (!gpu)
        return make_stdlib_error(std::errc::invalid_argument);

    if (gpu->flags() & gpu_flags::vulkan)
    {

    }
    else if (gpu->flags() & (gpu_flags::d3d10 | gpu_flags::d3d11))
    {
        shared_ptr<directx_render_system> ptr;
        ICY_ERROR(make_shared(ptr, gpu));
        ICY_ERROR(ptr->initialize());
        system = std::move(ptr);
        return error_type();
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
