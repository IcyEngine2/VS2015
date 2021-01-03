#include "icy_display.hpp"
#include "icy_texture.hpp"
#include <dxgi1_6.h>

using namespace icy;

display_data::~display_data()
{
    while (auto texture_data = static_cast<render_texture::data_type*>(m_frame.queue.pop()))
        texture_data->destroy();

    if (m_gpu_event) CloseHandle(m_gpu_event);
    if (m_cpu_timer) CloseHandle(m_cpu_timer);
    if (m_update) CloseHandle(m_update);
}
error_type display_data::initialize(IUnknown& device, IDXGIFactory& factory, HWND__* const window, const display_flags flags) noexcept
{
    m_flags = flags;

    DXGI_SWAP_CHAIN_DESC desc0 = {};
    DXGI_SWAP_CHAIN_DESC1 desc1 = {};
    desc0.BufferDesc.Format = desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc0.SampleDesc = desc1.SampleDesc = { 1, 0 };
    desc0.BufferUsage = desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc0.BufferCount = desc1.BufferCount = buffer_count(flags);
    desc0.OutputWindow = window;
    desc0.Windowed = TRUE;
    desc0.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc0.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    desc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    com_ptr<IDXGIFactory2> factory2;
    factory.QueryInterface(&factory2);
    if (factory2)
    {
        com_ptr<IDXGISwapChain1> chain1;
        com_ptr<IDXGISwapChain2> chain2;
        factory2->CreateSwapChainForHwnd(&device, window, &desc1, nullptr, nullptr, &chain1);
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
        ICY_COM_ERROR(factory.CreateSwapChain(&device, &desc0, &m_chain));
    }

    DXGI_SWAP_CHAIN_DESC desc;
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    ICY_ERROR(resize(*m_chain, window_size(desc.BufferDesc.Width, desc.BufferDesc.Height)));

    m_cpu_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!m_cpu_timer)
        return last_system_error();

    m_update = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_update)
        return last_system_error();

    return error_type();
}
error_type display_data::exec() noexcept
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
                return error_type();
            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<display_options>();
                if (event_data.vsync.count() >= 0)
                {
                    m_frame.delta = event_data.vsync;
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
            ICY_ERROR(resize(*m_chain, size));
        }

        if (m_cpu_ready && m_gpu_ready)
        {
            if (auto texture_data = static_cast<render_texture::data_type*>(m_frame.queue.pop()))
            {
                ICY_SCOPE_EXIT{ texture_data->destroy(); };
                m_cpu_ready = false;
                m_gpu_ready = false;
                render_frame frame;
                const auto now = clock_type::now();

                ICY_ERROR(repaint(*m_chain, m_frame.delta == display_frame_vsync, texture_data->dxgi));
                frame.time[render_frame::time_display] = clock_type::now() - now;
                frame.index = m_frame.index++;
                ICY_ERROR(event::post(this, event_type::render_frame, std::move(frame)));
                reset();
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
error_type display_data::options(display_options options) noexcept
{
    return post(this, event_type::system_internal, std::move(options));
}
error_type display_data::signal(const event_data&) noexcept
{
    if (!SetEvent(m_update))
        return last_system_error();
    return error_type();
}
error_type display_data::render(render_texture& texture) noexcept
{
    if (!texture.data)
        return make_stdlib_error(std::errc::invalid_argument);

    m_frame.queue.push(texture.data);
    texture.data = nullptr;

    if (!SetEvent(m_update))
        return last_system_error();
    return error_type();
}

error_type icy::create_event_system(shared_ptr<display_system>& system, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    if (adapter.flags() & adapter_flags::d3d12)
        return display_data::create_d3d12(system, adapter, hwnd, flags);
    else if (adapter.flags() & (adapter_flags::d3d11 | adapter_flags::d3d10))
        return display_data::create_d3d11(system, adapter, hwnd, flags);
    else
        return make_stdlib_error(std::errc::invalid_argument);
}