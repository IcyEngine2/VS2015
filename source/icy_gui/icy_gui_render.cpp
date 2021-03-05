#include "icy_gui_render.hpp"
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/graphics/icy_adapter.hpp>
#include <dwrite_3.h>
#include <d2d1_3.h>
#include <d3d11.h>

using namespace icy;

static D2D1::ColorF d2d_color(const icy::color color) noexcept
{
	return D2D1::ColorF(color.to_rgb(), color.a / 255.0f);
}

IDWriteTextLayout* gui_text::operator->() const noexcept
{
	return static_cast<IDWriteTextLayout*>(value.get());
}

error_type gui_texture::initialize() noexcept
{
	com_ptr<ID3D11Texture2D> texture_d3d;
	auto d3d_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_size.x, m_size.y,
		1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
	d3d_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	ICY_COM_ERROR(static_cast<ID3D11Device*>(m_system->m_device_d3d.get())->CreateTexture2D(
		&d3d_desc, nullptr, &texture_d3d));

	com_ptr<ID2D1DeviceContext> context;
	ICY_COM_ERROR(static_cast<ID2D1Device*>(m_system->m_device_d2d.get())->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context));

	com_ptr<IDXGISurface> dxgi;
	ICY_COM_ERROR(texture_d3d->QueryInterface(IID_PPV_ARGS(&dxgi)));

	const auto d2d_desc = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, 
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

	com_ptr<ID2D1Bitmap1> texture_d2d;
	ICY_COM_ERROR(context->CreateBitmapFromDxgiSurface(dxgi, d2d_desc, &texture_d2d));

	context->SetTarget(texture_d2d);
	context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
	m_texture_d3d = texture_d3d;
	m_texture_d2d = texture_d2d;
	m_context = context;
	return error_type();
}
void* gui_texture::handle() const noexcept
{
	void* ptr = nullptr;
	com_ptr<IDXGIResource> dxgi;
	m_texture_d3d->QueryInterface(IID_PPV_ARGS(&dxgi));
	dxgi->GetSharedHandle(&ptr);
	return ptr;
}
adapter gui_texture::adapter() const noexcept
{
	return m_system->m_adapter;
}

