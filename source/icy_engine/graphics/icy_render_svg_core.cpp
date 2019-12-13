#include "icy_render_svg_core.hpp"
#pragma warning(push)
#pragma warning(disable:4061)
#pragma warning(disable:4263)
#pragma warning(disable:4264)
#pragma warning(disable:4365)
#include <d2d1_3helper.h>
#include <dwrite_3.h>
#pragma warning(pop)
#include "../../external/src/nanosvg/nanosvg.h"
using namespace icy;

#define ICY_DWRITE_ERROR(HR) if (const auto hr_ = HR){ m_error = make_system_error(hr_); return hr_; }

ICY_STATIC_NAMESPACE_BEG
icy::global_heap_type* g_alloc = &icy::detail::global_heap;
class render_svg_tesselation_sink : public ID2D1TessellationSink
{
private:
	HRESULT __stdcall QueryInterface(REFIID, void**) noexcept override
	{
		return E_NOTIMPL;
	}
	ULONG __stdcall Release() noexcept override
	{
		return 0;
	}
	ULONG __stdcall AddRef() noexcept override
	{
		return 0;
	}
	void __stdcall AddTriangles(const D2D1_TRIANGLE* triangles, uint32_t count) noexcept override
	{
		if (m_error)
			return;

		const auto ptr = reinterpret_cast<const render_d2d_triangle*>(triangles);
		m_error = m_triangles.append(ptr, ptr + count);
	}
	HRESULT __stdcall Close() noexcept override
	{
		return S_OK;
	}
public:
	error_type m_error;
	array<render_d2d_triangle> m_triangles;
};
class render_svg_text_stack
{
public:
	error_type initialize(const render_svg_font_data& style) noexcept;
	error_type push(const render_svg_font_data& data) noexcept;
	void pop(const render_svg_font_data& data) noexcept;
public:
	DWRITE_TEXT_ALIGNMENT text_align = {};
	DWRITE_PARAGRAPH_ALIGNMENT paragraph_align = {};
	uint32_t line_spacing = 0u;
	DWRITE_TRIMMING_GRANULARITY trimming = {};
	array<DWRITE_FONT_WEIGHT> weights;
	array<DWRITE_FONT_STRETCH> stretches;
	array<array<wchar_t>> names;
	array<DWRITE_FONT_STYLE> styles;
	array<float> sizes;
	uint32_t strikethrough = 0;
	uint32_t underline = 0;
private:
	any_system_acqrel_pointer<render_svg_system> m_render;
};
class render_svg_text_renderer : public IDWriteTextRenderer
{
public:
	render_svg_text_renderer(render_svg_system& render, const render_d2d_matrix& transform, const color color) noexcept : 
        m_render(render), m_transform(transform), m_color(color)
	{

	}
	ULONG STDMETHODCALLTYPE AddRef() noexcept
	{
		return 0;
	}
	ULONG STDMETHODCALLTYPE Release() noexcept
	{
		return 0;
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) noexcept
	{
		return E_NOINTERFACE;
	}
	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* result) noexcept
	{
		*result = FALSE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* result) noexcept
	{
		*result = { 1, 0, 0, 1, 0, 0 };
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* result) noexcept
	{
		*result = 1.0F;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) noexcept
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown* effect) noexcept;
	error_type error() const noexcept
	{
		return m_error;
	}
	array<render_d2d_vertex>& vertices() noexcept
	{
		return m_vertices;
	}
private:
	render_svg_system& m_render;
	const render_d2d_matrix m_transform;
    const color m_color;
	array<render_d2d_vertex> m_vertices;
	error_type m_error;
};
ICY_STATIC_NAMESPACE_END

