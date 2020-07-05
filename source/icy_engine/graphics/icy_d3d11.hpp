#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/graphics/icy_render_svg.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include "icy_graphics.hpp"

struct IDXGIFactory;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11CommandList;
struct ID3D11VertexShader;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11RasterizerState;

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
    class d3d11_render_svg
    {
    public:
        error_type initialize(ID3D11Device& device) noexcept;
        void operator()(ID3D11DeviceContext& context) const noexcept;
    private:
        com_ptr<ID3D11VertexShader> m_vshader;
        com_ptr<ID3D11InputLayout> m_layout;
        com_ptr<ID3D11PixelShader> m_pshader;
        com_ptr<ID3D11RasterizerState> m_rasterizer;
    };
    struct d3d11_render
    {
        d3d11_render_svg svg;
    };

    class d3d11_back_buffer
    {
    public:
        error_type initialize(ID3D11Device& device) noexcept;
        error_type resize(const window_size size, const window_flags flags) noexcept;
        error_type update(const render_list& list, const d3d11_render& render) noexcept;
        error_type draw(ID3D11RenderTargetView& rtv) noexcept;
    private:
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<ID3D11CommandList> m_commands;
        window_size m_size;
        com_ptr<ID3D11Texture2D> m_texture;
        com_ptr<ID3D11RenderTargetView> m_view;
        com_ptr<ID3D11Buffer> m_svg_vertices;
        com_ptr<ID3D11Buffer> m_svg_buffer;
    };

    class d3d11_display : public display
    {
    public:
        ~d3d11_display() noexcept;
        error_type initialize(const adapter& adapter, const window_flags flags) noexcept;
        error_type bind(HWND__* const window) noexcept;
        error_type draw(const size_t frame, const bool vsync) noexcept override;
        error_type resize() noexcept override;
        void* event() noexcept override
        {
            return nullptr;
        }
        error_type update(const render_list& list) noexcept override;
    private:
        adapter m_adapter;
        window_flags m_flags = window_flags::none;
        library m_d3d11_lib = "d3d11"_lib;
        com_ptr<ID3D11Device> m_device;
        d3d11_swap_chain m_chain;
        array<d3d11_back_buffer> m_buffers;
        d3d11_render m_render;
        size_t m_frame = 0;
    };
}