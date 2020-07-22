#pragma once

#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_render.hpp>

struct IDXGIResource;

class icy::render_texture::data_type
{
public:
    void destroy() noexcept;
public:
    void* _unused = nullptr;
    std::atomic<uint32_t> ref = 1;
    icy::com_ptr<IDXGIResource> dxgi;
    icy::weak_ptr<render_factory> factory;
    icy::window_size size;
};