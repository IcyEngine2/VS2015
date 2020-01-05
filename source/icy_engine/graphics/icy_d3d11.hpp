#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/graphics/icy_graphics.hpp>
#include <icy_engine/utility/icy_com.hpp>

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11CommandList;

namespace icy
{
    class d3d11_swap_chain
    {
    public:
        error_type initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const display_flag flags) noexcept;
        error_type resize() noexcept;
        error_type window(HWND__*& hwnd) const noexcept;
        error_type present(const uint32_t vsync = 1) noexcept;
        ID3D11RenderTargetView& view() const noexcept
        {
            return *m_view;
        }
        error_type size(window_size& output) const noexcept;
    private:
        com_ptr<IDXGISwapChain> m_chain;
        com_ptr<ID3D11RenderTargetView> m_view;
    };
    class d3d11_back_buffer
    {
    public:
        error_type initialize(ID3D11Device& device) noexcept;
        ID3D11RenderTargetView& view() const noexcept
        {
            return *m_view;
        }
        error_type resize(const window_size size, const display_flag flags) noexcept;
    private:
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<ID3D11CommandList> m_commands;
        com_ptr<ID3D11RenderTargetView> m_view;
    };
    class d3d11_display : public display
    {
    public:
        ~d3d11_display() noexcept override
        {
            filter(0);
        }
        error_type initialize(const adapter& adapter, const display_flag flags) noexcept;
        error_type bind(HWND__* const window) noexcept override;
        error_type loop(const duration_type timeout) noexcept override;
        error_type signal(const event_data& event) noexcept override;
        uint64_t frame() const noexcept
        {
            return m_frame.load(std::memory_order_acquire);
        }
        ID3D11Device& device() const noexcept
        {
            return *m_device;
        }
        display_flag flags() const noexcept override
        {
            return m_flags;
        }
    private:
        adapter m_adapter;
        display_flag m_flags = display_flag::none;
        library m_d3d11_lib = "d3d11"_lib;
        com_ptr<ID3D11Device> m_device;
        std::atomic<uint64_t> m_frame = 0;
        d3d11_swap_chain m_chain;
        array<d3d11_back_buffer> m_buffers;
    };
}