error_type render_svg_system::initialize() noexcept
{
	ICY_ERROR(m_d2d1_lib.initialize());
	ICY_ERROR(m_dwrite_lib.initialize());

	using d2d1_create_factory_type = HRESULT(WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);

	if (const auto func = m_d2d1_lib.find<d2d1_create_factory_type>("D2D1CreateFactory"))
	{
		D2D1_FACTORY_OPTIONS options = {};
#if _DEBUG
		options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		ICY_COM_ERROR(func(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &options, reinterpret_cast<void**>(&m_d2d1_factory)));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}

	if (const auto func = ICY_FIND_FUNC(m_dwrite_lib, DWriteCreateFactory))
	{
		com_ptr<IUnknown> any_factory;
		ICY_COM_ERROR(func(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &any_factory));
		ICY_COM_ERROR(any_factory->QueryInterface(&m_dwrite_factory));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}

#if 0
	using D2D1_func_type = HRESULT(WINAPI*)(IDXGIDevice*, const D2D1_CREATION_PROPERTIES*, ID2D1Device * *);
	if (const auto func = lib.find<D2D1_func_type>("D2D1CreateDevice"))
	{
		D2D1_CREATION_PROPERTIES d2d1_options = {};
#if _DEBUG
		d2d1_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		d2d1_options.threadingMode = D2D1_THREADING_MODE_MULTI_THREADED;
		d2d1_options.options = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;

		com_ptr<ID2D1Device> any_device;
		ICY_COM_ERROR(func(data.d3d11.dxgi, &d2d1_options, &any_device));
		ICY_COM_ERROR(any_device->QueryInterface(&device));

		com_ptr<ID2D1Factory> any_factory;
		any_device->GetFactory(&any_factory);
		ICY_COM_ERROR(any_factory->QueryInterface(&factory));
		ICY_COM_ERROR(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &context));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}
#endif
	return {};
}
float render_svg_system::dpi() const noexcept
{
	auto user32_lib = "user32"_lib;
	auto gdi_lib = "gdi32"_lib;
	const auto default_dpi = 96.0F;

	if (user32_lib.initialize() || gdi_lib.initialize())
		return default_dpi;

	const auto func_acquire = ICY_FIND_FUNC(user32_lib, GetDC);
	const auto func_release = ICY_FIND_FUNC(user32_lib, ReleaseDC);
	const auto func_devcaps = ICY_FIND_FUNC(gdi_lib, GetDeviceCaps);
	if (!func_acquire || !func_release || !func_devcaps)
		return default_dpi;

	const auto screen = func_acquire(nullptr);
	ICY_SCOPE_EXIT{ func_release(nullptr, screen); };
	if (const auto dpi = func_devcaps(screen, LOGPIXELSY))
		return static_cast<float>(dpi);

	return default_dpi;
}

