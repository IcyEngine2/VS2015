#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/utility/icy_com.hpp>

struct IDXGIAdapter;
struct ID3D11CommandList;
struct ID3D11Texture2D;

class icy::adapter::data_type
{
public:
    error_type initialize(const icy::com_ptr<IDXGIAdapter> adapter, const adapter_flag flags) noexcept;
    IDXGIAdapter& adapter() const noexcept
    {
        return *m_adapter;
    }
    string_view name() const noexcept
    {
        return m_name;
    }
    adapter_flag flags() const noexcept
    {
        return m_flags;
    }
    error_type msaa_d3d11(array<uint32_t>& array) noexcept;
    error_type msaa_d3d12(array<uint32_t>& array) noexcept;
    std::atomic<uint32_t> ref = 1;
private:
    library m_lib_dxgi = "dxgi"_lib;
    library m_lib_debug = "dxgidebug"_lib;
    com_ptr<IDXGIAdapter> m_adapter;
    adapter_flag m_flags = adapter_flag::none;
    string m_name;
};

namespace icy
{
    class render_system;
    struct render_frame
    {
        size_t index = 0;
        HWND__* window = nullptr;
        struct
        {
            com_ptr<ID3D11CommandList> commands;
            com_ptr<ID3D11Texture2D> texture;
        } d3d11;
    };

    class display
    {
    public:
        virtual ~display() noexcept = default;
        virtual error_type draw(const bool vsync) noexcept = 0;
        virtual error_type resize() noexcept = 0;
        virtual error_type create(shared_ptr<render_system>& render) const noexcept = 0;
        virtual error_type update(const render_frame& frame) noexcept;
        virtual void* event() const noexcept = 0;
    };
    error_type make_d3d11_display(unique_ptr<display>& display, const adapter& adapter, const window_flags flags, HWND__* const window) noexcept;
    error_type make_d3d12_display(unique_ptr<display>& display, const adapter& adapter, const window_flags flags, HWND__* const window) noexcept;
}