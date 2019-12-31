#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/utility/icy_com.hpp>
#include "icy_adapter.hpp"
#include "icy_system.hpp"
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <d3d11_4.h>

using namespace icy;

error_type adapter::data_type::initialize(const com_ptr<IDXGIAdapter> adapter, const adapter_flag flags) noexcept
{
    ICY_ERROR(m_lib_dxgi.initialize());
    m_adapter = adapter;
    m_flags = flags;

    DXGI_ADAPTER_DESC desc = {};
    ICY_COM_ERROR(adapter->GetDesc(&desc));
    ICY_ERROR(to_string(desc.Description, m_name));
    return {};
}
error_type adapter::enumerate(const adapter_flag filter, array<adapter>& array) noexcept
{
    auto lib_dxgi = "dxgi"_lib;
    auto lib_d3d12 = "d3d12"_lib;
    auto lib_d3d11 = "d3d11"_lib;
    ICY_ERROR(lib_dxgi.initialize());

#if _DEBUG
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
#endif

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

    if ((filter & adapter_flag::d3d12) && !d3d12_func)
        return {};
    if ((filter & adapter_flag::d3d11) && !d3d11_func)
        return {};

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

        if ((filter & adapter_flag::hardware) && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            continue;

        const auto has_api_mask = uint32_t(filter) & ~uint32_t(adapter_flag::hardware);
        const auto level = D3D_FEATURE_LEVEL_11_0;
        const auto err_d3d12 = d3d12_func(com_adapter, level, __uuidof(ID3D12Device), nullptr);
        const auto err_d3d11 = d3d11_func(com_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &level, 1, D3D11_SDK_VERSION, nullptr, nullptr, nullptr);
        const auto has_d3d12 = d3d12_func && err_d3d12 == S_FALSE;
        const auto has_d3d11 = d3d11_func && err_d3d11 == S_FALSE;

        const auto append = [&desc, &array, com_adapter](const adapter_flag flag)->error_type
        {
            adapter new_adapter;
            new_adapter.shr_mem = desc.SharedSystemMemory;
            new_adapter.vid_mem = desc.DedicatedVideoMemory;
            new_adapter.sys_mem = desc.DedicatedSystemMemory;
            new_adapter.luid = (uint64_t(desc.AdapterLuid.HighPart) << 0x20) | desc.AdapterLuid.LowPart;
            new_adapter.flags = flag;
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                new_adapter.flags = new_adapter.flags | adapter_flag::hardware;
            ICY_ERROR(any_system_initialize(new_adapter.data, com_adapter, new_adapter.flags));
            ICY_ERROR(array.push_back(std::move(new_adapter)));
            return {};
        };

        if (has_d3d12 && (has_api_mask ? (filter & adapter_flag::d3d12) : true))
        {
            ICY_ERROR(append(adapter_flag::d3d12));
        }
        if (has_d3d11 && (has_api_mask ? (filter & adapter_flag::d3d11) : true))
        {
            ICY_ERROR(append(adapter_flag::d3d11));
        }
    }
    std::sort(array.begin(), array.end(), [](const adapter& lhs, const adapter& rhs) { return lhs.vid_mem > rhs.vid_mem; });
    return{};
}
string_view adapter::name() const noexcept
{
    if (data)
        return data->name();
    return {};
}
void* adapter::handle() const noexcept
{
    return data ? &data->adapter() : nullptr;
}
error_type adapter_msaa(adapter& adapter, display_flag& quality) noexcept
{
    if (!adapter.data)
        return make_stdlib_error(std::errc::invalid_argument);

    array<uint32_t> levels;
    if (adapter.data->flags() & adapter_flag::d3d12)
    {
        ICY_ERROR(adapter.data->msaa_d3d12(levels));
    }
    else if (adapter.data->flags() & adapter_flag::d3d11)
    {
        ICY_ERROR(adapter.data->msaa_d3d11(levels));
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
            quality = quality | display_flag::msaa_x2;
            break;
        case 0x04:
            quality = quality | display_flag::msaa_x4;
            break;
        case 0x08:
            quality = quality | display_flag::msaa_x8;
            break;
        case 0x10:
            quality = quality | display_flag::msaa_x16;
            break;
        }
    }

    return {};
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

error_type adapter::data_type::msaa_d3d12(array<uint32_t>& quality) noexcept
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
    return {};
}
error_type adapter::data_type::msaa_d3d11(array<uint32_t>& quality) noexcept
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
    return {};
}


namespace icy
{
    error_type display_create_d3d12(const adapter&, const display_flag, shared_ptr<display>&) noexcept;
    error_type display_create_d3d11(const adapter&, const display_flag, shared_ptr<display>&) noexcept;
}

error_type display::create(shared_ptr<display>& output, const adapter& adapter, const display_flag flags) noexcept
{
    if (!adapter.data)
        return make_stdlib_error(std::errc::invalid_argument);

    if (adapter.data->flags() & adapter_flag::d3d12)
        return display_create_d3d12(adapter, flags, output);
    else if (adapter.data->flags() & adapter_flag::d3d11)
        return display_create_d3d11(adapter, flags, output);
    else
        return make_stdlib_error(std::errc::invalid_argument);
}