#pragma once

#include <icy_engine/utility/icy_com.hpp>
#include <icy_engine/graphics/icy_render_svg.hpp>
#include <icy_engine/graphics/icy_render.hpp>

struct IDWriteTextFormat;
struct IDWriteTextLayout;
struct NSVGimage;
struct ID2D1RenderTarget;

class icy::render_svg_font::data_type
{
public:
    std::atomic<uint32_t> ref = 1;
    icy::weak_ptr<icy::render_factory> factory;
    icy::com_ptr<IDWriteTextFormat> format;
};
class icy::render_svg_text::data_type
{
public:
    std::atomic<uint32_t> ref = 1;
    icy::weak_ptr<icy::render_factory> factory;
    icy::com_ptr<IDWriteTextLayout> layout;
    icy::color color;
};
class icy::render_svg_geometry::data_type
{
public:
    icy::error_type render(ID2D1RenderTarget& context) noexcept;
public:
    std::atomic<uint32_t> ref = 1;
    icy::weak_ptr<icy::render_factory> factory;
    NSVGimage* image = nullptr;
};