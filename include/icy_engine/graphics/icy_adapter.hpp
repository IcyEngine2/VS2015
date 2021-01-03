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
    enum class adapter_flags : uint32_t
    {
        none = 0x00,
        hardware = 0x01,
        d3d10 = 0x02,
        d3d11 = 0x04,
        d3d12 = 0x08,
        debug = 0x10,
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

    inline constexpr bool operator&(const adapter_flags lhs, const adapter_flags rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr adapter_flags operator|(const adapter_flags lhs, const adapter_flags rhs) noexcept
    {
        return adapter_flags(uint32_t(lhs) | uint32_t(rhs));
    }

    class adapter
    {
    public:
        static error_type enumerate(const adapter_flags filter, array<adapter>& array) noexcept;
        adapter() noexcept = default;
        adapter(const adapter& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(adapter);
        ~adapter() noexcept;
    public:
        string_view name() const noexcept;
        error_type msaa(render_flags& flags) const noexcept;
        explicit operator bool() const noexcept
        {
            return !!data;
        }
        void* handle() const noexcept;
        size_t vid_mem() const noexcept;
        size_t sys_mem() const noexcept;
        size_t shr_mem() const noexcept;
        uint64_t luid() const noexcept;
        adapter_flags flags() const noexcept;
    public:
        class data_type;
        data_type* data = nullptr;
    };
}