void gui_texture::draw_begin(const icy::color color) noexcept
{
	const auto context = static_cast<ID2D1DeviceContext*>(m_context.get());
	context->BeginDraw();
	//context->SetTransform(D2D1::Matrix3x2F::Scale(0.5f, 0.5f));
	context->Clear(d2d_color(color));
}
error_type gui_texture::draw_end() noexcept
{
	const auto context_d2d = static_cast<ID2D1DeviceContext*>(m_context.get());
	ICY_COM_ERROR(context_d2d->EndDraw());
	
	auto& device = *static_cast<ID3D11Device*>(m_system->m_device_d3d.get());
	com_ptr<ID3D11DeviceContext> context_d3d;
	device.GetImmediateContext(&context_d3d);
	context_d3d->Flush();

	return error_type();
}
error_type gui_texture::draw_text(const float x, const float y, const icy::color color, icy::com_ptr<IUnknown> text) noexcept
{
	const auto context = static_cast<ID2D1DeviceContext*>(m_context.get());
	com_ptr<ID2D1SolidColorBrush> brush;
	ICY_COM_ERROR(context->CreateSolidColorBrush(d2d_color(color), &brush));
	context->DrawTextLayout(D2D1::Point2F(x, y), static_cast<IDWriteTextLayout*>(text.get()), brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
	return error_type();
}
error_type gui_texture::fill_rect(const rect_type& rect, const color color) noexcept
{
	const auto context = static_cast<ID2D1DeviceContext*>(m_context.get());
	com_ptr<ID2D1SolidColorBrush> brush;
	ICY_COM_ERROR(context->CreateSolidColorBrush(d2d_color(color), &brush));
	context->FillRectangle(D2D1::RectF(rect.min_x, rect.min_y, rect.max_x, rect.max_y), brush);
	return error_type();
}

error_type gui_render_system::initialize() noexcept
{
	ICY_ERROR(m_lib_user32.initialize());
	ICY_ERROR(m_lib_dwrite.initialize());
	ICY_ERROR(m_lib_d3d.initialize());
	ICY_ERROR(m_lib_d2d.initialize());

	if (const auto func = ICY_FIND_FUNC(m_lib_dwrite, DWriteCreateFactory))
	{
		ICY_COM_ERROR(func(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), &m_factory_dwrite));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}

	com_ptr<ID3D11Device> device_d3d;
	if (const auto func = ICY_FIND_FUNC(m_lib_d3d, D3D11CreateDevice))
	{
		const auto level = m_adapter.flags() & adapter_flags::d3d10 ? D3D_FEATURE_LEVEL_10_1 : D3D_FEATURE_LEVEL_11_0;
		const auto adapter = static_cast<IDXGIAdapter*>(m_adapter.handle());
		uint32_t flags = 
			D3D11_CREATE_DEVICE_BGRA_SUPPORT | 
			D3D11_CREATE_DEVICE_SINGLETHREADED | 
			D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
		
		if (m_adapter.flags() & adapter_flags::debug)
			flags |= D3D11_CREATE_DEVICE_DEBUG;
		
		ICY_COM_ERROR(func(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, &level, 1, 
			D3D11_SDK_VERSION, &device_d3d, nullptr, nullptr));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}

	com_ptr<ID2D1Device> device_d2d;
	if (const auto func = m_lib_d2d.find("D2D1CreateDevice"))
	{
		com_ptr<IDXGIDevice> dxgi_device;
		ICY_COM_ERROR(device_d3d->QueryInterface(&dxgi_device));

		auto func_type = reinterpret_cast<HRESULT(WINAPI*)(IDXGIDevice*, const D2D1_CREATION_PROPERTIES*, ID2D1Device**)>(func);
		auto props = D2D1::CreationProperties(D2D1_THREADING_MODE_SINGLE_THREADED,
			m_adapter.flags() & adapter_flags::debug ? D2D1_DEBUG_LEVEL_INFORMATION : D2D1_DEBUG_LEVEL_NONE,
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE);
		ICY_COM_ERROR(func_type(dxgi_device, &props, &device_d2d));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}

	com_ptr<ID2D1Factory> factory;
	device_d2d->GetFactory(&factory);

	m_device_d3d = device_d3d;
	m_device_d2d = device_d2d;
	m_factory_d2d = factory;
	
	return error_type();
}
error_type gui_render_system::enum_font_names(array<string>& fonts) const noexcept
{
	com_ptr<IDWriteFontCollection> collection;
	ICY_COM_ERROR(static_cast<IDWriteFactory*>(m_factory_dwrite.get())->GetSystemFontCollection(&collection));

	const auto count = collection->GetFontFamilyCount();
	fonts.clear();
	ICY_ERROR(fonts.reserve(count));

	for (auto k = 0u; k < count; ++k)
	{
		com_ptr<IDWriteFontFamily> family;
		ICY_COM_ERROR(collection->GetFontFamily(k, &family));

		com_ptr<IDWriteLocalizedStrings> names;
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
			ICY_ERROR(fonts.push_back(std::move(new_font)));
		}
	}
	return error_type();
}
error_type gui_render_system::create_font(gui_font& font, HWND__* hwnd, const gui_system_font_type type) const noexcept
{
	auto dpi = 0u;
	if (const auto func = ICY_FIND_FUNC(m_lib_user32, GetDpiForWindow))
	{
		dpi = func(hwnd);
		if (!dpi)
			return last_system_error();
	}
	else
	{
		dpi = 96u;// make_stdlib_error(std::errc::function_not_supported);
	}

	auto ncm = NONCLIENTMETRICS{ sizeof(NONCLIENTMETRICS) };
	if (const auto func = ICY_FIND_FUNC(m_lib_user32, SystemParametersInfoW))
	{
		if (!func(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
			return last_system_error();
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}


	auto factory_dwrite = static_cast<IDWriteFactory*>(m_factory_dwrite.get());
	com_ptr<IDWriteGdiInterop> gdi;
	ICY_COM_ERROR(factory_dwrite->GetGdiInterop(&gdi));

	LOGFONT* font_ptr = &ncm.lfMessageFont;
	switch (type)
	{
	case gui_system_font_type_menu:
		font_ptr = &ncm.lfMenuFont;
		break;
	case gui_system_font_type_caption:
		font_ptr = &ncm.lfCaptionFont;
		break;
	case gui_system_font_type_caption_small:
		font_ptr = &ncm.lfSmCaptionFont;
		break;
	case gui_system_font_type_status:
		font_ptr = &ncm.lfStatusFont;
		break;
	}
	com_ptr<IDWriteFont> dwrite_font;
	ICY_COM_ERROR(gdi->CreateFontFromLOGFONT(font_ptr, &dwrite_font));

	if (font_ptr->lfHeight > 0)
	{
		DWRITE_FONT_METRICS metrics;
		dwrite_font->GetMetrics(&metrics);
		font_ptr->lfHeight *= metrics.designUnitsPerEm;
		font_ptr->lfHeight /= (metrics.ascent + metrics.descent);
	}
	const auto size = MulDiv(abs(font_ptr->lfHeight), dpi, 72);

	com_ptr<IDWriteTextFormat> format;
	ICY_COM_ERROR(factory_dwrite->CreateTextFormat(font_ptr->lfFaceName, nullptr,
		dwrite_font->GetWeight(), dwrite_font->GetStyle(), dwrite_font->GetStretch(), 
		float(size), L"en-US", &format));

	font.value = format;
	return error_type();
}
error_type gui_render_system::create_text(gui_text& text, const gui_font& font, const string_view str) const noexcept
{
	const auto factory = static_cast<IDWriteFactory*>(m_factory_dwrite.get());

	array<wchar_t> wstr;
	ICY_ERROR(to_utf16(str, wstr));
	const auto count = uint32_t(wstr.size() - 1);

	com_ptr<IDWriteTextLayout> layout;
	ICY_COM_ERROR(factory->CreateTextLayout(wstr.data(), count, 
		static_cast<IDWriteTextFormat*>(font.value.get()), FLT_MAX, FLT_MAX, &layout));

	text.value = layout;
	return error_type();
}
error_type gui_render_system::create_texture(shared_ptr<gui_texture>& texture, const window_size size, const render_flags flags) const noexcept
{
	ICY_ERROR(make_shared(texture, make_shared_from_this(this), size, flags));
	ICY_ERROR(texture->initialize());
	return error_type();
}

/*
	com_ptr<ID3D11Texture2D> cpu_texture;
ICY_COM_ERROR(device.CreateTexture2D(&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM,
	m_size.x, m_size.y, 1, 1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ), nullptr, &cpu_texture));

D3D11_BOX box = { 0, 0, 0, m_size.x, m_size.y, 1 };
context_d3d->CopySubresourceRegion(cpu_texture, 0, 0, 0, 0, static_cast<ID3D11Texture2D*>(m_texture_d3d.get()), 0, &box);

D3D11_MAPPED_SUBRESOURCE map;
ICY_COM_ERROR(context_d3d->Map(cpu_texture, 0, D3D11_MAP_READ, 0, &map));
ICY_SCOPE_EXIT{ context_d3d->Unmap(cpu_texture, 0); };
auto dst = colors.data();
for (auto row = 0u; row < m_size.y; ++row)
{
	auto ptr = reinterpret_cast<const uint32_t*>(static_cast<char*>(map.pData) + row * map.RowPitch);
	for (auto col = 0u; col < m_size.x; ++col, ++dst)
		*dst = color::from_rgba(ptr[col]);
}*/