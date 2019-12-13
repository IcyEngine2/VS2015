#include <icy_adapter.hpp>
#include <icy_window.hpp>
#include "icy_com.hpp"
#include "icy_utility.hpp"
#include <dxgi1_6.h>
#include "d3dx12.h"
using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class d3d12_display
{
public:
    ~data_type() noexcept;
    error_type initialize(IDXGIAdapter& adapter, HWND__& hwnd, const display_flag flags) noexcept;
    error_type wake() noexcept;
    error_type loop(display_system& queue, const duration_type timeout) noexcept;
private:
    win32_cvar m_cvar;
    library m_d3d12_lib = "d3d12"_lib;
    com_ptr<ID3D12Device> m_device;
    com_ptr<ID3D12CommandQueue> m_queue;
    com_ptr<ID3D12DescriptorHeap> m_heap;
    display_flag m_flags = display_flag::none;
    com_ptr<IDXGISwapChain1> m_chain;
    HANDLE m_wait_queue = nullptr;
    HANDLE m_wait_chain = nullptr;
};

void display_system::shutdown() noexcept
{
    any_system_shutdown(data);
}
error_type display_system::initialize(const adapter& adapter, window& window, const display_flag flags) noexcept
{
    if (!adapter.handle() || !window.handle())
        return make_stdlib_error(std::errc::invalid_argument);

    shutdown();
    ICY_ERROR(any_system_initialize(data, *static_cast<IDXGIAdapter*>(adapter.handle()), *window.handle(), flags));
    filter(event_type::window_resize);
    return {};
}
error_type display_system::loop(const duration_type timeout) noexcept
{
    if (!data || timeout.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);
    return data->loop(*this, timeout);
}
error_type display_system::signal(const event_data&) noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);
    data->wake();
    return {};
}

display_system::data_type::~data_type() noexcept
{
    if (m_wait_queue)
        CloseHandle(m_wait_queue);
}
error_type display_system::data_type::initialize(IDXGIAdapter& adapter, HWND__& hwnd, const display_flag flags) noexcept
{
    const auto buffer_count = 2 + (flags & display_flag::triple_buffer ? 1 : 0);
    //adapter = new_adapter;
    m_flags = flags;
    ICY_ERROR(m_d3d12_lib.initialize());

    if (flags & display_flag::debug_layer)
    {
        if (const auto func = ICY_FIND_FUNC(m_d3d12_lib, D3D12GetDebugInterface))
        {
            com_ptr<ID3D12Debug> debug;
            func(IID_PPV_ARGS(&debug));
            if (debug)
                debug->EnableDebugLayer();
        }
    }

    if (const auto func = ICY_FIND_FUNC(m_d3d12_lib, D3D12CreateDevice))
    {
        ICY_COM_ERROR(func(&adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }

    if (flags & display_flag::debug_layer)
    {
        if (auto debug = com_ptr<ID3D12InfoQueue>(m_device))
        {
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            debug->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID deny[] =
            {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumSeverities = _countof(severities);
            filter.DenyList.pSeverityList = severities;
            filter.DenyList.NumIDs = _countof(deny);
            filter.DenyList.pIDList = deny;
            ICY_COM_ERROR(debug->PushStorageFilter(&filter));
        }
    }
    D3D12_COMMAND_QUEUE_DESC queue_desc{ D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT };
    ICY_COM_ERROR(m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_queue)));
    
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{ };
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors = buffer_count;
    ICY_COM_ERROR(m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_heap)));
    
    com_ptr<IDXGIFactory2> factory;
    ICY_COM_ERROR(adapter.GetParent(IID_PPV_ARGS(&factory)));
    ICY_COM_ERROR(factory->MakeWindowAssociation(&hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_PRINT_SCREEN));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = { 1, 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = buffer_count;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    //desc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ICY_COM_ERROR(factory->CreateSwapChainForHwnd(m_queue, &hwnd, &desc, nullptr, nullptr, &m_chain));

    for (auto k = 0u; k < buffer_count; ++k)
    {
        com_ptr<ID3D12Resource> texture;
        ICY_COM_ERROR(m_chain->GetBuffer(k, IID_PPV_ARGS(&texture)));
        const auto size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_device->CreateRenderTargetView(texture, nullptr, 
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), k, size));
    }
    com_ptr<IDXGISwapChain2> chain;
    if (m_chain->QueryInterface(&chain) == S_OK)
    {
       // m_wait_chain = chain->GetFrameLatencyWaitableObject();
        //ICY_COM_ERROR(chain->SetMaximumFrameLatency(2));
    }

    m_wait_queue = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return {};
}
error_type display_system::data_type::wake() noexcept
{
    if (!SetEvent(m_wait_queue))
        return last_system_error();
    return {};    
}
error_type display_system::data_type::loop(display_system& queue, const duration_type timeout) noexcept
{
    while (true)
    {
        HANDLE handles[] = { m_wait_queue };
        const auto wait = WaitForMultipleObjectsEx(_countof(handles), handles, FALSE, 0, TRUE);
        if (wait == WAIT_OBJECT_0)
        {
            auto size = window_size{};
            while (auto event = queue.pop())
            {
                if (event->type == event_type::global_quit)
                {
                    return {};
                }
                else if (event->type == event_type::window_resize)
                {
                    size = event->data<window_size>();
                }
            }
            size = {};
        }
        else if (wait == WAIT_TIMEOUT)
        {
            const auto hr = m_chain->Present(1, 0);
            ICY_COM_ERROR(hr);
        }
        else
        {
            return last_system_error();
        }
    }

}