#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_display.hpp>
#include <icy_engine/graphics/icy_window.hpp>
#include <icy_engine/graphics/icy_render.hpp>
#include <dxgi1_6.h>
#include <d3d11_4.h>
#include "shaders/screen_vs.hpp"
#include "shaders/draw_texture_ps.hpp"

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct internal_event
{
    duration_type frame = duration_type(-1);
    window_size size;
};
struct display_texture
{
    window_size offset;
    window_size size;
    com_ptr<ID3D11Texture2D> buffer;
    com_ptr<ID3D11ShaderResourceView> srv;
};
class display_system_thread : public thread
{
public:
    display_system* system = nullptr;
    void cancel() noexcept
    {
        system->post(nullptr, event_type::global_quit);
    }
    error_type run() noexcept
    {
        if (auto error = system->exec())
            return event::post(system, event_type::system_error, std::move(error));
        return error_type();
    }
};
class display_system_data : public display_system
{
public:
    display_system_data(const adapter adapter, void* hwnd) : m_adapter(adapter), m_hwnd(hwnd)
    {

    }
    ~display_system_data() noexcept;
    error_type initialize() noexcept;
    error_type do_resize(const window_size size) noexcept;
    error_type do_repaint(const bool vsync, const display_texture& texture) noexcept;
private:
    error_type exec() noexcept override;
    error_type signal(const event_data& event) noexcept override;
    const icy::thread& thread() const noexcept override
    {
        return *m_thread;
    }
    error_type repaint(const texture& texture) noexcept override
    {
        return repaint(texture, window_size(), window_size());
    }
    error_type repaint(const texture& texture, const window_size offset, const window_size size) noexcept override;
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
        mpsc_queue<display_texture> queue;
    };
private:
    const adapter m_adapter;
    void* const m_hwnd;
    library m_lib = "d3d11"_lib;
    com_ptr<ID3D11Device> m_device;
    com_ptr<IDXGISwapChain> m_chain;
    com_ptr<ID3D11VertexShader> m_vertex;
    com_ptr<ID3D11PixelShader> m_pixel;
    com_ptr<ID3D11SamplerState> m_sampler;
    void* m_gpu_event = nullptr;
    void* m_cpu_timer = nullptr;
    void* m_update = nullptr;
    bool m_gpu_ready = true;
    bool m_cpu_ready = true;
    shared_ptr<display_system_thread> m_thread;
    frame_type m_frame;
    com_ptr<ID3D11RenderTargetView> m_rtv;
};
ICY_STATIC_NAMESPACE_END

