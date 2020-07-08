#include "icy_d3d11.hpp"
#include "icy_window.hpp"
#include <d3d11.h>
#include "shaders/svg_ps.hpp"
#include "shaders/svg_vs.hpp"

using namespace icy;

error_type d3d11_swap_chain::window(HWND__*& hwnd) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc;
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    hwnd = desc.OutputWindow;
    return error_type();
}
error_type d3d11_swap_chain::size(window_size& output) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    output = { desc.BufferDesc.Width, desc.BufferDesc.Height };
    return error_type();
}
error_type d3d11_swap_chain::update(const render_frame& frame, const bool vsync) noexcept
{
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    device->GetImmediateContext(&context);

    if (frame.d3d11.commands && frame.d3d11.texture)
    {
        com_ptr<ID3D11Resource> back_buffer;
        context->ExecuteCommandList(frame.d3d11.commands, FALSE);
        m_view->GetResource(&back_buffer);
        context->ResolveSubresource(back_buffer, 0, frame.d3d11.texture, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
    else
    {
        float rgba[4] = { 0, 0, 0, 1 };
        context->ClearRenderTargetView(m_view, rgba);
    }
    ICY_COM_ERROR(m_chain->Present(vsync, 0));
    return error_type();
}
error_type d3d11_swap_chain::initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const window_flags flags) noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.Windowed = TRUE;
    desc.OutputWindow = hwnd;
    com_ptr<IDXGISwapChain> chain;
    ICY_COM_ERROR(factory.CreateSwapChain(&device, &desc, &chain));
    ICY_COM_ERROR(chain->QueryInterface(&m_chain));

    com_ptr<ID3D11Resource> texture;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&texture)));
    ICY_COM_ERROR(device.CreateRenderTargetView(texture, nullptr, &m_view));
    return error_type();
}
error_type d3d11_swap_chain::resize() noexcept
{
    m_view = nullptr;

    com_ptr<ID3D11Device> device;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    ICY_COM_ERROR(m_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

    com_ptr<ID3D11Resource> texture;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&texture)));
    ICY_COM_ERROR(device->CreateRenderTargetView(texture, nullptr, &m_view));

    return error_type();
}

error_type d3d11_render_svg::initialize(ID3D11Device& device) noexcept
{
    ICY_COM_ERROR(device.CreateVertexShader(g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), nullptr, &m_vshader));
    ICY_COM_ERROR(device.CreatePixelShader(g_shader_bytes_svg_ps, _countof(g_shader_bytes_svg_ps), nullptr, &m_pshader));
    D3D11_INPUT_ELEMENT_DESC layout[2] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ICY_COM_ERROR(device.CreateInputLayout(layout, _countof(layout), g_shader_bytes_svg_vs, _countof(g_shader_bytes_svg_vs), &m_layout));

    auto rasterizer = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
    //rasterizer.FrontCounterClockwise = TRUE;
    rasterizer.MultisampleEnable = TRUE;
    ICY_COM_ERROR(device.CreateRasterizerState(&rasterizer, &m_rasterizer));

    return error_type();
}
void d3d11_render_svg::operator()(ID3D11DeviceContext& context) const noexcept
{
    context.VSSetShader(m_vshader, nullptr, 0);
    context.PSSetShader(m_pshader, nullptr, 0);
    context.IASetInputLayout(m_layout);
    context.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context.RSSetState(m_rasterizer);
}

