#include <d2d1_3.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dwrite_3.h>
#include <icy_engine/core/icy_queue.hpp>
#include <icy_engine/core/icy_map.hpp>
#include "icy_render_factory.hpp"
#include "icy_render_svg.hpp"
#include "icy_adapter.hpp"
#include "icy_texture.hpp"
#include "icy_render_commands.hpp"

using namespace icy;

render_commands_2d::render_commands_2d(const render_commands_2d& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_commands_2d::~render_commands_2d() noexcept
{
    if (data)
        data->destroy();
}
error_type render_commands_2d::draw(const render_svg_geometry& svg, const render_d2d_matrix& transform, const float layer, const render_texture texture) noexcept
{
    if (!data || !svg.data)
        return make_stdlib_error(std::errc::invalid_argument);

    render_command_2d cmd;
    cmd.transform = transform;
    cmd.layer = layer;
    cmd.geometry = svg;
    return data->queue.push(std::move(cmd));
}
error_type render_commands_2d::draw(const render_svg_text& text, const render_d2d_matrix& transform, const float layer) noexcept
{
    if (!data || !text.data)
        return make_stdlib_error(std::errc::invalid_argument);

    auto render = shared_ptr<render_factory_data>(data->factory);
    if (!render)
        return make_stdlib_error(std::errc::invalid_argument);

    render_command_2d cmd;
    cmd.transform = transform;
    cmd.layer = layer;
    cmd.text = text.data->layout;
    cmd.color = text.data->color;
    return data->queue.push(std::move(cmd));
}
error_type render_commands_2d::close(const window_size size, render_texture& texture) noexcept
{
    if (!data || size.x == 0 || size.y == 0)
        return make_stdlib_error(std::errc::invalid_argument);
    ICY_ERROR(data->exec(size, texture));
    return error_type();
}

error_type render_commands_2d::data_type::exec(const window_size size, render_texture& output) noexcept
{
    auto render = shared_ptr<render_factory_data>(factory);
    if (!render)
        return make_stdlib_error(std::errc::invalid_argument);
    
    if (!d3d11)
        ICY_ERROR(render->create_device(d3d11));
    if (!device)
        ICY_ERROR(render->create_device(*d3d11, device));
    if (!context)
        ICY_COM_ERROR(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &context));

    ICY_ERROR(render->create_texture(*d3d11, size, output));

    HANDLE handle = nullptr;
    ICY_COM_ERROR(output.data->dxgi->GetSharedHandle(&handle));
    com_ptr<ID3D11Texture2D> d3d11_texture;
    ICY_COM_ERROR(d3d11->OpenSharedResource(handle, IID_PPV_ARGS(&d3d11_texture)));

    com_ptr<IDXGISurface> dxgi;
    ICY_COM_ERROR(d3d11_texture->QueryInterface(&dxgi));
    
    auto pixel = D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, pixel);
        
    com_ptr<ID2D1Bitmap1> bitmap;
    ICY_COM_ERROR(context->CreateBitmapFromDxgiSurface(dxgi, props, &bitmap));

    map<float, array<render_command_2d>> vec;
    while (true)
    {
        render_command_2d cmd;
        if (!queue.pop(cmd))
            break;

        auto layer = vec.find(cmd.layer);
        if (layer == vec.end())
            ICY_ERROR(vec.insert(cmd.layer, array<render_command_2d>(), &layer));

        ICY_ERROR(layer->value.push_back(std::move(cmd)));
    }

    context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    //context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    context->SetTarget(bitmap);
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0, 0, 0, 0));
    
    for (const auto& layer : vec)
    {
        for (auto&& cmd : layer.value)
        {
            context->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(&cmd.transform));
            if (cmd.geometry.data)
            {
                ICY_ERROR(cmd.geometry.data->render(*context));
            }
            if (cmd.text)
            {
                com_ptr<ID2D1SolidColorBrush> brush;
                ICY_COM_ERROR(context->CreateSolidColorBrush(D2D1::ColorF(cmd.color.to_rgb(), cmd.color.a / 255.0f), &brush));
                context->DrawTextLayout(D2D1::Point2F(), cmd.text, brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }
        }
    }
    ICY_COM_ERROR(context->EndDraw());
    context->SetTarget(nullptr);
    
    com_ptr<ID3D11DeviceContext> d3d11_context;
    d3d11->GetImmediateContext(&d3d11_context);
    d3d11_context->Flush();

    return error_type();
}
void render_commands_2d::data_type::destroy() noexcept
{
    if (ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (auto render = shared_ptr<render_factory_data>(factory))
        {
            render->destroy(*this);
        }
        else
        {
            allocator_type::destroy(this);
            allocator_type::deallocate(this);
        }
    }
}

error_type render_factory_data::create_device(ID3D11Device& d3d11_device, com_ptr<ID2D1Device>& device) noexcept
{
    if (const auto func = m_lib_d2d1.find("D2D1CreateDevice"))
    { 
        auto func_type = reinterpret_cast<HRESULT(WINAPI*)(IDXGIDevice*, const D2D1_CREATION_PROPERTIES*, ID2D1Device**)>(func);
        com_ptr<IDXGIDevice> dxgi_device;
        ICY_COM_ERROR(d3d11_device.QueryInterface(&dxgi_device));
        auto props = D2D1::CreationProperties(D2D1_THREADING_MODE_MULTI_THREADED,
            m_adapter.flags() & adapter_flags::debug ? D2D1_DEBUG_LEVEL_INFORMATION : D2D1_DEBUG_LEVEL_NONE,
            D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS);
        device = nullptr; 
        ICY_COM_ERROR(func_type(dxgi_device, &props, &device));
        return error_type();
    }
    return make_stdlib_error(std::errc::function_not_supported);
}
error_type render_factory_data::open_commands_2d(render_commands_2d& commands) noexcept
{
    if (!commands.data)
    {
        commands.data = static_cast<render_commands_2d::data_type*>(m_cmd_2d.pop());
        if (commands.data)
            commands.data->ref.fetch_add(1, std::memory_order_release);
    }
    if (!commands.data)
    {
        commands.data = allocator_type::allocate<render_commands_2d::data_type>(1);
        if (!commands.data)
            return make_stdlib_error(std::errc::invalid_argument);
        allocator_type::construct(commands.data);
        commands.data->factory = make_shared_from_this(this);
    }
    return error_type();
}