display_system_data::~display_system_data() noexcept
{
    if (m_thread) m_thread->wait();
    if (m_gpu_event) CloseHandle(m_gpu_event);
    if (m_cpu_timer) CloseHandle(m_cpu_timer);
    if (m_update) CloseHandle(m_update);
    filter(0);
}
error_type display_system_data::initialize() noexcept
{
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

    ICY_COM_ERROR(factory->MakeWindowAssociation(static_cast<HWND__*>(m_hwnd),
        DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ICY_COM_ERROR(m_device->CreateVertexShader(g_shader_bytes_screen_vs, sizeof(g_shader_bytes_screen_vs), nullptr, &m_vertex));
    context->VSSetShader(m_vertex, nullptr, 0);

    ICY_COM_ERROR(m_device->CreatePixelShader(g_shader_bytes_draw_texture_ps, sizeof(g_shader_bytes_draw_texture_ps), nullptr, &m_pixel));
    context->PSSetShader(m_pixel, nullptr, 0);

    ICY_COM_ERROR(m_device->CreateSamplerState(&CD3D11_SAMPLER_DESC(D3D11_DEFAULT), &m_sampler));
    context->PSSetSamplers(0, 1, &m_sampler);

    //DXGI_SWAP_CHAIN_DESC desc;
    //ICY_COM_ERROR(m_chain->GetDesc(&desc));
    ICY_ERROR(do_resize(window_size()));

    m_cpu_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!m_cpu_timer)
        return last_system_error();

    m_update = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_update)
        return last_system_error();
  
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;

    filter(event_type::system_internal);
    return error_type();
}
error_type display_system_data::do_resize(const window_size) noexcept
{
    m_rtv = nullptr;
    m_chain = nullptr;

    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC desc0 = {};
    DXGI_SWAP_CHAIN_DESC1 desc1 = {};
    desc0.BufferDesc.Format = desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc0.SampleDesc = desc1.SampleDesc = { 1, 0 };
    desc0.BufferUsage = desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc0.BufferCount = desc1.BufferCount = 2;// buffer_count(flags);
    desc0.OutputWindow = static_cast<HWND__*>(m_hwnd);
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
        factory2->CreateSwapChainForHwnd(m_device, static_cast<HWND__*>(m_hwnd), &desc1, nullptr, nullptr, &chain1);
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
    com_ptr<ID3D11Texture2D> buffer;
    ICY_COM_ERROR(m_chain->GetBuffer(0, IID_PPV_ARGS(&buffer)));
    ICY_COM_ERROR(m_device->CreateRenderTargetView(buffer, &CD3D11_RENDER_TARGET_VIEW_DESC(
        D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM), &m_rtv));
    
    return error_type();
}
error_type display_system_data::do_repaint(const bool vsync, const display_texture& texture) noexcept
{
    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));
    device->GetImmediateContext(&context);

    DXGI_SWAP_CHAIN_DESC desc;
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    
    auto viewport = D3D11_VIEWPORT
    { 
        float(texture.offset.x), float(texture.offset.y), 
        float(texture.size.x), float(texture.size.y), 0, 1 
    };
    if (!viewport.Width) viewport.Width = float(desc.BufferDesc.Width);
    if (!viewport.Height) viewport.Height = float(desc.BufferDesc.Height);


    ID3D11ShaderResourceView* srv[] = { texture.srv };
    context->PSSetShaderResources(0, 1, srv);
    context->RSSetViewports(1, &viewport);
    context->OMSetRenderTargets(1, &m_rtv, nullptr);
    context->Draw(3, 0);

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
    while (true)
    {
        window_size size;
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
            {
                return error_type();
            }
            else if (event->type == event_type::window_resize)
            {
                const auto& event_data = event->data<window_message>();
                const auto is_fullscreen = uint32_t(event_data.state) & uint32_t(window_state::popup | window_state::maximized);
                if (!is_fullscreen)
                {
                    auto window = shared_ptr<icy::window>(event_data.window);
                    if (window && window->handle() == m_hwnd)
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
            display_texture texture;
            if (m_frame.queue.pop(texture))
            {
                m_cpu_ready = false;
                m_gpu_ready = false;
                
                display_message msg;
                const auto now = clock_type::now();
                ICY_ERROR(do_repaint(m_frame.delta == display_frame_vsync, texture));
                msg.frame = clock_type::now() - now;
                msg.index = m_frame.index++;
                ICY_ERROR(event::post(this, event_type::display_update, std::move(msg)));
                reset();
                
                /*auto success = false;
                ICY_ERROR(m_frame.queue.push_if_empty(texture, success));
                if (success && !SetEvent(m_update))
                    return last_system_error();*/
            }
        }

        HANDLE handles[3] = {};
        auto count = 0u;
        handles[count++] = m_update;
        if (m_cpu_timer) handles[count++] = m_cpu_timer;
        if (m_gpu_event) handles[count++] = m_gpu_event;

        const auto index = WaitForMultipleObjectsEx(count, handles, FALSE, INFINITE, TRUE);
        void* handle = nullptr;
        if (index >= WAIT_OBJECT_0 && index < WAIT_OBJECT_0 + count)
            handle = handles[index - WAIT_OBJECT_0];

        if (handle == m_update)
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
error_type display_system_data::signal(const event_data& event) noexcept
{
    if (!SetEvent(m_update))
        return last_system_error();
    return error_type();
}
error_type display_system_data::repaint(const texture& texture, const window_size offset, const window_size size) noexcept
{
    const auto handle = texture.handle();
    if (!handle)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D11Device> device;
    ICY_COM_ERROR(m_chain->GetDevice(IID_PPV_ARGS(&device)));

    display_texture new_texture;
    ICY_COM_ERROR(device->OpenSharedResource(handle, IID_PPV_ARGS(&new_texture.buffer)));
    ICY_COM_ERROR(device->CreateShaderResourceView(new_texture.buffer, 
        &CD3D11_SHADER_RESOURCE_VIEW_DESC(new_texture.buffer, D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1), &new_texture.srv));
    new_texture.offset = offset;
    new_texture.size = size;
    ICY_ERROR(m_frame.queue.push(std::move(new_texture)));

    if (!SetEvent(m_update))
        return last_system_error();

    return error_type();
}

error_type icy::create_display_system(shared_ptr<display_system>& system, void* const handle, const adapter adapter) noexcept
{
    shared_ptr<display_system_data> ptr;
    ICY_ERROR(make_shared(ptr, adapter, handle));
    ICY_ERROR(ptr->initialize());
    system = std::move(ptr);
    return error_type();
}
