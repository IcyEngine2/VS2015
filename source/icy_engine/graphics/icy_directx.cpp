#include "icy_directx.hpp"
#include "shaders/screen_vs.hpp"
#include "shaders/draw_texture_ps.hpp"
#include "shaders/gui_vs.hpp"
#include "shaders/gui_ps.hpp"
#include <icy_engine/core/icy_entity.hpp>
#include <xmmintrin.h>

using namespace icy;

error_type directx_system::initialize() noexcept
{
    ICY_ERROR(m_lib_dxgi.initialize());
    if (m_flags & gpu_flags::debug)
    {
        ICY_ERROR(m_lib_debug.initialize());
        if (const auto func = ICY_FIND_FUNC(m_lib_debug, DXGIGetDebugInterface1))
        {
            com_ptr<IDXGIInfoQueue> debug;
            func(0, IID_PPV_ARGS(&debug));
            if (debug)
            {
                const auto guid = GUID{ 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 };
                debug->SetBreakOnSeverity(guid, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                debug->SetBreakOnSeverity(guid, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
                debug->SetBreakOnSeverity(guid, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, TRUE);
                debug->SetBreakOnSeverity(guid, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO, TRUE);
            }
        }
    }
    if (m_flags & gpu_flags::d3d12)
    {
        ICY_ERROR(m_lib_d3d12.initialize());
    }
    if (m_flags & (gpu_flags::d3d10 | gpu_flags::d3d11))
    {
        m_lib_d3d11.initialize();
    }
    return error_type();
}
error_type directx_system::create_gpu_list(array<shared_ptr<gpu_device>>& list) const noexcept
{
    decltype(&D3D12CreateDevice) d3d12_func = nullptr;
    decltype(&D3D11CreateDevice) d3d11_func = nullptr;

    d3d12_func = ICY_FIND_FUNC(m_lib_d3d12, D3D12CreateDevice);
    d3d11_func = ICY_FIND_FUNC(m_lib_d3d11, D3D11CreateDevice);

    if ((m_flags & gpu_flags::d3d12) && !d3d12_func)
        return error_type();
    if ((m_flags & gpu_flags::d3d11) && !d3d11_func)
        return error_type();

    com_ptr<IDXGIFactory1> dxgi_factory;
    if (const auto func = ICY_FIND_FUNC(m_lib_dxgi, CreateDXGIFactory1))
    {
        ICY_COM_ERROR(func(IID_PPV_ARGS(&dxgi_factory)));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    auto count = 0u;
    while (true)
    {
        com_ptr<IDXGIAdapter1> com_adapter;
        auto hr = dxgi_factory->EnumAdapters1(count++, &com_adapter);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;
        ICY_COM_ERROR(hr);

        DXGI_ADAPTER_DESC1 desc;
        ICY_COM_ERROR(com_adapter->GetDesc1(&desc));

        if ((m_flags & gpu_flags::hardware) && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            continue;

        const auto has_api_mask = uint32_t(m_flags) & uint32_t(gpu_flags::d3d10 | gpu_flags::d3d11 | gpu_flags::d3d12);

        const auto d3d10_level = D3D_FEATURE_LEVEL_10_1;
        const auto d3d11_level = D3D_FEATURE_LEVEL_11_0;
        const auto d3d12_level = D3D_FEATURE_LEVEL_11_0;

        const auto err_d3d12 = !d3d12_func ? S_FALSE : d3d12_func(com_adapter, d3d12_level, __uuidof(ID3D12Device), nullptr);
        const auto err_d3d11 = !d3d11_func ? S_FALSE : d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &d3d11_level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
        const auto err_d3d10 = !d3d11_func ? S_FALSE : d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &d3d10_level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);

        const auto has_d3d12 = d3d12_func && err_d3d12 == S_FALSE;
        const auto has_d3d11 = d3d11_func && err_d3d11 == S_FALSE;
        const auto has_d3d10 = d3d11_func && err_d3d10 == S_FALSE;

        const auto append = [&desc, &list, com_adapter, this](gpu_flags flag)->error_type
        {
            if (m_flags & gpu_flags::debug)
                flag = flag | gpu_flags::debug;
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                flag = flag | gpu_flags::hardware;

            shared_ptr<directx_gpu_device> new_adapter;
            ICY_ERROR(make_shared(new_adapter, make_shared_from_this(this)));
            ICY_ERROR(new_adapter->initialize(desc, com_adapter, flag));
            ICY_ERROR(list.push_back(std::move(new_adapter)));
            return error_type();
        };

        if (has_d3d12 && (has_api_mask ? (m_flags & gpu_flags::d3d12) : true))
        {
            ICY_ERROR(append(gpu_flags::d3d12));
        }
        if (has_d3d11 && (has_api_mask ? (m_flags & gpu_flags::d3d11) : true))
        {
            ICY_ERROR(append(gpu_flags::d3d11));
        }
        if (has_d3d10 && (has_api_mask ? (m_flags & gpu_flags::d3d10) : true))
        {
            ICY_ERROR(append(gpu_flags::d3d10));
        }
    }
    return error_type();
}

error_type directx_gpu_device::msaa(render_flags& flags) const noexcept
{
    return error_type();
}
error_type directx_gpu_device::initialize(const DXGI_ADAPTER_DESC1& desc, const com_ptr<IDXGIAdapter> handle, const gpu_flags flags) noexcept
{
    m_handle = handle;
    m_flags = flags;

    const auto wide = const_array_view<wchar_t>(desc.Description, wcsnlen(desc.Description, _countof(desc.Description)));
    ICY_ERROR(to_string(wide, m_name));

    m_shr_mem = desc.SharedSystemMemory;
    m_vid_mem = desc.DedicatedVideoMemory;
    m_sys_mem = desc.DedicatedSystemMemory;
    m_luid = (uint64_t(desc.AdapterLuid.HighPart) << 0x20) | desc.AdapterLuid.LowPart;

    return error_type();
}
error_type directx_gpu_device::create_device(com_ptr<ID3D11Device>& device) noexcept
{
    if (const auto func = ICY_FIND_FUNC(m_system->m_lib_d3d11, D3D11CreateDevice))
    {
        auto level = m_flags & gpu_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
        auto flags = 0u;
        if (m_flags & gpu_flags::debug)
            flags |= D3D11_CREATE_DEVICE_DEBUG;

        ICY_COM_ERROR(func(m_handle, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1,
            D3D11_SDK_VERSION, &device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}

directx_display_system::~directx_display_system() noexcept
{
    if (m_thread) m_thread->wait();
    if (m_gpu_event) CloseHandle(m_gpu_event);
    if (m_cpu_timer) CloseHandle(m_cpu_timer);
    filter(0);
}
error_type directx_display_system::initialize() noexcept
{
    auto window = shared_ptr<icy::window>(m_hwnd);
    if (!window)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(m_adapter->create_device(m_device));
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter->handle())->GetParent(IID_PPV_ARGS(&factory)));

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

    filter(event_type::system_internal | event_type::resource_load);
    return error_type();
}
error_type directx_display_system::do_resize(const window_size size) noexcept
{
    m_rtv = nullptr;
    m_chain = nullptr;

    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter->handle())->GetParent(IID_PPV_ARGS(&factory)));

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

    return error_type();
}
error_type directx_display_system::do_repaint(const bool vsync) noexcept
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

        auto rsystem = resource_system::global();
        for (auto&& list : texture.data)
        {
            for (auto&& cmd : list.cmd)
            {
                const auto it = m_tex2d.find(cmd.tex);
                if (it != m_tex2d.end() || !rsystem)
                    continue;

                resource_header header;
                header.index = cmd.tex;
                header.type = resource_type::image;
                ICY_ERROR(rsystem->load(header));
                ICY_ERROR(m_tex2d.insert(cmd.tex, com_ptr<ID3D11ShaderResourceView>()));
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
                const auto it = m_tex2d.find(cmd.tex);
                if (it == m_tex2d.end())
                    return make_unexpected_error();

                if (!it->value)
                    continue;

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

    const auto hr = m_chain->Present(vsync, 0);

    if (FAILED(hr))
    {
        if (hr == DXGI_STATUS_OCCLUDED)
            ;
        else
            return make_system_error(hr);
    }
    return error_type();
}
error_type directx_display_system::exec() noexcept
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
                const auto& event_data = event->data<directx_display_internal_event>();
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
            else if (event->type == event_type::resource_load)
            {
                auto& event_data = event->data<resource_event>();
                if (event_data.error)
                    continue;

                const auto it = m_tex2d.find(event_data.header.index);
                if (it == m_tex2d.end() || it->value)
                    continue;

                const auto& image = event_data.image();

                const auto desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM,
                    uint32_t(image.cols()), uint32_t(image.rows()), 1, 1, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE);

                D3D11_SUBRESOURCE_DATA init = {};
                init.pSysMem = image.data();
                init.SysMemPitch = uint32_t(image.cols()) * sizeof(color);

                com_ptr<ID3D11Texture2D> texture;
                ICY_COM_ERROR(m_device->CreateTexture2D(&desc, &init, &texture));

                auto srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(texture, D3D11_SRV_DIMENSION_TEXTURE2D);
                com_ptr<ID3D11ShaderResourceView> srv;
                ICY_COM_ERROR(m_device->CreateShaderResourceView(texture, &srv_desc, &srv));
                it->value = std::move(srv);
            }
        }
        if (size.x && size.y)
        {
            ICY_ERROR(do_resize(size));
        }

        if (m_cpu_ready && m_gpu_ready)
        {
            std::pair<string, render_gui_frame> pair2d;
            if (m_frame.queue2d.pop(pair2d))
            {
                while (m_frame.queue2d.pop(pair2d))
                    ;
                m_cpu_ready = false;
                auto it = m_frame.map2d.find(pair2d.first);
                if (it == m_frame.map2d.end())
                {
                    ICY_ERROR(m_frame.map2d.insert(std::move(pair2d.first), render_gui_frame(), &it));
                }
                it->value = std::move(pair2d.second);
            }
            std::pair<string, directx_display_texture> pair3d;
            if (m_frame.queue3d.pop(pair3d))
            {
                while (m_frame.queue3d.pop(pair3d))
                    ;
                m_cpu_ready = false;
                auto it = m_frame.map3d.find(pair3d.first);
                if (it == m_frame.map3d.end())
                {
                    ICY_ERROR(m_frame.map3d.insert(std::move(pair3d.first), directx_display_texture(), &it));
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
error_type directx_display_system::signal(const event_data* event) noexcept
{
    return m_update.wake();
}
error_type directx_display_system::repaint(const string_view tag, render_gui_frame& frame) noexcept
{
    string str;
    ICY_ERROR(copy(tag, str));
    ICY_ERROR(m_frame.queue2d.push(std::make_pair(std::move(str), std::move(frame))));
    ICY_ERROR(m_update.wake());
    return error_type();
}
error_type directx_display_system::repaint(const string_view tag, shared_ptr<render_surface> frame) noexcept
{
    string str;
    ICY_ERROR(copy(tag, str));
    //ICY_ERROR(m_frame.queue3d.push(std::make_pair(std::move(str), std::move(frame))));
    ICY_ERROR(m_update.wake());
    return error_type();
}


shared_ptr<render_system> directx_render_surface::system() noexcept 
{
    return shared_ptr<directx_render_system>(m_system);
}

directx_render_system::~directx_render_system() noexcept
{
    if (m_thread) m_thread->wait();
    filter(0);
}
error_type directx_render_system::initialize() noexcept
{
    ICY_ERROR(m_adapter->create_device(m_device));
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    ICY_ERROR(m_sync.initialize());

    CD3D11_BUFFER_DESC desc(uint32_t(64_kb), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.vtx_world));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.vtx_angle));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.vtx_color));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.vtx_texuv));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.obj_world));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.obj_scale));
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.obj_angle));

    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ICY_COM_ERROR(m_device->CreateBuffer(&desc, nullptr, &m_cpu_buffer.idx_array));

    
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    filter(event_type::system_internal | event_type::resource_load | event_type::entity_event);
    return error_type();
}
error_type directx_render_system::exec() noexcept
{
    while (*this)
    {
        window_size size;
        while (auto event = pop())
        {
            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<directx_render_internal_event>();
                if (event_data.type == directx_render_internal_event_type::repaint)
                {
                    ICY_ERROR(do_repaint(event_data.size));
                }
            }
            else if (event->type == event_type::resource_load)
            {
                const auto& event_data = event->data<resource_event>();
                if (event_data.error)
                    continue;

                if (event_data.header.type == resource_type::mesh)
                {
                    ICY_ERROR(load_mesh(event_data.header.index, event_data.mesh()));
                }
                else if (event_data.header.type == resource_type::material)
                {
                    const auto it = m_materials.find(event_data.header.index);
                    if (it == m_materials.end())
                        continue;
                    ICY_ERROR(copy(event_data.material(), *it->value));
                }
            }
            else if (event->type == event_type::entity_event)
            {
                const auto& event_data = event->data<entity_event>();
                if (event_data.type == entity_event_type::create_entity)
                {
                    if (uint32_t(event_data.entity.types()) & uint32_t(component_type::mesh))
                    {
                        auto new_node = make_unique<directx_render_node>();
                        ICY_ERROR(m_nodes.insert(event_data.entity.index(), std::move(new_node)));
                    }
                }
                else if (event_data.type == entity_event_type::remove_entity)
                {
                    const auto it = m_nodes.find(event_data.entity.index());
                    if (it != m_nodes.end())
                    {
                        //reset_materials(it->value);
                        m_nodes.erase(it);
                    }
                }
                else if (event_data.type == entity_event_type::modify_component)
                {
                    if (event_data.ctype == component_type::transform)
                    {
                        const auto it = m_nodes.find(event_data.entity.index());
                        if (it == m_nodes.end())
                            continue;

                        if (!event_data.value.get(it->value->transform))
                            continue;

                       /* for (auto&& pair : it->value->materials)
                        {
                            //pair.value->meshes;
                        }

                        for (auto&& index : it->value.meshes)
                        {
                            const auto mesh = m_meshes.find(index);
                            if (mesh != m_meshes.end())
                            {
                                const auto material = m_materials.find(mesh->value.material);
                                if (material != m_materials.end())
                                    material->value.state &= ~directx_material_ready_obj;
                            }
                        }*/
                    }
                    else if (event_data.ctype == component_type::mesh)
                    {
                        const auto it = m_nodes.find(event_data.entity.index());
                        if (it == m_nodes.end())
                            continue;

                        /*for (auto&& index : it->value.meshes)
                        {
                            const auto mesh = m_meshes.find(index);
                            if (mesh != m_meshes.end())
                            {
                                const auto material = m_materials.find(mesh->value.material);
                                if (material != m_materials.end())
                                    material->value.state &= ~directx_material_ready_vtx;
                            }
                        }

                        const_array_view<guid> meshes;
                        if (event_data.value.get(meshes))
                        {
                            ICY_ERROR(it->value.meshes.assign(meshes));
                            for (auto&& mesh : it->value.meshes)
                            {
                                const auto jt = m_meshes.find(mesh);
                                if (jt == m_meshes.end())
                                {
                                    ICY_ERROR(m_meshes.insert(mesh, directx_render_mesh()));
                                    auto rsystem = resource_system::global();
                                    if (rsystem)
                                    {
                                        resource_header header;
                                        header.index = mesh;
                                        header.type = resource_type::mesh;
                                        ICY_ERROR(rsystem->load(header));
                                    }
                                }
                                else
                                {
                                    const auto material = m_materials.find(jt->value.material);
                                    if (material != m_materials.end())
                                        material->value.state &= ~directx_material_ready_vtx;
                                }
                            }
                        }*/
                    }
                    
                }

            }
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type directx_render_system::signal(const event_data* event) noexcept
{
    return m_sync.wake();
}
error_type directx_render_system::repaint(const window_size size) noexcept
{
    directx_render_internal_event new_event;
    new_event.type = directx_render_internal_event_type::repaint;
    new_event.size = size;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(new_event)));
    return error_type();
}
error_type directx_render_system::load_mesh(const guid index, const render_mesh& mesh) noexcept
{
    const auto it = m_meshes.find(index);
    if (it == m_meshes.end())
        return error_type();

    auto& new_mesh = *it->value;
    new_mesh.type = mesh.type;
    for (auto&& vec : mesh.world)
    {
        ICY_ERROR(new_mesh.world.push_back(_mm_set_ps(vec.x, vec.y, vec.z, 1)));
    }
    for (auto&& vec : mesh.angle)
    {
        ICY_ERROR(new_mesh.angle.push_back(_mm_set_ps(vec.x, vec.y, vec.z, 1)));
    }
    ICY_ERROR(copy(mesh.indices, new_mesh.indices));

 /*  auto jt = m_materials.find(old_mesh.material);
    if (jt == m_materials.end())
    {
        auto new_material = make_unique<directx_render_material>();
        if (!new_material)
            return make_stdlib_error(std::errc::not_enough_memory);

        ICY_ERROR(new_material->meshes.insert(it->key, &new_mesh));
        ICY_ERROR(m_materials.insert(old_mesh.material, std::move(new_material), &jt));

        auto rsystem = resource_system::global();
        if (rsystem)
        {
            resource_header header;
            header.index = old_mesh.material;
            header.type = resource_type::material;
            ICY_ERROR(rsystem->load(header));
        }
    }

    new_mesh.material = jt->value.get();
    ICY_ERROR(jt->value->meshes.insert(it->key, &new_mesh));*/
    return error_type();
}
error_type directx_render_system::do_repaint(const window_size size) noexcept
{
    /*  const auto update_buffer = [this](com_ptr<ID3D11Buffer>& cpu_buffer, com_ptr<ID3D11Buffer>& gpu_buffer, const void* const data, const size_t bytes)
    {
        D3D11_BUFFER_DESC cpu_desc = {};
        D3D11_BUFFER_DESC gpu_desc = {};
        cpu_buffer->GetDesc(&cpu_desc);
        if (!gpu_buffer)
        {
            gpu_desc = cpu_desc;
            gpu_desc.CPUAccessFlags = 0;
            gpu_desc.Usage = D3D11_USAGE_DEFAULT;
            ICY_COM_ERROR(m_device->CreateBuffer(&gpu_desc, nullptr, &gpu_buffer));
        }
        else
        {
            gpu_buffer->GetDesc(&gpu_desc);
        }

        if (cpu_desc.ByteWidth < bytes)
        {
            cpu_desc.ByteWidth = UINT(bytes);
            ICY_COM_ERROR(m_device->CreateBuffer(&cpu_desc, nullptr, &cpu_buffer));
        }
        if (gpu_desc.ByteWidth < bytes)
        {
            gpu_desc.ByteWidth = UINT(bytes);
            ICY_COM_ERROR(m_device->CreateBuffer(&gpu_desc, nullptr, &gpu_buffer));
        }
        com_ptr<ID3D11DeviceContext> context;
        m_device->GetImmediateContext(&context);

        D3D11_MAPPED_SUBRESOURCE map;
        ICY_COM_ERROR(context->Map(cpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
        memcpy(map.pData, data, bytes);
        context->Unmap(cpu_buffer, 0);
        context->CopyResource(gpu_buffer, cpu_buffer);
        return error_type();
    };
    for (auto&& pair : m_materials)
    {
       
        if (pair.value.state & directx_material_ready_vtx)
            continue;

        auto vcount = 0u;
        auto icount = 0u;
        array<const directx_render_mesh*> mesh_list;

        auto offset = 0u;
        for (auto&& mesh_pair : m_meshes)
        {
            const auto& mesh = mesh_pair.value;
            if (mesh.material != pair.key)
                continue;

            ICY_ERROR(mesh_list.push_back(&mesh));
            vcount += uint32_t(mesh.world.size());
            icount += uint32_t(mesh.indices.size());
            ICY_ERROR(pair.value.mesh_to_index.insert(pair.key, offset));
            offset += uint32_t(mesh.indices.size());
        }
        array<directx_vec4> world;
        array<uint32_t> index;
        ICY_ERROR(world.reserve(vcount));
        ICY_ERROR(index.reserve(icount));

        offset = 0u;
        for (auto&& ptr : mesh_list)
        {
            ICY_ERROR(world.append(ptr->world.begin(), ptr->world.end()));
            for (auto&& value : ptr->indices)
            {
                ICY_ERROR(index.push_back(value + offset));
            }
            offset += uint32_t(ptr->indices.size());
        }
        ICY_ERROR(update_buffer(m_cpu_buffer.vtx_world, pair.value.vtx_world, world.data(), world.size() * sizeof(world[0])));
        ICY_ERROR(update_buffer(m_cpu_buffer.idx_array, pair.value.idx_array, index.data(), index.size() * sizeof(index[0])));
        pair.value.state |= directx_material_ready_vtx;
    }
    for (auto&& pair : m_materials)
    {
        if (pair.value.state & directx_material_ready_obj)
            continue;

        array<directx_vec4> world;
        array<directx_vec4> angle;
        array<directx_vec4> scale;
        auto offset = 0u;
        for (auto&& node_pair : m_nodes)
        {
            const auto it = node_pair.value.meshes.find(pair.key);
            if (it == node_pair.value.meshes.end())
                continue;

            ICY_ERROR(world.push_back(node_pair.value.transform.world));
            ICY_ERROR(scale.push_back(node_pair.value.transform.scale));
            ICY_ERROR(angle.push_back(node_pair.value.transform.angle));

            ICY_ERROR(pair.value.node_to_index.insert(node_pair.key, offset));
            offset += 1;
        }
        ICY_ERROR(update_buffer(m_cpu_buffer.obj_world, pair.value.obj_world, world.data(), world.size() * sizeof(world[0])));
        ICY_ERROR(update_buffer(m_cpu_buffer.obj_scale, pair.value.obj_scale, scale.data(), scale.size() * sizeof(scale[0])));
        ICY_ERROR(update_buffer(m_cpu_buffer.obj_angle, pair.value.obj_angle, world.data(), angle.size() * sizeof(angle[0])));
        pair.value.state |= directx_material_ready_obj;
    }

    shared_ptr<directx_render_surface> new_surface;
    ICY_ERROR(make_shared(new_surface, *this, m_index.fetch_add(1, std::memory_order_acq_rel) + 1));
    ICY_ERROR(do_repaint(*new_surface, size));
    */
    render_event new_event;
    //new_event.surface = std::move(new_surface);
    
    
    ICY_ERROR(event::post(this, event_type::render_event, std::move(new_event)));
    

    return error_type();
}
error_type directx_render_system::do_repaint(directx_render_surface& surface, const window_size size) noexcept
{
  /*  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    ICY_COM_ERROR(m_device->CreateTexture2D(&desc, nullptr, &surface.m_buffer));

    auto rtv = CD3D11_RENDER_TARGET_VIEW_DESC(D3D11_RTV_DIMENSION_TEXTURE2D);
    auto srv = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D);
    ICY_COM_ERROR(m_device->CreateRenderTargetView(surface.m_buffer, &rtv, &surface.m_rtv));
    ICY_COM_ERROR(m_device->CreateShaderResourceView(surface.m_buffer, &srv, &surface.m_srv));

    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(&context);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto&& pair : m_materials)
    {
        const auto& material = pair.value;
        context->IASetIndexBuffer(material.idx_array, DXGI_FORMAT_R32_UINT, 0);

        ID3D11Buffer* buffers[] = { material.vtx_world, material.obj_world };
        context->IASetVertexBuffers(0, _countof(buffers), buffers, strides, offsets);
        context->

    }

    context->IASetInputLayout(m_layout);

    context->Flush();

    */
    return error_type();
}