error_type render_svg_font::enumerate(array<string>& fonts) noexcept
{
	any_system_acqrel_pointer<render_svg_system> render;
	ICY_ERROR(render.initialize());
	
	com_ptr<IDWriteFontCollection> collection;
	ICY_COM_ERROR(render->dwrite().GetSystemFontCollection(&collection));

	const auto count = collection->GetFontFamilyCount();
	array<string> new_fonts;
	ICY_ERROR(new_fonts.reserve(count));

	for (auto k = 0u; k < count; ++k)
	{
		com_ptr<IDWriteFontFamily> family;
		ICY_COM_ERROR(collection->GetFontFamily(k, &family));

		auto names = com_ptr<IDWriteLocalizedStrings>{};
		ICY_COM_ERROR(family->GetFamilyNames(&names));

		auto index = 0u;
		auto exists = FALSE;
		ICY_COM_ERROR(names->FindLocaleName(L"en-US", &index, &exists));

		if (exists)
		{
			auto length = 0u;
			ICY_COM_ERROR(names->GetStringLength(index, &length));
			array<wchar_t> wstr;
			ICY_ERROR(wstr.resize(length + 1));
			ICY_COM_ERROR(names->GetString(index, wstr.data(), length + 1));

			string new_font;
			ICY_ERROR(to_string(wstr, new_font));
			ICY_ERROR(new_fonts.push_back(std::move(new_font)));
		}
	}
	fonts = std::move(new_fonts);
	return {};
}
render_svg_font::render_svg_font(const render_svg_font& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1);
}
void render_svg_font::shutdown() noexcept
{
	any_system_shutdown_ref(data);
}
error_type render_svg_font::initialize(const render_svg_font_data& base) noexcept
{
	data_type* new_data = nullptr;
	ICY_ERROR(any_system_initialize(new_data, base));
	shutdown();
	data = new_data;
	return {};
}
error_type render_svg_font::data_type::initialize(const render_svg_font_data& font) noexcept
{
	ICY_ERROR(m_render.initialize());

	render_svg_text_stack stack;
	ICY_ERROR(stack.initialize(font));

	ICY_COM_ERROR(m_render->dwrite().CreateTextFormat(stack.names.back().data(),
		nullptr, stack.weights.back(), stack.styles.back(), stack.stretches.back(), stack.sizes.back(), L"en-US", &m_format));
	ICY_COM_ERROR(m_format->SetLineSpacing
	(
		stack.line_spacing ? DWRITE_LINE_SPACING_METHOD_UNIFORM : DWRITE_LINE_SPACING_METHOD_DEFAULT,
		static_cast<float>(stack.line_spacing) * 1.0F,
		static_cast<float>(stack.line_spacing) * 0.8F
	));
	ICY_COM_ERROR(m_format->SetParagraphAlignment(stack.paragraph_align));
	ICY_COM_ERROR(m_format->SetTextAlignment(stack.text_align));
	if (stack.trimming)
	{
		DWRITE_TRIMMING desc{ stack.trimming };
		ICY_COM_ERROR(m_format->SetTrimming(&desc, nullptr));
	}
	return {};
}
render_svg_geometry::render_svg_geometry(const render_svg_geometry& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1, std::memory_order_release);
}
void render_svg_geometry::shutdown() noexcept
{
	any_system_shutdown_ref(data);
}
error_type render_svg_geometry::initialize(const render_d2d_matrix& transform, const color color, const float width, const const_array_view<render_d2d_vector> shape) noexcept
{
	data_type* new_data = nullptr;
	ICY_ERROR(any_system_initialize(new_data, transform, color, width, shape));
	shutdown();
	data = new_data;
	return {};
}
error_type render_svg_geometry::initialize(const render_d2d_matrix& transform, const color color, const render_svg_font font, const string_view text) noexcept
{
	data_type* new_data = nullptr;
	ICY_ERROR(any_system_initialize(new_data, transform, color, font, text));
	shutdown();
	data = new_data;
	return {};
}
error_type render_svg_geometry::initialize(const render_d2d_matrix& transform, const string_view text) noexcept
{
	data_type* new_data = nullptr;
	ICY_ERROR(any_system_initialize(new_data, transform, text));
	shutdown();
	data = new_data;
	return {};
}
error_type render_svg_geometry::data_type::initialize(const render_d2d_matrix& transform, const color color, const float width, const const_array_view<render_d2d_vector> shape) noexcept
{
	ICY_ERROR(m_render.initialize());

	com_ptr<ID2D1PathGeometry> geometry;
	ICY_COM_ERROR(m_render->d2d1().CreatePathGeometry(&geometry));
	com_ptr<ID2D1GeometrySink> sink;
	ICY_COM_ERROR(geometry->Open(&sink));

	const auto begin = width ? D2D1_FIGURE_BEGIN_HOLLOW : D2D1_FIGURE_BEGIN_FILLED;
	sink->BeginFigure({ shape[0].x, shape[0].y }, begin);
	for (auto k = 1u; k < shape.size(); ++k)
		sink->AddLine({ shape[k].x, shape[k].y });
	sink->EndFigure(begin == D2D1_FIGURE_BEGIN_FILLED ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
	ICY_COM_ERROR(sink->Close());

	array<render_d2d_triangle> mesh;
	if (width)
	{
		com_ptr<ID2D1PathGeometry> widen;
		ICY_COM_ERROR(m_render->d2d1().CreatePathGeometry(&widen));
		sink = nullptr;
		ICY_COM_ERROR(widen->Open(&sink));
		ICY_COM_ERROR(geometry->Widen(width, nullptr, reinterpret_cast<const D2D1_MATRIX_3X2_F*>(&transform),
			D2D1_DEFAULT_FLATTENING_TOLERANCE, sink));
		ICY_COM_ERROR(sink->Close());
		ICY_ERROR(render_svg_tesselate({}, *widen, mesh));
	}
	else
	{
		ICY_ERROR(render_svg_tesselate(transform, *geometry, mesh));
	}
	NSVGpaint paint = {};
	paint.type = NSVGpaintType::NSVG_PAINT_COLOR;
	paint.color = color.bgra;
	ICY_ERROR(render_svg_rasterize(mesh, paint, 1, m_vertices));
	return {};
}
error_type render_svg_geometry::data_type::initialize(const render_d2d_matrix& transform, const color color, render_svg_font font, const string_view text) noexcept
{
	ICY_ERROR(m_render.initialize());

	if (!font.data)
		ICY_ERROR(font.initialize({}));

	array<wchar_t> wtext;
	ICY_ERROR(to_utf16(text, wtext));
	ICY_COM_ERROR(m_render->dwrite().CreateTextLayout(wtext.data(), uint32_t(wtext.size()),
		&font.data->format(), FLT_MAX, FLT_MAX, &m_layout));

	render_svg_text_renderer text_render(*m_render.operator->(), transform, color);
	ICY_COM_ERROR(m_layout->Draw(nullptr, &text_render, 0, 0));
	ICY_ERROR(text_render.error());
	m_vertices = std::move(text_render.vertices());
	return {};
}
error_type render_svg_geometry::data_type::initialize(const render_d2d_matrix& transform, const string_view text) noexcept
{
    any_system_acqrel_pointer<render_svg_system> render;
    ICY_ERROR(render.initialize());

    string str;
    ICY_ERROR(to_string(text, str));

    NSVGimage* image = nullptr;
    if (const auto error = nsvgParse(str.bytes().data(), &image))
        return make_stdlib_error(static_cast<std::errc>(error));
    ICY_SCOPE_EXIT{ nsvgDelete(image); };

    auto shape = image->shapes;
    while (shape)
    {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE) || !shape->opacity)
        {
            shape = shape->next;
            continue;
        }

        com_ptr<ID2D1PathGeometry> path_geometry;
        ICY_COM_ERROR(render->d2d1().CreatePathGeometry(&path_geometry));
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

            sink->AddBeziers(reinterpret_cast<D2D1_BEZIER_SEGMENT*>(path->pts + 2), count);
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
            ICY_COM_ERROR(render->d2d1().CreateStrokeStyle(props, shape->strokeDashArray, shape->strokeDashCount, &stroke));
        }

        if (shape->fill.type != NSVGpaintType::NSVG_PAINT_NONE)
        {
            array<render_d2d_triangle> mesh;
            ICY_ERROR(render_svg_tesselate(transform, *path_geometry, mesh));
            ICY_ERROR(render_svg_rasterize(mesh, shape->fill, shape->opacity, m_vertices));
        }
        if (shape->stroke.type != NSVGpaintType::NSVG_PAINT_NONE && shape->strokeWidth)
        {
            com_ptr<ID2D1PathGeometry> outline;
            ICY_COM_ERROR(render->d2d1().CreatePathGeometry(&outline));
            ICY_COM_ERROR(outline->Open(&sink));
            const auto hr = path_geometry->Widen(shape->strokeWidth,
                stroke, D2D1::Matrix3x2F::Identity(), D2D1_DEFAULT_FLATTENING_TOLERANCE, sink);
            ICY_COM_ERROR(sink->Close());
            ICY_COM_ERROR(hr);
            array<render_d2d_triangle> mesh;
            ICY_ERROR(render_svg_tesselate(transform, *outline, mesh));
            ICY_ERROR(render_svg_rasterize(mesh, shape->stroke, shape->opacity, m_vertices));
        }
        shape = shape->next;
    }

    return {};
}

