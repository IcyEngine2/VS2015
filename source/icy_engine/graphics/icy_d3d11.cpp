#include "icy_d3d11.hpp"
#include <icy_engine/graphics/icy_window.hpp>
#include <d3d11.h>

using namespace icy;

error_type d3d11_swap_chain::window(HWND__*& hwnd) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    hwnd = desc.OutputWindow;
    return {};
}
error_type d3d11_swap_chain::present(const uint32_t vsync) noexcept
{
    ICY_COM_ERROR(m_chain->Present(vsync, 0));
    return {};
}
error_type d3d11_swap_chain::size(window_size& output) const noexcept
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(m_chain->GetDesc(&desc));
    output = { desc.BufferDesc.Width, desc.BufferDesc.Height };
    return {};
}
error_type d3d11_swap_chain::initialize(IDXGIFactory& factory, ID3D11Device& device, HWND__* const hwnd, const display_flag flags) noexcept
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
    return {};
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

    return {};
}

error_type d3d11_display::initialize(const adapter& adapter, const display_flag flags) noexcept
{
    m_adapter = adapter;
    m_flags = flags;
    ICY_ERROR(m_d3d11_lib.initialize());

    if (const auto func = ICY_FIND_FUNC(m_d3d11_lib, D3D11CreateDevice))
    {
        auto level = D3D_FEATURE_LEVEL_11_0;
        ICY_COM_ERROR(func(static_cast<IDXGIAdapter*>(adapter.handle()),
            D3D_DRIVER_TYPE_UNKNOWN, nullptr, (flags & display_flag::debug_layer) ? 
            D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG : 0,
            &level, 1, D3D11_SDK_VERSION, &m_device, nullptr, nullptr));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (flags & display_flag::debug_layer)
    {
        if (auto debug = com_ptr<ID3D11Debug>(m_device))
        {
            ;
        }
    }

    filter(event_type::none
        | event_type::window_resize
        | event_type::window_active
        | event_type::window_minimized
        | event_type::window_close);
    return {};
}
error_type d3d11_display::bind(HWND__* const window) noexcept
{
    com_ptr<IDXGIFactory> factory;
    ICY_COM_ERROR(static_cast<IDXGIAdapter*>(m_adapter.handle())->GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));
    ICY_ERROR(m_chain.initialize(*factory, *m_device, window, m_flags));

    window_size size = {};
    ICY_ERROR(m_chain.size(size));
    return {};
}
error_type d3d11_display::loop(const duration_type timeout) noexcept
{
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
            {
                return error_type{};
            }
            else if (event->type == event_type::window_close && event->source && event->data<bool>())
            {
                HWND__* hwnd = nullptr;
                ICY_ERROR(m_chain.window(hwnd));

                auto src = shared_ptr<event_queue>(event->source);
                if (src && hwnd == static_cast<const window*>(src.get())->handle())
                    return error_type{};
            }
        }
        com_ptr<ID3D11DeviceContext> context;
        m_device->GetImmediateContext(&context);
        //ID3D11RenderTargetView* rtv[] = { &m_chain.view() };
        //context->OMSetRenderTargets(_countof(rtv), rtv, nullptr);
        auto fr = frame();


        float colors[] = { 0.5F * (fr % 2000) / 2000, 0.3F * (fr % 1000) / 1000, 0.9F * (fr % 3000) / 3000, 1.0F };
        context->ClearRenderTargetView(&m_chain.view(), colors);

        //auto& buffer = m_buffers[m_frame.load(std::memory_order_acquire) % m_buffers.size()];
        //context->ExecuteCommandList(buffer.view(), FALSE);
        //m_queue[m_frame.load()];

        ICY_ERROR(post(this, event_type::display_refresh, m_frame.load(std::memory_order_acquire)));
        if (const auto error = m_chain.present())
        {
            if (error.source == error_source_system &&
                error.code == DXGI_STATUS_OCCLUDED)
                ;
            else
                return error;
        }
        m_frame.fetch_add(1, std::memory_order_release);
    }
}
error_type d3d11_display::signal(const event_data&) noexcept
{
    return {};
}


namespace icy
{
    error_type display_create_d3d11(const adapter& adapter, const display_flag flags, shared_ptr<display>& display) noexcept
    {
        shared_ptr<d3d11_display> d3d11;
        ICY_ERROR(make_shared(d3d11));
        ICY_ERROR(d3d11->initialize(adapter, flags));
        display = d3d11;
        return {};
    }
}