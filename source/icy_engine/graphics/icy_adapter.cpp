#include "icy_adapter.hpp"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11.h>

using namespace icy;

adapter::adapter(const adapter& rhs) noexcept
{
    memcpy(this, &rhs, sizeof(adapter));
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
adapter::~adapter() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}
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
        auto hr = dxgi_factory->EnumAdapters1(count++, &com_adapter);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break;
        ICY_COM_ERROR(hr);

        DXGI_ADAPTER_DESC1 desc;
        ICY_COM_ERROR(com_adapter->GetDesc1(&desc));

        if ((filter & adapter_flags::hardware) && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            continue;

        const auto has_api_mask = uint32_t(filter) & uint32_t(adapter_flags::d3d10 | adapter_flags::d3d11 | adapter_flags::d3d12);

        const auto d3d10_level = D3D_FEATURE_LEVEL_10_1;
        const auto d3d11_level = D3D_FEATURE_LEVEL_11_0;
        const auto d3d12_level = D3D_FEATURE_LEVEL_11_0;

        const auto err_d3d12 = !d3d12_func ? S_FALSE : d3d12_func(com_adapter, d3d12_level, __uuidof(ID3D12Device), nullptr);
        const auto err_d3d11 = !d3d11_func ? S_FALSE : d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &d3d11_level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
        const auto err_d3d10 = !d3d11_func ? S_FALSE : d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &d3d10_level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);

        const auto has_d3d12 = d3d12_func && err_d3d12 == S_FALSE;
        const auto has_d3d11 = d3d11_func && err_d3d11 == S_FALSE;
        const auto has_d3d10 = d3d11_func && err_d3d10 == S_FALSE;

        const auto append = [&desc, &array, com_adapter, filter](adapter_flags flag)->error_type
        {
            if (filter & adapter_flags::debug)
                flag = flag | adapter_flags::debug;
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                flag = flag | adapter_flags::hardware;

            adapter new_adapter;           
            new_adapter.data = allocator_type::allocate<data_type>(1);
            if (!new_adapter.data)
                return make_stdlib_error(std::errc::not_enough_memory);
            allocator_type::construct(new_adapter.data);
            ICY_ERROR(new_adapter.data->initialize(com_adapter, flag));
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
        if (has_d3d10 && (has_api_mask ? (filter & adapter_flags::d3d10) : true))
        {
            ICY_ERROR(append(adapter_flags::d3d10));
        }
    }
    std::sort(array.begin(), array.end(), [](const adapter& lhs, const adapter& rhs) 
        { return lhs.vid_mem() > rhs.vid_mem(); });
    return{};
}
string_view adapter::name() const noexcept
{
    return data ? data->m_name : string_view();
}
void* adapter::handle() const noexcept
{
    return data ? static_cast<IDXGIAdapter*>(data->adapter()) : nullptr;
}
error_type adapter::msaa(render_flags& quality) const noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    array<uint32_t> levels;
    ICY_ERROR(data->query_msaa(levels));
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
size_t adapter::vid_mem() const noexcept
{
    return data ? data->m_vid_mem : 0;
}
size_t adapter::sys_mem() const noexcept
{
    return data ? data->m_sys_mem : 0;
}
size_t adapter::shr_mem() const noexcept
{
    return data ? data->m_shr_mem : 0;
}
uint64_t adapter::luid() const noexcept
{
    return data ? data->m_luid : 0;
}
adapter_flags adapter::flags() const noexcept
{
    return data ? data->m_flags : adapter_flags::none;
}

error_type adapter::data_type::initialize(const com_ptr<IDXGIAdapter> adapter, const adapter_flags flags) noexcept
{
    ICY_ERROR(m_lib_dxgi.initialize());

    m_adapter = adapter;
    m_flags = flags;

    DXGI_ADAPTER_DESC desc;
    ICY_COM_ERROR(adapter->GetDesc(&desc));
    const auto wide = const_array_view<wchar_t>(desc.Description, wcsnlen(desc.Description, _countof(desc.Description)));
    ICY_ERROR(to_string(wide, m_name));

    m_shr_mem = desc.SharedSystemMemory;
    m_vid_mem = desc.DedicatedVideoMemory;
    m_sys_mem = desc.DedicatedSystemMemory;
    m_luid = (uint64_t(desc.AdapterLuid.HighPart) << 0x20) | desc.AdapterLuid.LowPart;

    return error_type();
}
error_type adapter::data_type::query_msaa(array<uint32_t>& quality) const noexcept
{
    quality.clear();
    if (m_flags & adapter_flags::d3d12)
    {
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
    }
    else if (m_flags & (adapter_flags::d3d10 | adapter_flags::d3d11))
    {
        auto lib = "d3d11"_lib;
        ICY_ERROR(lib.initialize());
        if (const auto func = ICY_FIND_FUNC(lib, D3D11CreateDevice))
        {
            const auto level = m_flags & adapter_flags::d3d10 ?
                D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
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
    }
    else
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    return error_type();
}
