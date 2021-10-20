#pragma once

#include <icy_engine/core/icy_array.hpp>

namespace icy
{
    enum class render_flags : uint32_t
    {
        none = 0x00,
        msaa_x2 = 0x01,
        msaa_x4 = 0x02,
        msaa_x8 = 0x04,
        msaa_x16 = 0x08,
        msaa_hardware = 0x10,
        sRGB = 0x20,
    };
    enum class gpu_flags : uint32_t
    {
        none = 0x00,
        hardware = 0x01,
        d3d10 = 0x02,
        d3d11 = 0x04,
        d3d12 = 0x08,
        vulkan = 0x10,
        debug = 0x20,
    };

    inline constexpr bool operator&(const render_flags lhs, const render_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr render_flags operator|(const render_flags lhs, const render_flags rhs) noexcept
    {
        return render_flags(uint32_t(lhs) | uint32_t(rhs));
    }
    inline uint32_t sample_count(const render_flags flag) noexcept
    {
        if (flag & render_flags::msaa_x16)
            return 16;
        else if (flag & render_flags::msaa_x8)
            return 8;
        else if (flag & render_flags::msaa_x4)
            return 4;
        else if (flag & render_flags::msaa_x2)
            return 2;
        else
            return 1;
    }

    inline constexpr bool operator&(const gpu_flags lhs, const gpu_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr gpu_flags operator|(const gpu_flags lhs, const gpu_flags rhs) noexcept
    {
        return gpu_flags(uint32_t(lhs) | uint32_t(rhs));
    }

    struct gpu_device
    {
        ~gpu_device() noexcept
        {

        }
        virtual string_view name() const noexcept = 0;
        virtual error_type msaa(render_flags& flags) const noexcept = 0;
        virtual void* handle() const noexcept = 0;
        virtual size_t vid_mem() const noexcept = 0;
        virtual size_t sys_mem() const noexcept = 0;
        virtual size_t shr_mem() const noexcept = 0;
        virtual uint64_t luid() const noexcept = 0;
        virtual gpu_flags flags() const noexcept = 0;
    };
    error_type create_gpu_list(const gpu_flags filter, array<shared_ptr<gpu_device>>& array) noexcept;
}