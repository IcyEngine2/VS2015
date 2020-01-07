#pragma once

#include <icy_engine/core/icy_event.hpp>

struct HWND__;

namespace icy
{
    enum class adapter_flag : uint32_t
    {
        none = 0x00,
        hardware = 0x01,
        d3d11 = 0x02,
        d3d12 = 0x04,
    };
    enum class display_flag : uint32_t
    {
        none = 0x00,
        debug_layer = 0x01,
        triple_buffer = 0x02,
        sRGB = 0x04,
        msaa_x2 = 0x08,
        msaa_x4 = 0x10,
        msaa_x8 = 0x20,
        msaa_x16 = 0x40,
    };
    static constexpr auto display_default =
#if _DEBUG
        display_flag::debug_layer;
#else
        display_flag::none;
#endif

    inline constexpr bool operator&(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr adapter_flag operator|(const adapter_flag lhs, const adapter_flag rhs) noexcept
    {
        return adapter_flag(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr bool operator&(const display_flag lhs, const display_flag rhs) noexcept
    {
        return !!(uint32_t(lhs) & uint32_t(rhs));
    }
    inline constexpr display_flag operator|(const display_flag lhs, const display_flag rhs) noexcept
    {
        return display_flag(uint32_t(lhs) | uint32_t(rhs));
    }

    class adapter
    {
    public:
        static error_type enumerate(const adapter_flag filter, array<adapter>& array) noexcept;
        adapter() noexcept = default;
        adapter(const adapter& rhs) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(adapter);
        ~adapter() noexcept;
    public:
        string_view name() const noexcept;
        explicit operator bool() const noexcept
        {
            return !!data;
        }
        void* handle() const noexcept;
        size_t vid_mem = 0;
        size_t sys_mem = 0;
        size_t shr_mem = 0;
        uint64_t luid = 0;
        adapter_flag flags = adapter_flag::none;
    public:
        class data_type;
        data_type* data = nullptr;
    };

    class window;
    class display
    {
    public:
        virtual ~display() noexcept = 0 { };
        static error_type create(shared_ptr<display>& display, const adapter& adapter, const display_flag display_flags = display_default) noexcept;
        virtual error_type bind(HWND__* const window) noexcept = 0;
        virtual error_type bind(window& window) noexcept = 0;
        virtual error_type draw() noexcept = 0;
        virtual size_t frame() const noexcept = 0;
        virtual display_flag flags() const noexcept = 0;
    };


    error_type adapter_msaa(adapter& adapter, display_flag& quality) noexcept;

}