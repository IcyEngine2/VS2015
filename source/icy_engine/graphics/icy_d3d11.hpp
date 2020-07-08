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
struct HWND__;

namespace icy
{
    struct render_frame;

    class d3d11_swap_chain
    {
    public:
        error_type initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const window_flags flags) noexcept;
        error_type resize() noexcept;
        error_type window(HWND__*& hwnd) const noexcept;
        error_type size(window_size& output) const noexcept;
        error_type update(const render_frame& frame, const bool vsync) noexcept;
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
    class d3d11_render_global
    {
    public:
        HWND__* hwnd = nullptr;
        com_ptr<ID3D11Device> device;
        d3d11_render_svg svg;
    };
    class d3d11_render_system : public render_system
    {
    public:
        error_type initialize(const d3d11_render_global global, const window_size size, const window_flags flags) noexcept;
        ~d3d11_render_system() noexcept
        {
            filter(0);
        }
    private:
        error_type exec() noexcept override;
        error_type signal(const event_data& event) noexcept override;
        error_type update(render_list&& render) noexcept override;
    private:
        error_type resize(const window_size size, const window_flags flags) noexcept;
    private:
        cvar m_cvar;
        mutex m_lock;
        d3d11_render_global m_global;
        com_ptr<ID3D11DeviceContext> m_context;
        com_ptr<ID3D11Texture2D> m_texture;
        com_ptr<ID3D11RenderTargetView> m_view;
        com_ptr<ID3D11Buffer> m_svg_vertices;
        com_ptr<ID3D11Buffer> m_svg_buffer;
    };

    class d3d11_display : public display
    {
    public:
        error_type draw(const bool vsync) noexcept override;
        error_type resize() noexcept override;
        error_type create(shared_ptr<render_system>& render) const noexcept override;
        void* event() const noexcept override
        {
            return nullptr;
        }
        error_type update(const render_frame& frame) noexcept override;
        error_type initialize(const adapter& adapter, const window_flags flags) noexcept;
        error_type bind(HWND__* const window) noexcept;
    private:
        adapter m_adapter;
        window_flags m_flags = window_flags::none;
        library m_d3d11_lib = "d3d11"_lib;
        d3d11_render_global m_global;
        d3d11_swap_chain m_chain;
        map<uint32_t, render_frame> m_draw;
    };
}