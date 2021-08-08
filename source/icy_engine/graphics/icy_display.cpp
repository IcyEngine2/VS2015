#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_image.hpp>
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include "shaders/screen_vs.hpp"
#include "shaders/draw_texture_ps.hpp"
#include "shaders/gui_vs.hpp"
#include "shaders/gui_ps.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct internal_event
{
    duration_type frame = duration_type(-1);
    window_size size;
};
struct display_texture_3d
{
    window_size offset;
    window_size size;
    com_ptr<ID3D11Texture2D> buffer;
    com_ptr<ID3D11ShaderResourceView> srv;
};
using display_texture_2d = render_gui_frame;
class display_system_data : public display_system
{
public:
    display_system_data(const adapter adapter, shared_ptr<window> hwnd) : m_adapter(adapter), m_hwnd(hwnd)
    {

    }
    ~display_system_data() noexcept;
    error_type initialize() noexcept;
    error_type do_resize(const window_size size) noexcept;
    error_type do_repaint(const bool vsync) noexcept;
private:
    error_type exec() noexcept override;
    error_type signal(const event_data* event) noexcept override;
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    /*error_type repaint(const texture& texture) noexcept override
    {
        return repaint(texture, window_size(), window_size());
    }
    error_type repaint(const texture& texture, const window_size offset, const window_size size) noexcept override;
    */
    error_type repaint(const string_view tag, render_gui_frame& frame) noexcept override;
    error_type resize(const window_size size) noexcept override
    {
        if (size.x == 0 || size.y == 0)
            return make_stdlib_error(std::errc::invalid_argument);

        internal_event msg;
        msg.size = size;
        return event_system::post(nullptr, event_type::system_internal, msg);
    }
    error_type frame(const duration_type duration) noexcept override
    {
        if (duration.count() < 0)
            return make_stdlib_error(std::errc::invalid_argument);

        internal_event msg;
        msg.frame = duration;
        return event_system::post(nullptr, event_type::system_internal, msg);
    }
    //error_type repaint(IDXGISwapChain& chain, const bool vsync, IDXGIResource* const dxgi) noexcept;
private:
    struct frame_type
    {
        size_t index = 0;
        duration_type delta = display_frame_vsync;
        clock_type::time_point next = {};
        mpsc_queue<std::pair<string, display_texture_2d>> queue2d;
        mpsc_queue<std::pair<string, display_texture_3d>> queue3d;
        map<string, display_texture_2d> map2d;
        map<string, display_texture_3d> map3d;
    };
private:
    const adapter m_adapter;
    weak_ptr<window> m_hwnd;
    library m_lib = "d3d11"_lib;
    com_ptr<ID3D11Device> m_device;
    com_ptr<IDXGISwapChain> m_chain;
    com_ptr<ID3D11VertexShader> m_vshader2d;
    com_ptr<ID3D11VertexShader> m_vshader3d;
    com_ptr<ID3D11PixelShader> m_pshader2d;
    com_ptr<ID3D11PixelShader> m_pshader3d;
    com_ptr<ID3D11InputLayout> m_layout2d;
    com_ptr<ID3D11BlendState> m_blend2d;
    com_ptr<ID3D11RasterizerState> m_raster2d;
    com_ptr<ID3D11SamplerState> m_sampler;
    void* m_gpu_event = nullptr;
    void* m_cpu_timer = nullptr;
    sync_handle m_update;
    bool m_gpu_ready = true;
    bool m_cpu_ready = true;
    shared_ptr<event_thread> m_thread;
    frame_type m_frame;
    com_ptr<ID3D11RenderTargetView> m_rtv;
    com_ptr<ID3D11Buffer> m_vbuffer2d;
    com_ptr<ID3D11Buffer> m_ibuffer2d;
    map<uint32_t, com_ptr<ID3D11ShaderResourceView>> m_tex2d;
};
ICY_STATIC_NAMESPACE_END

