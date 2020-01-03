#define CINTERFACE 1
#define D3D11_NO_HELPERS 1
#include "icy_dxgi.hpp"
#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <dxgi.h>
#include <d3d11.h>
#pragma comment(lib, "windowscodecs")
#pragma comment(lib, "dxguid")

using namespace icy;

error_type dxgi_offset(size_t& offset) noexcept
{
    auto user32 = "user32"_lib;
    auto dxgi = "dxgi"_lib;
    auto d3d11 = "d3d11"_lib;

    ICY_ERROR(user32.initialize());
    ICY_ERROR(dxgi.initialize());
    ICY_ERROR(d3d11.initialize());

    com_ptr<IDXGIFactory> factory;
    if (const auto func = ICY_FIND_FUNC(dxgi, CreateDXGIFactory))
    {
        ICY_COM_ERROR(func(IID_IDXGIFactory, reinterpret_cast<void**>(&factory)));
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);

    com_ptr<ID3D11Device> device;
    if (const auto func = ICY_FIND_FUNC(d3d11, D3D11CreateDevice))
    {
        auto level = D3D_FEATURE_LEVEL_11_0;
        ICY_COM_ERROR(func(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
            0, D3D11_SDK_VERSION, &device, &level, nullptr));
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);

    HWND window = nullptr;
    const auto window_create = ICY_FIND_FUNC(user32, CreateWindowExW);
    const auto window_delete = ICY_FIND_FUNC(user32, DestroyWindow);
    if (window_create && window_delete)
    {
        window = window_create(0, L"Static", L"DXGIWindow", 0,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            nullptr, nullptr, nullptr, nullptr);
    }
    else
        return make_stdlib_error(std::errc::function_not_supported);

    if (!window)
        return last_system_error();

    ICY_SCOPE_EXIT{ window_delete(window); };

    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.OutputWindow = window;

    com_ptr<IDXGISwapChain> chain;
    ICY_COM_ERROR(factory->lpVtbl->CreateSwapChain(factory,
        reinterpret_cast<IUnknown*>(static_cast<ID3D11Device*>(device)), &desc, &chain));

    offset =
        reinterpret_cast<const uint8_t*>(chain->lpVtbl->Present) -
        reinterpret_cast<const uint8_t*>(GetModuleHandleW(L"dxgi.dll"));
    return {};
}

error_type dxgi_window(IDXGISwapChain& chain, icy::string& wname, icy::window_size& wsize, HWND__*& hwnd)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    ICY_COM_ERROR(chain.lpVtbl->GetDesc(&chain, &desc));
    hwnd = desc.OutputWindow;
    wsize = { desc.BufferDesc.Width, desc.BufferDesc.Height };
    
    const auto lib = GetModuleHandleW(L"user32");
    if (!lib)
        return last_system_error();

    const auto window_text_length = reinterpret_cast<decltype(&GetWindowTextLengthW)>(GetProcAddress(lib, "GetWindowTextLengthW"));
    const auto window_text = reinterpret_cast<decltype(&GetWindowTextW)>(GetProcAddress(lib, "GetWindowTextW"));
    if (!window_text_length || !window_text)
        return make_stdlib_error(std::errc::function_not_supported);
    
    const auto length = window_text_length(desc.OutputWindow);
    array<wchar_t> buffer;
    ICY_ERROR(buffer.resize(size_t(length) + 1));
    window_text(desc.OutputWindow, buffer.data(), length + 1);
    ICY_ERROR(to_string(buffer, wname));

    return {};
}
error_type dxgi_copy(IDXGISwapChain& chain, const const_array_view<rectangle> src, array<matrix<color>>& dst) noexcept
{
    if (src.empty())
        return {};

    ICY_ERROR(dst.resize(src.size()));
    for (auto k = 0u; k < src.size(); ++k)
    {
        dst[k] = matrix<color>(src[k].h, src[k].w);
        if (dst[k].size() != src[k].w * src[k].h)
            return make_stdlib_error(std::errc::not_enough_memory);
    }

    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    com_ptr<ID3D11Texture2D> source;
    com_ptr<ID3D11Texture2D> target;
    ICY_COM_ERROR(chain.lpVtbl->GetDevice(&chain, IID_ID3D11Device, reinterpret_cast<void**>(&device)));
    ICY_COM_ERROR(chain.lpVtbl->GetBuffer(&chain, 0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&source)));
    if (source && device)
    {
        auto desc = D3D11_TEXTURE2D_DESC{ };
        source->lpVtbl->GetDesc(source, &desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        ICY_COM_ERROR(device->lpVtbl->CreateTexture2D(device, &desc, nullptr, &target));
        device->lpVtbl->GetImmediateContext(device, &context);
    }
    if (context)
    {
        context->lpVtbl->CopyResource(context,
            reinterpret_cast<ID3D11Resource*>(static_cast<ID3D11Texture2D*>(target)),
            reinterpret_cast<ID3D11Resource*>(static_cast<ID3D11Texture2D*>(source)));
    }

    D3D11_MAPPED_SUBRESOURCE map = {};
    ICY_COM_ERROR(context->lpVtbl->Map(context,
        reinterpret_cast<ID3D11Resource*>(static_cast<ID3D11Texture2D*>(target)), 0, D3D11_MAP_READ, 0, &map));
    ICY_SCOPE_EXIT{ context->lpVtbl->Unmap(context,
        reinterpret_cast<ID3D11Resource*>(static_cast<ID3D11Texture2D*>(target)), 0); };
    
    for (auto k = 0_z; k < src.size(); ++k)
    {
        auto dst_ptr = dst[k].data();
        for (auto row = 0u; row < src[k].h; ++row)
        {
            auto src_ptr = static_cast<uint8_t*>(map.pData);
            src_ptr += map.RowPitch * (row + src[k].y);
            src_ptr += src[k].x * sizeof(color);
            for (auto col = 0u; col < src[k].w; ++col, ++dst_ptr, src_ptr += 4)
            {
                dst_ptr->array[0] = src_ptr[2];
                dst_ptr->array[1] = src_ptr[1];
                dst_ptr->array[2] = src_ptr[0];
                dst_ptr->array[3] = 0xFF;
            }
        }
    }
    return {};
}