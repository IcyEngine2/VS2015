#include "icy_texture.hpp"
#include "icy_render_factory.hpp"
#include <d3d11.h>

using namespace icy;

window_size render_texture::size() const noexcept
{
    return data ? data->size : window_size();
}
error_type render_texture::save(const render_d2d_rectangle_u& rect, matrix_view<color> colors) const noexcept
{
    if (!data)
        return make_stdlib_error(std::errc::invalid_argument);

    auto render = shared_ptr<render_factory>(data->factory);
    if (!render
        || rect.w != colors.cols()
        || rect.h != colors.rows() 
        || rect.w > data->size.x 
        || rect.h > data->size.y
        || rect.x > data->size.x - rect.w 
        || rect.y > data->size.y - rect.h)
        return make_stdlib_error(std::errc::invalid_argument);

    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;

    ICY_ERROR(static_cast<render_factory_data*>(render.get())->create_device(device));
    device->GetImmediateContext(&context);
    
    com_ptr<ID3D11Texture2D> gpu_texture;
    com_ptr<ID3D11Texture2D> cpu_texture;

    HANDLE handle = nullptr;
    ICY_COM_ERROR(data->dxgi->GetSharedHandle(&handle));
    ICY_COM_ERROR(device->OpenSharedResource(handle, IID_PPV_ARGS(&gpu_texture)));
    ICY_COM_ERROR(device->CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, 
        rect.w, rect.h, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ), nullptr, &cpu_texture));

    D3D11_BOX box = { rect.x, rect.y, 0, rect.x + rect.w, rect.y + rect.h, 1 };
    context->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, gpu_texture, 0, &box);

    D3D11_MAPPED_SUBRESOURCE map;
    ICY_COM_ERROR(context->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &map));
    ICY_SCOPE_EXIT{ context->Unmap(cpu_texture, 0); };

    for (auto row = 0u; row < colors.rows(); ++row)
    {
        auto ptr = reinterpret_cast<const uint32_t*>(static_cast<char*>(map.pData) + row * map.RowPitch);
        for (auto col = 0u; col < colors.cols(); ++col)
            colors.at(row, col) = color::from_rgba(ptr[col]);        
    }
    context->Flush();

    return error_type();
}

render_texture::render_texture(const render_texture& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_texture::~render_texture() noexcept
{
    if (data)
        data->destroy();
}
void render_texture::data_type::destroy() noexcept
{
    if (ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        auto render = shared_ptr<render_factory>(factory);
        if (auto factory_data = static_cast<render_factory_data*>(render.get()))
        {
            factory_data->destroy(*this);
        }
        else
        {
            allocator_type::destroy(this);
            allocator_type::deallocate(this);
        }
    }
}