display_system_data::~display_system_data() noexcept
{
    if (m_thread) m_thread->wait();
    if (m_gpu_event) CloseHandle(m_gpu_event);
    if (m_cpu_timer) CloseHandle(m_cpu_timer);
    filter(0);
}
error_type display_system_data::initialize() noexcept
{
    auto window = shared_ptr<icy::window>(m_hwnd);
    if (!window)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_lib.initialize());

    //com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    if (const auto func = ICY_FIND_FUNC(m_lib, D3D11CreateDevice))
    {
        auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        auto flags = 0u;
        if (m_adapter.flags() & adapter_flags::debug)
            flags |= D3D11_CREATE_DEVICE_DEBUG;

        const auto adapter = static_cast<IDXGIAdapter*>(m_adapter.handle());
        ICY_COM_ERROR(func(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1,
            D3D11_SDK_VERSION, &m_device, nullptr, &context));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));

    void* hwnd = nullptr;
    ICY_ERROR(window->win_handle(hwnd));
    if (hwnd)
    {
        ICY_COM_ERROR(factory->MakeWindowAssociation(static_cast<HWND__*>(hwnd),
            DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    }
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_screen_vs, sizeof(g_shader_bytes_screen_vs), nullptr, &m_vshader3d));
    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_draw_texture_ps, sizeof(g_shader_bytes_draw_texture_ps), nullptr, &m_pshader3d));
    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_gui_vs, sizeof(g_shader_bytes_gui_vs), nullptr, &m_vshader2d));
    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_gui_ps, sizeof(g_shader_bytes_gui_ps), nullptr, &m_pshader2d));

    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ICY_COM_ERROR(m_device->CreateInputLayout(layout, _countof(layout), g_shader_bytes_gui_vs, sizeof(g_shader_bytes_gui_vs), &m_layout2d));

    auto blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
    auto& blend_render = blend_desc.RenderTarget[0];
    blend_render = D3D11_RENDER_TARGET_BLEND_DESC
    {
        true, 
        D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
        D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
        D3D11_COLOR_WRITE_ENABLE_ALL
    };
    ICY_COM_ERROR(m_device->CreateBlendState(&blend_desc, &m_blend2d));

    auto rasterizer_desc = CD3D11_RASTERIZER_DESC(D3D11_DEFAULT);
    rasterizer_desc.ScissorEnable = true;
    rasterizer_desc.AntialiasedLineEnable = true;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.DepthClipEnable = false;
    ICY_COM_ERROR(m_device->CreateRasterizerState(&rasterizer_desc, &m_raster2d));

    auto desc = CD3D11_SAMPLER_DESC(D3D11_DEFAULT);
    ICY_COM_ERROR(m_device->CreateSamplerState(&desc, &m_sampler));
  
    ICY_ERROR(do_resize(window->size()));

    m_cpu_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!m_cpu_timer)
        return last_system_error();

    ICY_ERROR(m_update.initialize());

    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    filter(event_type::system_internal);
    return error_type();
}
error_type display_system_data::do_resize(const window_size size) noexcept
{
    m_rtv = nullptr;
    m_chain = nullptr;

    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));

#if X11_GUI
    com_ptr<ID3D11Texture2D> buffer;
    const auto desc = CD3D11_TEXTURE2D_DESC(
        DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_RENDER_TARGET);
    ICY_COM_ERROR(m_device->CreateTexture2D(&desc, nullptr, &buffer));
    ICY_COM_ERROR(m_device->CreateRenderTargetView(buffer, &CD3D11_RENDER_TARGET_VIEW_DESC(
        D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM), &m_rtv));

