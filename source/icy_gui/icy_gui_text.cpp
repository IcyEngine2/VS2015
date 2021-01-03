#include "icy_gui_text.hpp"
#include <dwrite_3.h>
using namespace icy;

error_type gui_text_system::initialize() noexcept
{
	ICY_ERROR(m_lib_dwrite.initialize());
	ICY_ERROR(m_lib_user.initialize());
	ICY_ERROR(m_lib_gdi.initialize());

	com_ptr<IDWriteFactory> factory;
	if (const auto func = ICY_FIND_FUNC(m_lib_dwrite, DWriteCreateFactory))
	{
		ICY_COM_ERROR(func(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&factory)));
	}
	else
	{
		return make_stdlib_error(std::errc::function_not_supported);
	}
	com_ptr<IDWriteFontCollection> collection;
	ICY_COM_ERROR(factory->GetSystemFontCollection(&collection));

	const auto count = collection->GetFontFamilyCount();
	ICY_ERROR(m_fonts.reserve(count));

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
			ICY_ERROR(m_fonts.push_back(std::move(new_font)));
		}
	}
	return error_type();
}
error_type gui_text_system::enum_font_names(array<string>& fonts) const noexcept
{
	return copy(m_fonts, fonts);
}
error_type gui_text_system::enum_font_sizes(const string_view font, array<uint32_t>& sizes) const noexcept
{
	LOGFONT lfont = {};
	lfont.lfCharSet = DEFAULT_CHARSET;
	lfont.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
	
	auto fsize = sizeof(lfont.lfFaceName) / sizeof(*lfont.lfFaceName);
	ICY_ERROR(font.to_utf16(lfont.lfFaceName, &fsize));

#define X(LIB, VAR, NAME) const auto VAR = ICY_FIND_FUNC(LIB, NAME); if (!VAR) return make_stdlib_error(std::errc::function_not_supported);
	X(m_lib_user, func_acquire, GetDC);
	X(m_lib_user, func_release, ReleaseDC);
	X(m_lib_gdi, func_dpi, GetDeviceCaps);
	X(m_lib_gdi, func_enum, EnumFontFamiliesExW);

	struct data_type
	{
		HDC hdc = nullptr;
		array<uint32_t>* sizes;
		uint32_t dpi = 0;
		error_type error;
	} data;
	
	data.hdc = func_acquire(nullptr);
	if (!data.hdc)
		return last_system_error();
	ICY_SCOPE_EXIT{ func_release(nullptr, data.hdc); };
	
	data.dpi = func_dpi(data.hdc, LOGPIXELSY);
	if (!data.dpi)
		return last_system_error();

	data.sizes = &sizes;

	const auto proc = [](const LOGFONTW* lfont, const TEXTMETRICW* ptm, DWORD type, LPARAM ptr) -> int
	{
		const auto data = reinterpret_cast<data_type*>(ptr);

		const uint32_t true_type_sizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };
		if (type != TRUETYPE_FONTTYPE)
		{
			const auto lsize = ptm->tmHeight - ptm->tmInternalLeading;
			const auto psize = MulDiv(lsize, 72, data->dpi);
			data->error = data->sizes->push_back(psize);
			return data->error ? 0 : 1;
		}
		else
		{
			data->error = data->sizes->assign(true_type_sizes);
			return 0;
		}
	};
	func_enum(data.hdc, &lfont, proc, reinterpret_cast<LPARAM>(&data), 0);
	std::sort(sizes.begin(), sizes.end());
	sizes.pop_back(std::distance(std::unique(sizes.begin(), sizes.end()), sizes.end()));
	return data.error;
}

