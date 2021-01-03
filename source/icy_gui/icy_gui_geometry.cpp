/*#include "icy_render_svg.hpp"
#include "icy_render_factory.hpp"
#pragma warning(push)
#pragma warning(disable:4061)
#pragma warning(disable:4263)
#pragma warning(disable:4264)
#pragma warning(disable:4365)
#include <d2d1_3helper.h>
#pragma warning(pop)
#include "nanosvg.h"

using namespace icy;

render_svg_geometry::render_svg_geometry(const render_svg_geometry& rhs) noexcept : data(rhs.data)
{
    if (data)
        data->ref.fetch_add(1, std::memory_order_release);
}
render_svg_geometry::~render_svg_geometry() noexcept
{
    if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        nsvgDelete(data->image);
        allocator_type::destroy(data);
        allocator_type::deallocate(data);
    }
}

error_type render_svg_geometry::data_type::render(ID2D1RenderTarget& context) noexcept
{
    com_ptr<ID2D1Factory> factory;
    context.GetFactory(&factory);
    auto shape = image->shapes;
    while (shape)
    {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE) || !shape->opacity)
        {
            shape = shape->next;
            continue;
        }

        com_ptr<ID2D1PathGeometry> path_geometry;
        ICY_COM_ERROR(factory->CreatePathGeometry(&path_geometry));
        com_ptr<ID2D1GeometrySink> sink;
        ICY_COM_ERROR(path_geometry->Open(&sink));

        sink->SetFillMode(shape->fillRule == NSVG_FILLRULE_NONZERO ?
            D2D1_FILL_MODE::D2D1_FILL_MODE_ALTERNATE : D2D1_FILL_MODE_WINDING);

        auto path = shape->paths;
        while (path)
        {
            if (!path->npts)
            {
                path = path->next;
                continue;
            }
            sink->BeginFigure({ path->pts[0], path->pts[1] },
                shape->fill.type == NSVGpaintType::NSVG_PAINT_NONE ? D2D1_FIGURE_BEGIN_HOLLOW : D2D1_FIGURE_BEGIN_FILLED);
            const auto count = (path->npts - 1) / 3;

            sink->AddBeziers(reinterpret_cast<const D2D1_BEZIER_SEGMENT*>(path->pts + 2), count);
            sink->EndFigure(path->closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);

            path = path->next;
        }
        ICY_COM_ERROR(sink->Close());
        sink = nullptr;

        auto cap = D2D1_CAP_STYLE_FLAT;
        switch (shape->strokeLineCap)
        {
        case NSVG_CAP_ROUND:
            cap = D2D1_CAP_STYLE_ROUND;
            break;
        case NSVG_CAP_SQUARE:
            cap = D2D1_CAP_STYLE_SQUARE;
            break;
        }
        auto props = D2D1::StrokeStyleProperties(cap, cap, cap);
        switch (shape->strokeLineJoin)
        {
        case NSVG_JOIN_ROUND:
            props.lineJoin = D2D1_LINE_JOIN_ROUND;
            break;
        case NSVG_JOIN_BEVEL:
            props.lineJoin = D2D1_LINE_JOIN_BEVEL;
            break;
        }
        props.miterLimit = shape->miterLimit;
        props.dashOffset = shape->strokeDashOffset;

        if (shape->strokeDashCount)
            props.dashStyle = D2D1_DASH_STYLE_CUSTOM;

        com_ptr<ID2D1StrokeStyle> stroke;
        if (shape->strokeLineJoin || shape->strokeLineCap || shape->strokeDashCount)
        {
            ICY_COM_ERROR(factory->CreateStrokeStyle(props, shape->strokeDashArray, shape->strokeDashCount, &stroke));
        }

        auto make_brush = [&context](const NSVGpaint& nsvg, com_ptr<ID2D1Brush>& brush)
        {
            if (nsvg.type == NSVGpaintType::NSVG_PAINT_NONE)
            {
                return make_stdlib_error(std::errc::invalid_argument);
            }
            else if (nsvg.type == NSVGpaintType::NSVG_PAINT_COLOR)
            {
                com_ptr<ID2D1SolidColorBrush> solid;
                auto color = icy::color::from_rgba(nsvg.color);
                ICY_COM_ERROR(context.CreateSolidColorBrush(D2D1::ColorF(color.to_rgb(), color.a / 255.0f), &solid));
                brush = std::move(solid);
            }
            else
            {
                com_ptr<ID2D1GradientStopCollection> gradient;
                array<D2D1_GRADIENT_STOP> stops;
                for (auto k = 0; k < nsvg.gradient->nstops; ++k)
                {
                    const auto color = icy::color::from_rgba(nsvg.gradient->stops[k].color);
                    const auto stop = D2D1::GradientStop(nsvg.gradient->stops[k].offset,
                        D2D1::ColorF(color.to_rgb(), color.a / 255.0f));
                    ICY_ERROR(stops.push_back(stop));
                }
                ICY_COM_ERROR(context.CreateGradientStopCollection(stops.data(), uint32_t(stops.size()), &gradient));
                if (nsvg.type == NSVGpaintType::NSVG_PAINT_LINEAR_GRADIENT)
                {
                    const auto x1 = nsvg.gradient->xform[4];
                    const auto y1 = nsvg.gradient->xform[5];
                    const auto x2 = nsvg.gradient->xform[2] + x1;
                    const auto y2 = nsvg.gradient->xform[0] + y1;
                    auto props = D2D1::LinearGradientBrushProperties({ x1, y1 }, { x2, y2 });
                    com_ptr<ID2D1LinearGradientBrush> linear;
                    ICY_COM_ERROR(context.CreateLinearGradientBrush(props, gradient, &linear));
                    brush = std::move(linear);
                }
                else
                {
                    const auto r = nsvg.gradient->xform[0];
                    const auto cx = nsvg.gradient->xform[4];
                    const auto cy = nsvg.gradient->xform[5];
                    const auto rx = nsvg.gradient->fx * r;
                    const auto ry = nsvg.gradient->fy * r;
                    auto props = D2D1::RadialGradientBrushProperties( { cx, cy }, { 0, 0 }, rx, ry);
                    com_ptr<ID2D1RadialGradientBrush> radial;
                    ICY_COM_ERROR(context.CreateRadialGradientBrush(props, gradient, &radial));
                    brush = std::move(radial);
                }
                brush->SetTransform(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(nsvg.gradient->xform));
            }
            return error_type();
        };

        if (shape->fill.type != NSVGpaintType::NSVG_PAINT_NONE)
        {
            com_ptr<ID2D1Brush> brush;
            ICY_ERROR(make_brush(shape->fill, brush));
            brush->SetOpacity(shape->opacity);
            context.FillGeometry(path_geometry, brush);
        }
        if (shape->stroke.type != NSVGpaintType::NSVG_PAINT_NONE && shape->strokeWidth)
        {
            com_ptr<ID2D1Brush> brush;
            ICY_ERROR(make_brush(shape->stroke, brush));
            brush->SetOpacity(shape->opacity);
            context.DrawGeometry(path_geometry, brush, shape->strokeWidth, stroke);
        }
        shape = shape->next;
    }
    return error_type();
}

error_type render_factory_data::create_svg_geometry(const const_array_view<char> xml, render_svg_geometry& geometry) const noexcept
{
    if (xml.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    array<char> str_xml;
    ICY_ERROR(copy(xml, str_xml));
    ICY_ERROR(str_xml.push_back(0));

    NSVGimage* image = nullptr;
    if (const auto error = nsvgParse(str_xml.data(), &image))
        return make_stdlib_error(static_cast<std::errc>(error));
    ICY_SCOPE_EXIT{ nsvgDelete(image); };

    auto new_data = allocator_type::allocate<render_svg_geometry::data_type>(1);
    if (!new_data)
        return make_stdlib_error(std::errc::not_enough_memory);

    allocator_type::construct(new_data);
    new_data->factory = make_shared_from_this(this);
    new_data->image = image;
    image = nullptr;
    geometry = render_svg_geometry();
    geometry.data = new_data;
    return error_type();
}

#define NANOSVG_IMPLEMENTATION 1
#define NANOSVG_ALL_COLOR_KEYWORDS 1

static void* nsvg__realloc(void* ptr, size_t size)
{
    return icy::realloc(ptr, size);
}
#include "nanosvg.h"*/