error_type d3d11_render_system::initialize(const d3d11_render_global global, const window_size size, const window_flags flags) noexcept
{
    ICY_ERROR(m_lock.initialize());
    m_global = global;
    ICY_COM_ERROR(m_global.device->CreateDeferredContext(0, &m_context));
    ICY_ERROR(resize(size, flags));
    filter(event_type::window_close | event_type::window_resize | event_type::render_update);
    return error_type();
}
error_type d3d11_render_system::exec() noexcept
{
    while (true)
    {
        auto event = pop();
        if (!event)
        {
            ICY_ERROR(m_cvar.wait(m_lock));
            continue;
        }
        if (event->type == event_type::global_quit)
        {
            break;
        }
        else if (event->type & event_type::window_any)
        {
            auto system = shared_ptr<event_system>(event->source);
            auto window = static_cast<const window_system*>(system.get());
            if (window->handle() != m_global.hwnd)
                continue;

            if (event->type == event_type::window_close)
            {
                break;
            }
            else if (event->type == event_type::window_resize)
            {
                ICY_ERROR(resize(event->data<window_size>(), window->flags()));
            }
        }
        else if (event->type == event_type::render_update)
        {
            const auto& list = event->data<render_list>();
            array<render_d2d_vertex> d2d_vertices;
            auto d2d_capacity = 0_z;
            for (auto&& elem : list.data)
            {
                if (elem.type == render_element_type::svg)
                    d2d_capacity += elem.svg.geometry.vertices().size();
            }
            ICY_ERROR(d2d_vertices.reserve(d2d_capacity));

            for (auto&& elem : list.data)
            {
                switch (elem.type)
                {
                case render_element_type::clear:
                {
                    float rgba[4];
                    elem.clear.to_rgbaf(rgba);
                    m_context->ClearRenderTargetView(m_view, rgba);
                    break;
                }
                case render_element_type::svg:
                {
                    const auto& svg = elem.svg;
                    for (auto vertex : svg.geometry.vertices())
                    {
                        vertex.coord.x += svg.offset.x;
                        vertex.coord.y += svg.offset.y;
                        vertex.coord.z += svg.layer;
                        ICY_ERROR(d2d_vertices.push_back(vertex));
                    }
                    break;
                }
                default:
                    break;
                }
            }

            const auto make_buffer = [this](ID3D11Device& device, com_ptr<ID3D11Buffer>& gpu,
                const void* data, const size_t capacity, D3D11_BIND_FLAG flags)
            {
                D3D11_BUFFER_DESC gpu_desc = {};
                if (gpu) gpu->GetDesc(&gpu_desc);
                if (capacity > gpu_desc.ByteWidth)
                {
                    gpu_desc.ByteWidth = align_up(uint32_t(capacity), 256);
                    gpu_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
                    gpu_desc.BindFlags = flags;
                    gpu_desc.Usage = D3D11_USAGE_DYNAMIC;
                    ICY_COM_ERROR(device.CreateBuffer(&gpu_desc, nullptr, &gpu));
                }
                D3D11_MAPPED_SUBRESOURCE map;
                ICY_COM_ERROR(m_context->Map(gpu, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &map));
                memcpy(map.pData, data, capacity);
                m_context->Unmap(gpu, 0);
                return error_type();
            };

            if (!d2d_vertices.empty())
            {
                ICY_ERROR(make_buffer(*m_global.device, m_svg_vertices, d2d_vertices.data(),
                    d2d_vertices.size() * sizeof(render_d2d_vertex), D3D11_BIND_VERTEX_BUFFER));

                D3D11_TEXTURE2D_DESC desc;
                this->m_texture->GetDesc(&desc);
                const float gsize[] = { desc.Width, desc.Height };
                ICY_ERROR(make_buffer(*m_global.device, m_svg_buffer, gsize, sizeof(gsize), D3D11_BIND_CONSTANT_BUFFER));

                m_global.svg(*m_context);

                const auto view = D3D11_VIEWPORT{ 0, 0, gsize[0], gsize[1], 0, 1 };
                const uint32_t strides[] = { sizeof(render_d2d_vertex) };
                const uint32_t offsets[] = { 0 };
                m_context->IASetVertexBuffers(0, 1, &m_svg_vertices, strides, offsets);
                m_context->RSSetViewports(1, &view);
                m_context->VSSetConstantBuffers(0, 1, &m_svg_buffer);
                m_context->OMSetRenderTargets(1, &m_view, nullptr);
                m_context->Draw(uint32_t(d2d_vertices.size()), 0);
            }
            render_frame frame;
            ICY_COM_ERROR(m_context->FinishCommandList(TRUE, &frame.d3d11.commands));
            frame.d3d11.texture = m_texture;
            frame.index = list.frame;
            ICY_ERROR(event::post(this, event_type::render_frame, std::move(frame)));
        }
    }
    return error_type();
}
error_type d3d11_render_system::signal(const event_data&) noexcept
{
    m_cvar.wake();
    return error_type();
}
error_type d3d11_render_system::update(render_list&& list) noexcept
{
    ICY_ERROR(post(nullptr, event_type::render_update, std::move(list)));
    return error_type();
}
error_type d3d11_render_system::resize(const window_size size, const window_flags flags) noexcept
{
    auto tex_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_TYPELESS, size.x, size.y, 1, 1,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    auto rtv_desc = CD3D11_RENDER_TARGET_VIEW_DESC(sample_count(flags) > 1 ?
        D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    tex_desc.SampleDesc.Count = sample_count(flags);

    com_ptr<ID3D11Texture2D> texture;
    com_ptr<ID3D11RenderTargetView> view;
    ICY_COM_ERROR(m_global.device->CreateTexture2D(&tex_desc, nullptr, &texture));
    ICY_COM_ERROR(m_global.device->CreateRenderTargetView(texture, &rtv_desc, &view));

    m_texture = texture;
    m_view = view;
    return error_type();
}

error_type d3d11_display::initialize(const adapter& adapter, const window_flags flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;
    ICY_ERROR(m_d3d11_lib.initialize());

    if (const auto func = ICY_FIND_FUNC(m_d3d11_lib, D3D11CreateDevice))
    {
        auto level = D3D_FEATURE_LEVEL_11_0;
        ICY_COM_ERROR(func(static_cast<IDXGIAdapter*>(adapter.handle()),
            D3D_DRIVER_TYPE_UNKNOWN, nullptr, (flags & window_flags::debug_layer) ?
            D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG : 0,
            &level, 1, D3D11_SDK_VERSION, &m_global.device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (flags & window_flags::debug_layer)
    {
        if (auto debug = com_ptr<ID3D11Debug>(m_global.device))
        {
            ;
        }
    }
    ICY_ERROR(m_global.svg.initialize(*m_global.device));
    return error_type();
}
error_type d3d11_display::bind(HWND__* const window) noexcept
{
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    ICY_ERROR(m_chain.initialize(*factory, *m_global.device, window, m_flags));
    m_global.hwnd = window;
    return error_type();
}
error_type d3d11_display::draw(const bool vsync) noexcept
{
    render_frame frame;
    const auto it = m_draw.begin();
    if (it != m_draw.end())
    {
        frame = std::move(it->value);
        m_draw.erase(it);
    }
    if (const auto error = m_chain.update(frame, vsync))
    {
        if (error.source == error_source_system && error.code == DXGI_STATUS_OCCLUDED)
            ;
        else
            return error;
    }
    return error_type();
}
error_type d3d11_display::resize() noexcept
{
    ICY_ERROR(m_chain.resize());
    return error_type();
}
error_type d3d11_display::create(shared_ptr<render_system>& render) const noexcept
{
    window_size size;
    ICY_ERROR(m_chain.size(size));
    shared_ptr<d3d11_render_system> ptr;
    ICY_ERROR(ptr->initialize(m_global, size, m_flags));
    render = std::move(ptr);
    return error_type();
}
error_type d3d11_display::update(const render_frame& frame) noexcept
{
    ICY_ERROR(m_draw.find_or_insert(frame.index, render_frame(frame)));
    return error_type();
}

error_type icy::make_d3d11_display(unique_ptr<display>& display, const adapter& adapter, const window_flags flags, HWND__* const window) noexcept
{
    auto new_display = make_unique<d3d11_display>();
    if (!new_display)
        return make_stdlib_error(std::errc::not_enough_memory);
    ICY_ERROR(new_display->initialize(adapter, flags));
    ICY_ERROR(new_display->bind(window));
    display = std::move(new_display);
    return error_type();
}