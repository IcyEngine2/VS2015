#pragma once

#include <icy_engine/core/icy_array.hpp>
#include "icy_graphics.hpp"

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
        error_type initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const window_flags flags) noexcept;
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
        error_type resize(const window_size size, const window_flags flags) noexcept;
    private:
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<ID3D11CommandList> m_commands;
        com_ptr<ID3D11RenderTargetView> m_view;
    };
    class d3d11_display : public display
    {
    public:
        ~d3d11_display() noexcept;
        error_type initialize(const adapter& adapter, const window_flags flags) noexcept;
        error_type bind(HWND__* const window) noexcept;
        error_type draw(const size_t frame) noexcept override;
        ID3D11Device& device() const noexcept
        {
            return *m_device;
        }
        void* event() noexcept override
        {
            return nullptr;
        }
    private:
        adapter m_adapter;
        window_flags m_flags = window_flags::none;
        library m_d3d11_lib = "d3d11"_lib;
        com_ptr<ID3D11Device> m_device;
        d3d11_swap_chain m_chain;
        array<d3d11_back_buffer> m_buffers;
    };
}