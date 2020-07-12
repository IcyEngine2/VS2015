#include "icy_render.hpp"
#include "icy_system.hpp"
#include "icy_render_svg_core.hpp"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11_4.h>

using namespace icy;

error_type adapter::enumerate(const adapter_flags filter, array<adapter>& array) noexcept
{
    auto lib_dxgi = "dxgi"_lib;
    auto lib_d3d12 = "d3d12"_lib;
    auto lib_d3d11 = "d3d11"_lib;
    ICY_ERROR(lib_dxgi.initialize());

    if (filter & adapter_flags::debug)
    {
        auto lib_debug = "dxgidebug"_lib;
        ICY_ERROR(lib_debug.initialize());
        if (const auto func = ICY_FIND_FUNC(lib_debug, DXGIGetDebugInterface1))
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

    decltype(&D3D12CreateDevice) d3d12_func = nullptr;
    decltype(&D3D11CreateDevice) d3d11_func = nullptr;

    if (lib_d3d12.initialize())
        ;
    else
        d3d12_func = ICY_FIND_FUNC(lib_d3d12, D3D12CreateDevice);

    if (lib_d3d11.initialize())
        ;
    else
        d3d11_func = ICY_FIND_FUNC(lib_d3d11, D3D11CreateDevice);

    if ((filter & adapter_flags::d3d12) && !d3d12_func)
        return error_type();
    if ((filter & adapter_flags::d3d11) && !d3d11_func)
        return error_type();

    com_ptr<IDXGIFactory1> dxgi_factory;
    if (const auto func = ICY_FIND_FUNC(lib_dxgi, CreateDXGIFactory1))
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
        const auto hr = dxgi_factory->EnumAdapters1(count++, &com_adapter);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;
        ICY_COM_ERROR(hr);

        DXGI_ADAPTER_DESC1 desc = {};
        ICY_COM_ERROR(com_adapter->GetDesc1(&desc));

        if ((filter & adapter_flags::hardware) && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            continue;

        const auto has_api_mask = uint32_t(filter) & ~uint32_t(adapter_flags::hardware);
        const auto level = D3D_FEATURE_LEVEL_11_0;
        const auto err_d3d12 = d3d12_func(com_adapter, level, __uuidof(ID3D12Device), nullptr);
        const auto err_d3d11 = d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
        const auto has_d3d12 = d3d12_func && err_d3d12 == S_FALSE;
        const auto has_d3d11 = d3d11_func && err_d3d11 == S_FALSE;

        const auto append = [&desc, &array, com_adapter, filter](adapter_flags flag)->error_type
        {
            if (filter & adapter_flags::debug)
                flag = flag | adapter_flags::debug;

            adapter new_adapter;
            new_adapter.shr_mem = desc.SharedSystemMemory;
            new_adapter.vid_mem = desc.DedicatedVideoMemory;
            new_adapter.sys_mem = desc.DedicatedSystemMemory;
            new_adapter.luid = (uint64_t(desc.AdapterLuid.HighPart) << 0x20) | desc.AdapterLuid.LowPart;
            new_adapter.flags = flag;
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                new_adapter.flags = new_adapter.flags | adapter_flags::hardware;
            ICY_ERROR(any_system_initialize(new_adapter.data, com_adapter, new_adapter.flags));
            ICY_ERROR(array.push_back(std::move(new_adapter)));
            return error_type();
        };

        if (has_d3d12 && (has_api_mask ? (filter & adapter_flags::d3d12) : true))
        {
            ICY_ERROR(append(adapter_flags::d3d12));
        }
        if (has_d3d11 && (has_api_mask ? (filter & adapter_flags::d3d11) : true))
        {
            ICY_ERROR(append(adapter_flags::d3d11));
        }
    }
    std::sort(array.begin(), array.end(), [](const adapter& lhs, const adapter& rhs) { return lhs.vid_mem > rhs.vid_mem; });
    return{};
}
string_view adapter::name() const noexcept
{
    if (data)
        return data->name();
    return string_view();
}
void* adapter::handle() const noexcept
{
    return data ? &data->adapter() : nullptr;
}
error_type adapter::msaa(render_flags& quality) const noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    array<uint32_t> levels;
    if (data->flags() & adapter_flags::d3d12)
    {
        ICY_ERROR(data->msaa_d3d12(levels));
    }
    else if (data->flags() & adapter_flags::d3d11)
    {
        ICY_ERROR(data->msaa_d3d11(levels));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    for (auto&& level : levels)
    {
        switch (level)
        {
        case 0x02:
            quality = quality | render_flags::msaa_x2;
            break;
        case 0x04:
            quality = quality | render_flags::msaa_x4;
            break;
        case 0x08:
            quality = quality | render_flags::msaa_x8;
            break;
        case 0x10:
            quality = quality | render_flags::msaa_x16;
            break;
        }
    }
    return error_type();
}
adapter::adapter(const adapter& rhs) noexcept
{
    memcpy(this, &rhs, sizeof(adapter));
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
adapter::~adapter() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
        any_system_shutdown(data);
}

error_type adapter::data_type::initialize(const com_ptr<IDXGIAdapter> adapter, const adapter_flags flags) noexcept
{
    ICY_ERROR(m_lock.initialize());
    ICY_ERROR(m_lib_dxgi.initialize());
    m_adapter = adapter;
    m_flags = flags;

    DXGI_ADAPTER_DESC desc = {};
    ICY_COM_ERROR(adapter->GetDesc(&desc));
    ICY_ERROR(to_string(desc.Description, m_name));
    return error_type();
}
error_type adapter::data_type::msaa_d3d12(array<uint32_t>& quality) const noexcept
{
    quality.clear();

    auto lib = "d3d12"_lib;
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, D3D12CreateDevice))
    {
        com_ptr<ID3D12Device> device;
        ICY_COM_ERROR(func(m_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS sample = {};
        sample.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sample.SampleCount = 2u;

        while (true)
        {
            auto hr = device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &sample, sizeof(sample));
            if (SUCCEEDED(hr) && sample.NumQualityLevels)
            {
                ICY_ERROR(quality.push_back(sample.SampleCount));
                sample.SampleCount <<= 1;
            }
            else
                break;
        }
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}
error_type adapter::data_type::msaa_d3d11(array<uint32_t>& quality) const noexcept
{
    auto lib = "d3d11"_lib;
    ICY_ERROR(lib.initialize());
    if (const auto func = ICY_FIND_FUNC(lib, D3D11CreateDevice))
    {
        const auto level = D3D_FEATURE_LEVEL_11_0;
        com_ptr<ID3D11Device> device;
        ICY_COM_ERROR(func(m_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
            &level, 1, D3D11_SDK_VERSION, &device, nullptr, nullptr));

        auto sample = 2u;
        while (true)
        {
            auto count = 0u;
            const auto hr = device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sample, &count);
            if (SUCCEEDED(hr) && count)
            {
                ICY_ERROR(quality.push_back(sample));
                sample <<= 1;
            }
            else
                break;
        }
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}

display_data::~display_data()
{
    if (m_adapter & adapter_flags::d3d12)
    {
        while (auto frame = static_cast<d3d12_render_frame*>(m_frame.queue.pop()))
            destroy_frame(*frame);
    }
    else if (m_adapter & adapter_flags::d3d11)
    {
        while (auto frame = static_cast<d3d11_render_frame*>(m_frame.queue.pop()))
            destroy_frame(*frame);
    }
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
    
    m_cpu_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (!m_cpu_timer)
        return last_system_error();

    m_update = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_update)
        return last_system_error();

    m_chain->GetDesc(&desc0);
    ICY_ERROR(resize(window_size(desc0.BufferDesc.Width, desc0.BufferDesc.Height)));
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
    window_size size;
    while (true)
    {
        while (auto event = pop())
        {
            if (event->type == event_type::global_quit)
                return error_type();
            if (event->type == event_type::system_internal)
            {
                const auto& event_data = event->data<system_event>();
                if (event_data.vsync.count() < 0)
                {
                    size = event_data.size;
                }
                else
                {
                    m_frame.delta = event_data.vsync;
                    reset();
                }
            }
        }
        
        if (size.x && size.y)
        {
            ICY_ERROR(resize(*m_chain, size, m_flags));
            size = window_size();
        }
        if (m_cpu_ready && m_gpu_ready)
        {
            if (auto frame_data = m_frame.queue.pop())
            {
                render_frame frame;
                m_cpu_ready = false;
                m_gpu_ready = false;

                frame.data = allocator_type::allocate<render_frame::data_type>(1);
                if (!frame.data)
                    return make_stdlib_error(std::errc::not_enough_memory);
                allocator_type::construct(frame.data);

                if (m_adapter & adapter_flags::d3d12)
                {
                    frame.data->d3d12 = static_cast<d3d12_render_frame*>(frame_data);
                }
                else if (m_adapter & adapter_flags::d3d11)
                {
                    frame.data->d3d11 = static_cast<d3d11_render_frame*>(frame_data);
                }
                ICY_ERROR(repaint(*m_chain, m_frame.delta == display_frame_vsync, frame));
                frame.data->index = m_frame.index++;
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
error_type display_data::vsync(const duration_type delta) noexcept
{
    if (delta.count() < 0)
        return make_stdlib_error(std::errc::invalid_argument);
    system_event event;
    event.vsync = delta;
    return post(this, event_type::system_internal, event);
}
error_type display_data::resize(const window_size size) noexcept
{
    if (!size.x || !size.y)
        return make_stdlib_error(std::errc::invalid_argument);
    system_event event;
    event.size = size;
    return post(this, event_type::system_internal, event);    
}
error_type display_data::signal(const event_data&) noexcept
{
    if (!SetEvent(m_update))
        return last_system_error();
    return error_type();
}
error_type display_data::render(const render_frame frame) noexcept
{
    if (!frame.data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (m_adapter & adapter_flags::d3d12)
    {
        if (!frame.data->d3d12)
            return make_stdlib_error(std::errc::invalid_argument);
        m_frame.queue.push(frame.data->d3d12);
        frame.data->d3d12 = nullptr;
    }
    else if (m_adapter & adapter_flags::d3d11)
    {
        if (!frame.data->d3d11)
            return make_stdlib_error(std::errc::invalid_argument);
        m_frame.queue.push(frame.data->d3d11);
        frame.data->d3d11 = nullptr;
    }
    else 
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
    if (!SetEvent(m_update))
        return last_system_error();
    return error_type();
}

error_type render_list_data::clear(const color color) noexcept
{
    render_element elem(render_element_type::clear);
    elem.clear = color;
    return queue.push(std::move(elem));
}
error_type render_list_data::draw(const render_svg_geometry geometry, const render_d2d_matrix& transform, const float layer) noexcept
{
    const auto& vertices = geometry.vertices();
    if (vertices.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    render_element elem(render_element_type::svg);
    ICY_ERROR(elem.svg.resize(vertices.size()));
    for (auto k = 0_z; k < vertices.size(); ++k)
    {
        auto vec = transform * render_d2d_vector(vertices[k].coord.x, vertices[k].coord.y);
        auto& coord = elem.svg[k].coord;
        coord.x = vec.x;
        coord.y = vec.y;
        coord.z = vertices[k].coord.z + layer;
        elem.svg[k].color = vertices[k].color;
    }
    return queue.push(std::move(elem));
}

render_frame::render_frame(const render_frame& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_frame::~render_frame() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (data->d3d11) destroy_frame(*data->d3d11);
        if (data->d3d12) destroy_frame(*data->d3d12);
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}
size_t render_frame::index() const noexcept
{
    return data ? data->index : 0;
}
duration_type render_frame::time_cpu() const noexcept
{
    return data ? data->time_cpu : duration_type();
}
duration_type render_frame::time_gpu() const noexcept
{
    return data ? data->time_gpu : duration_type();
}

error_type icy::create_render_list(shared_ptr<render_list>& list) noexcept
{
    shared_ptr<render_list_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    list = new_ptr;
    return error_type();
}

error_type icy::create_render_factory(shared_ptr<render_factory>& system, const adapter& adapter, const render_flags flags) noexcept
{
    if (!adapter.data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (adapter.data->flags() & adapter_flags::d3d12)
    {
        return create_d3d12(system, *adapter.data, flags);
    }
    else if (adapter.data->flags() & adapter_flags::d3d11)
    {
        return create_d3d11(system, *adapter.data, flags);
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }
}
error_type icy::create_event_system(shared_ptr<display_system>& display, const adapter& adapter, HWND__* const hwnd, const display_flags flags) noexcept
{
    if (!adapter.data || !hwnd)
        return make_stdlib_error(std::errc::invalid_argument);

    if (adapter.data->flags() & adapter_flags::d3d12)
    {
        return create_d3d12(display, *adapter.data, hwnd, flags);
    }
    else if (adapter.data->flags() & adapter_flags::d3d11)
    {
        return create_d3d11(display, *adapter.data, hwnd, flags);
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);        
    }
}