error_type render_svg_text_stack::initialize(const render_svg_font_data& style) noexcept
{
	ICY_ERROR(m_render.initialize());
	
	auto ncm = NONCLIENTMETRICS{ sizeof(NONCLIENTMETRICS) };
	if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
		return last_system_error();

	auto gdi = com_ptr<IDWriteGdiInterop>{};
	ICY_COM_ERROR(m_render->dwrite().GetGdiInterop(&gdi));
	auto ptr = &ncm.lfMessageFont;
	com_ptr<IDWriteFont> font;
	ICY_COM_ERROR(gdi->CreateFontFromLOGFONT(ptr, &font));

	if (ptr->lfHeight > 0)
	{
		DWRITE_FONT_METRICS metrics = {};
		font->GetMetrics(&metrics);
		ptr->lfHeight *= metrics.designUnitsPerEm;
		ptr->lfHeight /= (metrics.ascent + metrics.descent);
	}

	com_ptr<IDWriteFontFamily> family;
	ICY_COM_ERROR(font->GetFontFamily(&family));

	com_ptr<IDWriteLocalizedStrings> localized{};
	ICY_COM_ERROR(family->GetFamilyNames(&localized));

	auto index = 0u;
	auto exists = FALSE;
	auto length = 0u;
	ICY_COM_ERROR(localized->FindLocaleName(L"en-US", &index, &exists));
	ICY_COM_ERROR(localized->GetStringLength(index, &length));

	array<wchar_t> name;
	ICY_ERROR(name.resize(length + 1));
	ICY_COM_ERROR(localized->GetString(index, name.data(), length + 1));

	const auto dpi = m_render->dpi();
	text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
	paragraph_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
	line_spacing = 0u;
	trimming = DWRITE_TRIMMING_GRANULARITY_NONE;
	ICY_ERROR(weights.push_back(font->GetWeight()));
	ICY_ERROR(stretches.push_back(font->GetStretch()));
	ICY_ERROR(names.push_back(std::move(name)));
	ICY_ERROR(styles.push_back(font->GetStyle()));
	ICY_ERROR(sizes.push_back(std::abs(ptr->lfHeight)* dpi / 72.0F));
	strikethrough = ptr->lfStrikeOut;
	underline = ptr->lfUnderline;

	return push(style);
}
error_type render_svg_text_stack::push(const render_svg_font_data& data) noexcept
{
	if (!data.name.empty())
	{
		array<wchar_t> name;
		ICY_ERROR(to_utf16(data.name, name));
		ICY_ERROR(names.push_back(std::move(name)));
	}

	color color;
	for (auto&& flag : data.flags)
	{
		switch (flag.type)
		{
		case render_svg_text_flag::font_italic:
			ICY_ERROR(styles.push_back(DWRITE_FONT_STYLE_ITALIC));
			break;
		case render_svg_text_flag::font_normal:
			ICY_ERROR(styles.push_back(DWRITE_FONT_STYLE_NORMAL));
			break;
		case render_svg_text_flag::font_oblique:
			ICY_ERROR(styles.push_back(DWRITE_FONT_STYLE_OBLIQUE));
			break;

		case render_svg_text_flag::font_size:
			ICY_ERROR(sizes.push_back(static_cast<float>(flag.value)));
			break;
		case render_svg_text_flag::font_stretch:
			ICY_ERROR(stretches.push_back(static_cast<DWRITE_FONT_STRETCH>(flag.value)));
			break;
		case render_svg_text_flag::font_weight:
			ICY_ERROR(weights.push_back(static_cast<DWRITE_FONT_WEIGHT>(flag.value)));
			break;

		case render_svg_text_flag::strikethrough:
			++strikethrough;
			break;
		case render_svg_text_flag::underline:
			++underline;
			break;

		case render_svg_text_flag::line_spacing:
			line_spacing = flag.value;
			break;

		case render_svg_text_flag::align_center:
			text_align = DWRITE_TEXT_ALIGNMENT_CENTER;
			break;
		case render_svg_text_flag::align_leading:
			text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
			break;
		case render_svg_text_flag::align_trailing:
			text_align = DWRITE_TEXT_ALIGNMENT_TRAILING;
			break;

		case render_svg_text_flag::par_align_center:
			paragraph_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
			break;
		case render_svg_text_flag::par_align_far:
			paragraph_align = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
			break;
		case render_svg_text_flag::par_align_near:
			paragraph_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
			break;

		case render_svg_text_flag::trimming_char:
			trimming = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
			break;
		case render_svg_text_flag::trimming_none:
			trimming = DWRITE_TRIMMING_GRANULARITY_NONE;
			break;
		case render_svg_text_flag::trimming_word:
			trimming = DWRITE_TRIMMING_GRANULARITY_WORD;
			break;
		}
	}
	return {};
}
void render_svg_text_stack::pop(const render_svg_font_data& data) noexcept
{
	if (!data.name.empty())
		names.pop_back();

	for (auto&& flag : data.flags)
	{
		switch (flag.type)
		{
		case render_svg_text_flag::font_italic:
		case render_svg_text_flag::font_normal:
		case render_svg_text_flag::font_oblique:
			styles.pop_back();
			break;

		case render_svg_text_flag::font_size:
			sizes.pop_back();
			break;
		case render_svg_text_flag::font_stretch:
			stretches.pop_back();
			break;
		case render_svg_text_flag::font_weight:
			weights.pop_back();
			break;

		case render_svg_text_flag::strikethrough:
			--strikethrough;
			break;
		case render_svg_text_flag::underline:
			--underline;
			break;
		}
	}
}
HRESULT STDMETHODCALLTYPE render_svg_text_renderer::DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) noexcept
{
	if (!run->glyphCount || m_error)
		return S_OK;

	com_ptr<ID2D1PathGeometry> path;
	com_ptr<ID2D1GeometrySink> sink;
	ICY_DWRITE_ERROR(m_render.d2d1().CreatePathGeometry(&path));
	ICY_DWRITE_ERROR(path->Open(&sink));
	const auto hr = run->fontFace->GetGlyphRunOutline(run->fontEmSize,
		run->glyphIndices, run->glyphAdvances, run->glyphOffsets,
		run->glyphCount, run->isSideways, run->bidiLevel % 2, sink);
	ICY_DWRITE_ERROR(sink->Close());
	ICY_DWRITE_ERROR(hr);
	sink = nullptr;

	array<render_d2d_triangle> mesh;
	if (m_error = render_svg_tesselate(m_transform * render_d2d_matrix(x, y), *path, mesh))
		return S_OK;

	NSVGpaint paint = {};
	paint.type = NSVGpaintType::NSVG_PAINT_COLOR;
	paint.color = m_color.bgra;
	if (m_error = render_svg_rasterize(mesh, paint, 1, m_vertices))
		return S_OK;

	return S_OK;
}
error_type icy::render_svg_tesselate(const render_d2d_matrix& matrix, ID2D1Geometry& geometry, array<render_d2d_triangle>& mesh) noexcept
{
	render_svg_tesselation_sink sink;
	ICY_COM_ERROR(geometry.Tessellate(reinterpret_cast<const D2D1_MATRIX_3X2_F*>(&matrix), &sink));
	ICY_ERROR(sink.m_error);
	mesh = std::move(sink.m_triangles);
	return {};
}
error_type icy::render_svg_rasterize(const_array_view<render_d2d_triangle> mesh, const NSVGpaint& paint, const float, array<render_d2d_vertex>& vertices) noexcept
{
	auto vcount = vertices.size();
	ICY_ERROR(vertices.resize(vertices.size() + mesh.size() * 3));

	auto color = 0u;
	if (paint.type == NSVGpaintType::NSVG_PAINT_COLOR)
	{
		color = paint.color;
	}

	for (auto&& triangle : mesh)
	{
		for (auto n = 0u; n < 3; ++n)
		{
			auto& vertex = vertices[vcount++];
			vertex.coord = { triangle.vec[n].x, triangle.vec[n].y, 0 };
			vertex.color = paint.color;
		}
	}
	return {};
}

#define NANOSVG_IMPLEMENTATION 1
#define NANOSVG_ALL_COLOR_KEYWORDS 1

static void* nsvg__realloc(void* ptr, size_t size)
{
	return g_alloc->realloc(ptr, size, g_alloc->user);
}
#include "../../external/src/nanosvg/nanosvg.h"