/*#include "icy_render_svg.hpp"
#include "icy_render_factory.hpp"
#include <dwrite_3.h>

using namespace icy;

class render_svg_text_stack
{
public:
	error_type initialize(IDWriteFactory& write, const render_svg_font_data& style) noexcept;
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
};

render_svg_font::render_svg_font(const render_svg_font& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1, std::memory_order_release);
}
render_svg_font::~render_svg_font() noexcept
{
	if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		allocator_type::destroy(data);
		allocator_type::deallocate(data);
	}
}

render_svg_text::render_svg_text(const render_svg_text& rhs) noexcept : data(rhs.data)
{
	if (data)
		data->ref.fetch_add(1, std::memory_order_release);
}
render_svg_text::~render_svg_text() noexcept
{
	if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		allocator_type::destroy(data);
		allocator_type::deallocate(data);
	}
}

error_type render_svg_text_stack::initialize(IDWriteFactory& write, const render_svg_font_data& style) noexcept
{
	auto ncm = NONCLIENTMETRICS{ sizeof(NONCLIENTMETRICS) };
	auto lib = "user32"_lib;
	ICY_ERROR(lib.initialize());
	if (const auto func = ICY_FIND_FUNC(lib, SystemParametersInfoW))
	{
		if (!func(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
			return last_system_error();
	}
	else
		return make_stdlib_error(std::errc::function_not_supported);

	com_ptr<IDWriteGdiInterop> gdi;
	ICY_COM_ERROR(write.GetGdiInterop(&gdi));
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

	com_ptr<IDWriteLocalizedStrings> localized;
	ICY_COM_ERROR(family->GetFamilyNames(&localized));

	auto index = 0u;
	auto exists = FALSE;
	auto length = 0u;
	ICY_COM_ERROR(localized->FindLocaleName(L"en-US", &index, &exists));
	ICY_COM_ERROR(localized->GetStringLength(index, &length));

	array<wchar_t> name;
	ICY_ERROR(name.resize(length + 1));
	ICY_COM_ERROR(localized->GetString(index, name.data(), length + 1));

	auto gdi_lib = "gdi32"_lib;
	ICY_ERROR(gdi_lib.initialize());
	
	auto dpi = 96.0f;
	const auto func_acquire = ICY_FIND_FUNC(lib, GetDC);
	const auto func_release = ICY_FIND_FUNC(lib, ReleaseDC);
	const auto func_devcaps = ICY_FIND_FUNC(gdi_lib, GetDeviceCaps);
	if (func_acquire && func_release && func_devcaps)
	{
		const auto screen = func_acquire(nullptr);
		ICY_SCOPE_EXIT{ func_release(nullptr, screen); };
		dpi = float(func_devcaps(screen, LOGPIXELSY));
	}

	text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
	paragraph_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
	line_spacing = 0u;
	trimming = DWRITE_TRIMMING_GRANULARITY_NONE;
	ICY_ERROR(weights.push_back(font->GetWeight()));
	ICY_ERROR(stretches.push_back(font->GetStretch()));
	ICY_ERROR(names.push_back(std::move(name)));
	ICY_ERROR(styles.push_back(font->GetStyle()));
	ICY_ERROR(sizes.push_back(std::abs(ptr->lfHeight) * dpi / 72.0F));
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
	return error_type();
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

error_type render_factory_data::enum_font_names(array<icy::string>& fonts) const noexcept
{

}
error_type render_factory_data::enum_font_sizes(array<uint32_t>& sizes) const noexcept
{
	return make_stdlib_error(std::errc::function_not_supported);
}
error_type render_factory_data::create_svg_font(const render_svg_font_data& data, render_svg_font& font) const noexcept
{
	render_svg_text_stack stack;
	ICY_ERROR(stack.initialize(*m_write, data));

	com_ptr<IDWriteTextFormat> format;
	ICY_COM_ERROR(m_write->CreateTextFormat(stack.names.back().data(),
		nullptr, stack.weights.back(), stack.styles.back(), stack.stretches.back(), 
		stack.sizes.back(), L"en-US", &format));
	ICY_COM_ERROR(format->SetLineSpacing
	(
		stack.line_spacing ? DWRITE_LINE_SPACING_METHOD_UNIFORM : DWRITE_LINE_SPACING_METHOD_DEFAULT,
		static_cast<float>(stack.line_spacing) * 1.0F,
		static_cast<float>(stack.line_spacing) * 0.8F
	));
	ICY_COM_ERROR(format->SetParagraphAlignment(stack.paragraph_align));
	ICY_COM_ERROR(format->SetTextAlignment(stack.text_align));
	if (stack.trimming)
	{
		DWRITE_TRIMMING desc{ stack.trimming };
		ICY_COM_ERROR(format->SetTrimming(&desc, nullptr));
	}

	auto new_data = allocator_type::allocate<render_svg_font::data_type>(1);
	if (!new_data)
		return make_stdlib_error(std::errc::not_enough_memory);
	allocator_type::construct(new_data);
	new_data->factory = make_shared_from_this(this);
	new_data->format = std::move(format);
	font = render_svg_font();
	font.data = new_data;

	return error_type();
}
error_type render_factory_data::create_svg_text(const string_view str, const color color, const window_size size, const render_svg_font& font, render_svg_text& text) const noexcept
{
	if (str.empty() || !font.data)
		return make_stdlib_error(std::errc::invalid_argument);

	array<wchar_t> wstr;
	ICY_ERROR(to_utf16(str, wstr));

	com_ptr<IDWriteTextLayout> layout;
	ICY_COM_ERROR(m_write->CreateTextLayout(wstr.data(), uint32_t(wstr.size()),
		font.data->format, size.x ? size.x : FLT_MAX, size.y ? size.y : FLT_MAX, &layout));

	auto new_data = allocator_type::allocate<render_svg_text::data_type>(1);
	if (!new_data)
		return make_stdlib_error(std::errc::not_enough_memory);
	allocator_type::construct(new_data);
	new_data->factory = make_shared_from_this(this);
	new_data->layout = std::move(layout);
	new_data->color = color;
	text = render_svg_text();
	text.data = new_data;
	return error_type();
}
*/