#elif _WIN32
    void* handle = nullptr;
    if (auto window = shared_ptr<icy::window>(m_hwnd))
        ICY_ERROR(window->win_handle(handle));
    
    DXGI_SWAP_CHAIN_DESC desc0 = {};
    DXGI_SWAP_CHAIN_DESC1 desc1 = {};
    desc0.BufferDesc.Format = desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc0.SampleDesc = desc1.SampleDesc = { 1, 0 };
    desc0.BufferUsage = desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc0.BufferCount = desc1.BufferCount = 2;// buffer_count(flags);
    desc0.OutputWindow = static_cast<HWND>(handle);
    desc0.Windowed = TRUE;
    desc0.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc0.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    desc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    com_ptr<IDXGIFactory2> factory2;
    factory->QueryInterface(&factory2);
    if (factory2)
    {
        com_ptr<IDXGISwapChain1> chain1;
        com_ptr<IDXGISwapChain2> chain2;
        factory2->CreateSwapChainForHwnd(m_device, desc0.OutputWindow, &desc1, nullptr, nullptr, &chain1);
        if (chain1)
        {
            chain1->QueryInterface(&chain2);
        }
        if (chain2)
        {
            m_gpu_event = chain2->GetFrameLatencyWaitableObject();
            ICY_COM_ERROR(chain2->QueryInterface(&m_chain));
            ICY_COM_ERROR(chain2->SetMaximumFrameLatency(1));
        }
    }
    if (!m_chain)
    {
        const auto hr = factory->CreateSwapChain(m_device, &desc0, &m_chain);
        ICY_COM_ERROR(hr);
    }
    com_ptr<ID3D11Texture2D> buffer;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&buffer)));
    auto desc = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM);
    ICY_COM_ERROR(m_device->CreateRenderTargetView(buffer, &desc, &m_rtv));
#endif

    /* com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    device->GetImmediateContext(&context);

    //m_rtv = nullptr;
    //context->OMSetRenderTargets(1, &m_rtv, nullptr);
   DXGI_SWAP_CHAIN_DESC chain_desc;
    ICY_COM_ERROR(m_chain->GetDesc(&chain_desc));
    ICY_COM_ERROR(m_chain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, chain_desc.Flags));
    
        */
    
    return error_type();
}
error_type display_system_data::do_repaint(const bool vsync) noexcept
{
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    float clear[4] = { 0, 0, 0, 1 };
    context->ClearRenderTargetView(m_rtv, clear);

    D3D11_TEXTURE2D_DESC desc;
    com_ptr<ID3D11Resource> resource;
    com_ptr<ID3D11Texture2D> buffer;
    m_rtv->GetResource(&resource);
    ICY_COM_ERROR(resource->QueryInterface(&buffer));
    buffer->GetDesc(&desc);

    context->OMSetRenderTargets(1, &m_rtv, nullptr);
    context->PSSetSamplers(0, 1, &m_sampler);


    if (!m_frame.map3d.empty())
    {
        context->PSSetShader(m_pshader3d, nullptr, 0);
        context->VSSetShader(m_vshader3d, nullptr, 0);
        context->IASetInputLayout(nullptr);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        context->RSSetState(nullptr);
    }

    for (auto&& pair : m_frame.map3d)
    {
        const auto& texture = pair.value;
        auto viewport = D3D11_VIEWPORT
        {
            float(texture.offset.x), float(texture.offset.y),
            float(texture.size.x), float(texture.size.y), 0, 1
        };
        if (!viewport.Width) viewport.Width = float(desc.Width);
        if (!viewport.Height) viewport.Height = float(desc.Height);

        ID3D11ShaderResourceView* srv[] = { texture.srv };

        context->PSSetShaderResources(0, 1, srv);    
        context->RSSetViewports(1, &viewport);

        D3D11_RECT rect = { 0, 0, LONG(desc.Width), LONG(desc.Height) };
        context->RSSetScissorRects(1, &rect);
        context->Draw(3, 0);
    }

    if (!m_frame.map2d.empty())
    {
        const float blend_factor[4] = { 0, 0, 0, 0 };
        context->PSSetShader(m_pshader2d, nullptr, 0);
        context->VSSetShader(m_vshader2d, nullptr, 0);
        context->IASetInputLayout(m_layout2d);
        context->OMSetBlendState(m_blend2d, blend_factor, 0xFFFFFFFF);
        context->RSSetState(m_raster2d);
    }

    for (auto&& pair : m_frame.map2d)
    {
        const auto& texture = pair.value;
        
        if (!pair.value.ibuffer || !pair.value.vbuffer)
            continue;

        D3D11_BUFFER_DESC idesc = {};
        D3D11_BUFFER_DESC vdesc = {};
        if (m_ibuffer2d) m_ibuffer2d->GetDesc(&idesc);
        if (m_vbuffer2d) m_vbuffer2d->GetDesc(&vdesc);

        const uint32_t ibytes = pair.value.ibuffer * sizeof(uint32_t);
        const uint32_t vbytes = pair.value.vbuffer * sizeof(render_gui_vtx);
        if (idesc.ByteWidth < ibytes)
        {
            idesc = CD3D11_BUFFER_DESC(ibytes, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
            ICY_COM_ERROR(m_device->CreateBuffer(&idesc, nullptr, &(m_ibuffer2d = nullptr)));
        }
        if (vdesc.ByteWidth < vbytes)
        {
            vdesc = CD3D11_BUFFER_DESC(vbytes, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
            ICY_COM_ERROR(m_device->CreateBuffer(&vdesc, nullptr, &(m_vbuffer2d = nullptr)));
        }

        auto viewport = D3D11_VIEWPORT
        {
            float(texture.viewport.vec[0]), float(texture.viewport.vec[1]),
            float(texture.viewport.vec[2]), float(texture.viewport.vec[3]), 0, 1
        };
        context->RSSetViewports(1, &viewport);

        D3D11_MAPPED_SUBRESOURCE imap;
        ICY_COM_ERROR(context->Map(m_ibuffer2d, 0, D3D11_MAP_WRITE_DISCARD, 0, &imap));
        auto iptr = static_cast<uint32_t*>(imap.pData);
        for (auto&& list : texture.data)
        {
            memcpy(iptr, list.idx.data(), list.idx.size() * sizeof(list.idx[0]));
            iptr += list.idx.size();
        }
        context->Unmap(m_ibuffer2d, 0);

        D3D11_MAPPED_SUBRESOURCE vmap;
        ICY_COM_ERROR(context->Map(m_vbuffer2d, 0, D3D11_MAP_WRITE_DISCARD, 0, &vmap));
        auto vptr = static_cast<render_gui_vtx*>(vmap.pData);
        for (auto&& list : texture.data)
        {
            memcpy(vptr, list.vtx.data(), list.vtx.size() * sizeof(list.vtx[0]));
            vptr += list.vtx.size();
        }
        context->Unmap(m_vbuffer2d, 0);

        for (auto&& list : texture.data)
        {
            for (auto&& cmd : list.cmd)
            {
                const auto it = m_tex2d.find(cmd.tex.index);
                if (it != m_tex2d.end())
                    continue;

                const auto buffer = blob_data(cmd.tex);
                image image;
                ICY_ERROR(image.load(global_realloc, nullptr, buffer, image_type::png));

                matrix<color> colors(image.size().y, image.size().x);
                if (colors.empty())
                    return make_stdlib_error(std::errc::not_enough_memory);

                ICY_ERROR(image.view(image_size(), colors));

                const auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, 
                    image.size().x, image.size().y, 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE);
                
                D3D11_SUBRESOURCE_DATA init = {};
                init.pSysMem = colors.data();
                init.SysMemPitch = image.size().x * sizeof(color);

                com_ptr<ID3D11Texture2D> texture;
                ICY_COM_ERROR(m_device->CreateTexture2D(&desc, &init, &texture));

                auto srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(texture, D3D11_SRV_DIMENSION_TEXTURE2D);
                com_ptr<ID3D11ShaderResourceView> srv;
                ICY_COM_ERROR(m_device->CreateShaderResourceView(texture, &srv_desc, &srv));
                ICY_ERROR(m_tex2d.insert(cmd.tex.index, std::move(srv)));
            }
        }

        context->IASetIndexBuffer(m_ibuffer2d, DXGI_FORMAT_R32_UINT, 0);
        const uint32_t strides[] = { sizeof(render_gui_vtx) };
        const uint32_t offsets[] = { 0 };
        context->IASetVertexBuffers(0, 1, &m_vbuffer2d, strides, offsets);

        auto ioffset = 0u;
        auto voffset = 0u;        
        for (auto&& list : texture.data)
        {
            for (auto&& cmd : list.cmd)
            {
                const auto it = m_tex2d.find(cmd.tex.index);
                if (it == m_tex2d.end())
                    return make_unexpected_error();

                D3D11_RECT rect =
                {
                    lround(cmd.clip.x - texture.viewport.x),
                    lround(cmd.clip.y - texture.viewport.y),
                    lround(cmd.clip.z - texture.viewport.x),
                    lround(cmd.clip.w - texture.viewport.y)
                };
                context->RSSetScissorRects(1, &rect);
                ID3D11ShaderResourceView* srv[1] = { it->value };
                context->PSSetShaderResources(0, 1, srv);
                context->DrawIndexed(cmd.size, cmd.idx + ioffset, cmd.vtx + voffset);
            }
            ioffset += uint32_t(list.idx.size());
            voffset += uint32_t(list.vtx.size());
        }
    }

#if X11_GUI
    context->Flush();
    if (auto window = shared_ptr<icy::window>(m_hwnd))
    {
        com_ptr<ID3D11Texture2D> cpu_texture;
        ICY_COM_ERROR(m_device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
            desc.Width, desc.Height, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ), nullptr, &cpu_texture));

        D3D11_BOX box = { 0, 0, 0, desc.Width, desc.Height, 1 };
        context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, static_cast<ID3D11Texture2D*>(buffer.get()), 0, &box);

        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(context->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &map));
        ICY_SCOPE_EXIT{ context->Unmap(cpu_texture, 0); };
        
        matrix<color> colors(desc.Height, desc.Width);
        if (colors.empty())
            return make_stdlib_error(std::errc::not_enough_memory);

        auto dst = colors.data();
        for (auto row = 0u; row < desc.Height; ++row)
        {
            auto ptr = reinterpret_cast<const uint32_t*>(static_cast<char*>(map.pData) + row * map.RowPitch);
            for (auto col = 0u; col < desc.Width; ++col, ++dst)
                *dst = color::from_rgba(ptr[col]);
        }
        ICY_ERROR(window->repaint(std::move(colors)));
    }
#elif _WIN32
    const auto hr = m_chain->Present(vsync, 0);

    //srv[0] = nullptr;
    //context->PSSetShaderResources(0, 1, srv);

    if (FAILED(hr))
    {
        if (hr == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(hr);
    }
#endif
    return error_type();
}
error_type display_system_data::exec() noexcept
{
    const auto reset = [this]
    {
        if (m_frame.delta == display_frame_vsync || m_frame.delta == display_frame_unlim && m_gpu_event)
        {
            m_cpu_ready = true;
        }
        else
        {
            auto now = clock_type::now();
            auto delta = m_frame.next - now;
            LARGE_INTEGER offset = {};
            if (delta.count() > 0)
            {
                m_frame.next = m_frame.next + m_frame.delta;
            }
            else
            {
                m_frame.next = now + (delta = m_frame.delta);
            }
            offset.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count() / 100;
            SetWaitableTimer(m_cpu_timer, &offset, 0, nullptr, nullptr, FALSE);
        }
    };
    while (*this)
    {
        window_size size;
        while (auto event = pop())
        {
            if (event->type == event_type::window_resize)
            {
                const auto& event_data = event->data<window_message>();
                const auto is_fullscreen = uint32_t(event_data.state) & uint32_t(window_state::popup | window_state::maximized);
                if (!is_fullscreen)
                {
                    auto window = shared_ptr<icy::window>(m_hwnd);
                    if (window && event_data.window == window->index())
                        size = event_data.size;
                }
            }
            else if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<internal_event>();
                if (event_data.frame.count() >= 0)
                {
                    m_frame.delta = event_data.frame;
                    reset();
                }
                if (event_data.size.x && event_data.size.y)
                {
                    size = event_data.size;
                }
            }
        }
        if (size.x && size.y)
        {
            ICY_ERROR(do_resize(size));
        }

        if (m_cpu_ready && m_gpu_ready)
        {
            std::pair<string, display_texture_2d> pair2d;
            if (m_frame.queue2d.pop(pair2d))
            {
                while (m_frame.queue2d.pop(pair2d))
                    ;
                m_cpu_ready = false;        
                auto it = m_frame.map2d.find(pair2d.first);
                if (it == m_frame.map2d.end())
                {
                    ICY_ERROR(m_frame.map2d.insert(std::move(pair2d.first), display_texture_2d(), &it));
                }
                it->value = std::move(pair2d.second);
            }
            std::pair<string, display_texture_3d> pair3d;
            if (m_frame.queue3d.pop(pair3d))
            {
                while (m_frame.queue3d.pop(pair3d))
                    ;
                m_cpu_ready = false;
                auto it = m_frame.map3d.find(pair3d.first);
                if (it == m_frame.map3d.end())
                {
                    ICY_ERROR(m_frame.map3d.insert(std::move(pair3d.first), display_texture_3d(), &it));
                }
                it->value = std::move(pair3d.second);
            }

            if (m_cpu_ready == false)
            {
                m_gpu_ready = m_chain ? false : true;

                display_message msg;
                const auto now = clock_type::now();
                ICY_ERROR(do_repaint(m_frame.delta == display_frame_vsync));
                msg.frame = clock_type::now() - now;
                msg.index = m_frame.index++;
                ICY_ERROR(event::post(this, event_type::display_update, std::move(msg)));
                reset();
            }
        }

        /*auto success = false;
        ICY_ERROR(m_frame.queue.push_if_empty(texture, success));
        if (success && !SetEvent(m_update))
            return last_system_error();*/

        HANDLE handles[3] = {};
        auto count = 0u;
        handles[count++] = m_update.handle();
        if (m_cpu_timer) handles[count++] = m_cpu_timer;
        if (m_gpu_event) handles[count++] = m_gpu_event;

        const auto index = WaitForMultipleObjectsEx(count, handles, FALSE, INFINITE, TRUE);
        void* handle = nullptr;
        if (index >= WAIT_OBJECT_0 && index < WAIT_OBJECT_0 + count)
            handle = handles[index - WAIT_OBJECT_0];

        if (handle == m_update.handle())
        {
            continue;
        }
        else if (handle && handle == m_cpu_timer)
        {
            m_cpu_ready = true;
        }
        else if (handle && handle == m_gpu_event)
        {
            m_gpu_ready = true;
        }
    }
    return error_type();
}
error_type display_system_data::signal(const event_data* event) noexcept
{
    return m_update.wake();
}
error_type display_system_data::repaint(const string_view tag, render_gui_frame& frame) noexcept
{
    display_texture_2d new_texture;
    new_texture = std::move(frame);
    string str;
    ICY_ERROR(copy(tag, str));
    ICY_ERROR(m_frame.queue2d.push(std::make_pair(std::move(str), std::move(new_texture))));
    ICY_ERROR(m_update.wake());
    return error_type();
}
/*error_type display_system_data::repaint(const texture& texture, const window_size offset, const window_size size) noexcept
{
    const auto handle = texture.handle();
    if (!handle)
        return make_stdlib_error(std::errc::invalid_argument);

    display_texture new_texture;
    ICY_COM_ERROR(m_device->OpenSharedResource(handle, IID_PPV_ARGS(&new_texture.buffer)));
    ICY_COM_ERROR(m_device->CreateShaderResourceView(new_texture.buffer,
        &CD3D11_SHADER_RESOURCE_VIEW_DESC(new_texture.buffer, D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1), &new_texture.srv));
    new_texture.offset = offset;
    new_texture.size = size;
    ICY_ERROR(m_frame.queue.push(std::move(new_texture)));
    ICY_ERROR(m_update.wake());
    return error_type();
}*/

error_type icy::create_display_system(shared_ptr<display_system>& system, shared_ptr<window> window, const adapter adapter) noexcept
{
    shared_ptr<display_system_data> ptr;
    ICY_ERROR(make_shared(ptr, adapter, window));
    ICY_ERROR(ptr->initialize());
    system = std::move(ptr);
    return error_type();
}
