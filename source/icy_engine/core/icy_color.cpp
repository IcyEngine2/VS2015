#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <cmath>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class color_lab
{
public:
    color_lab() noexcept : l(0), a(0), b(0)
    {

    }
    color_lab(const color color) noexcept;
public:
    float l;
    float a;
    float b;
};
struct D2D1
{
    enum ColorF
    {
        AliceBlue = 0xF0F8FF,
        AntiqueWhite = 0xFAEBD7,
        Aqua = 0x00FFFF,
        Aquamarine = 0x7FFFD4,
        Azure = 0xF0FFFF,
        Beige = 0xF5F5DC,
        Bisque = 0xFFE4C4,
        Black = 0x000000,
        BlanchedAlmond = 0xFFEBCD,
        Blue = 0x0000FF,
        BlueViolet = 0x8A2BE2,
        Brown = 0xA52A2A,
        BurlyWood = 0xDEB887,
        CadetBlue = 0x5F9EA0,
        Chartreuse = 0x7FFF00,
        Chocolate = 0xD2691E,
        Coral = 0xFF7F50,
        CornflowerBlue = 0x6495ED,
        Cornsilk = 0xFFF8DC,
        Crimson = 0xDC143C,
        Cyan = 0x00FFFF,
        DarkBlue = 0x00008B,
        DarkCyan = 0x008B8B,
        DarkGoldenrod = 0xB8860B,
        DarkGray = 0xA9A9A9,
        DarkGreen = 0x006400,
        DarkKhaki = 0xBDB76B,
        DarkMagenta = 0x8B008B,
        DarkOliveGreen = 0x556B2F,
        DarkOrange = 0xFF8C00,
        DarkOrchid = 0x9932CC,
        DarkRed = 0x8B0000,
        DarkSalmon = 0xE9967A,
        DarkSeaGreen = 0x8FBC8F,
        DarkSlateBlue = 0x483D8B,
        DarkSlateGray = 0x2F4F4F,
        DarkTurquoise = 0x00CED1,
        DarkViolet = 0x9400D3,
        DeepPink = 0xFF1493,
        DeepSkyBlue = 0x00BFFF,
        DimGray = 0x696969,
        DodgerBlue = 0x1E90FF,
        Firebrick = 0xB22222,
        FloralWhite = 0xFFFAF0,
        ForestGreen = 0x228B22,
        Fuchsia = 0xFF00FF,
        Gainsboro = 0xDCDCDC,
        GhostWhite = 0xF8F8FF,
        Gold = 0xFFD700,
        Goldenrod = 0xDAA520,
        Gray = 0x808080,
        Green = 0x008000,
        GreenYellow = 0xADFF2F,
        Honeydew = 0xF0FFF0,
        HotPink = 0xFF69B4,
        IndianRed = 0xCD5C5C,
        Indigo = 0x4B0082,
        Ivory = 0xFFFFF0,
        Khaki = 0xF0E68C,
        Lavender = 0xE6E6FA,
        LavenderBlush = 0xFFF0F5,
        LawnGreen = 0x7CFC00,
        LemonChiffon = 0xFFFACD,
        LightBlue = 0xADD8E6,
        LightCoral = 0xF08080,
        LightCyan = 0xE0FFFF,
        LightGoldenrodYellow = 0xFAFAD2,
        LightGreen = 0x90EE90,
        LightGray = 0xD3D3D3,
        LightPink = 0xFFB6C1,
        LightSalmon = 0xFFA07A,
        LightSeaGreen = 0x20B2AA,
        LightSkyBlue = 0x87CEFA,
        LightSlateGray = 0x778899,
        LightSteelBlue = 0xB0C4DE,
        LightYellow = 0xFFFFE0,
        Lime = 0x00FF00,
        LimeGreen = 0x32CD32,
        Linen = 0xFAF0E6,
        Magenta = 0xFF00FF,
        Maroon = 0x800000,
        MediumAquamarine = 0x66CDAA,
        MediumBlue = 0x0000CD,
        MediumOrchid = 0xBA55D3,
        MediumPurple = 0x9370DB,
        MediumSeaGreen = 0x3CB371,
        MediumSlateBlue = 0x7B68EE,
        MediumSpringGreen = 0x00FA9A,
        MediumTurquoise = 0x48D1CC,
        MediumVioletRed = 0xC71585,
        MidnightBlue = 0x191970,
        MintCream = 0xF5FFFA,
        MistyRose = 0xFFE4E1,
        Moccasin = 0xFFE4B5,
        NavajoWhite = 0xFFDEAD,
        Navy = 0x000080,
        OldLace = 0xFDF5E6,
        Olive = 0x808000,
        OliveDrab = 0x6B8E23,
        Orange = 0xFFA500,
        OrangeRed = 0xFF4500,
        Orchid = 0xDA70D6,
        PaleGoldenrod = 0xEEE8AA,
        PaleGreen = 0x98FB98,
        PaleTurquoise = 0xAFEEEE,
        PaleVioletRed = 0xDB7093,
        PapayaWhip = 0xFFEFD5,
        PeachPuff = 0xFFDAB9,
        Peru = 0xCD853F,
        Pink = 0xFFC0CB,
        Plum = 0xDDA0DD,
        PowderBlue = 0xB0E0E6,
        Purple = 0x800080,
        Red = 0xFF0000,
        RosyBrown = 0xBC8F8F,
        RoyalBlue = 0x4169E1,
        SaddleBrown = 0x8B4513,
        Salmon = 0xFA8072,
        SandyBrown = 0xF4A460,
        SeaGreen = 0x2E8B57,
        SeaShell = 0xFFF5EE,
        Sienna = 0xA0522D,
        Silver = 0xC0C0C0,
        SkyBlue = 0x87CEEB,
        SlateBlue = 0x6A5ACD,
        SlateGray = 0x708090,
        Snow = 0xFFFAFA,
        SpringGreen = 0x00FF7F,
        SteelBlue = 0x4682B4,
        Tan = 0xD2B48C,
        Teal = 0x008080,
        Thistle = 0xD8BFD8,
        Tomato = 0xFF6347,
        Turquoise = 0x40E0D0,
        Violet = 0xEE82EE,
        Wheat = 0xF5DEB3,
        White = 0xFFFFFF,
        WhiteSmoke = 0xF5F5F5,
        Yellow = 0xFFFF00,
        YellowGreen = 0x9ACD32,
    };
};
ICY_STATIC_NAMESPACE_END

static const std::pair<string_view, uint32_t> colors_array[] =
{
	{ },
	{ "Alice Blue"_s, D2D1::ColorF::AliceBlue },
	{ "Antique White"_s, D2D1::ColorF::AntiqueWhite },
	{ "Aqua"_s, D2D1::ColorF::Aqua },
	{ "Aquamarine"_s, D2D1::ColorF::Aquamarine },
	{ "Azure"_s, D2D1::ColorF::Azure },
	{ "Beige"_s, D2D1::ColorF::Beige },
	{ "Bisque"_s, D2D1::ColorF::Bisque },
	{ "Black"_s, D2D1::ColorF::Black },
	{ "Blanched Almond"_s, D2D1::ColorF::BlanchedAlmond },
	{ "Blue"_s, D2D1::ColorF::Blue },
	{ "Blue Violet"_s, D2D1::ColorF::BlueViolet },
	{ "Brown"_s, D2D1::ColorF::Brown },
	{ "Burly Wood"_s, D2D1::ColorF::BurlyWood },
	{ "Cadet Blue"_s, D2D1::ColorF::CadetBlue },
	{ "Chartreuse"_s, D2D1::ColorF::Chartreuse },
	{ "Chocolate"_s, D2D1::ColorF::Chocolate },
	{ "Coral"_s, D2D1::ColorF::Coral },
	{ "Cornflower Blue"_s, D2D1::ColorF::CornflowerBlue },
	{ "Cornsilk"_s, D2D1::ColorF::Cornsilk },
	{ "Crimson"_s, D2D1::ColorF::Crimson },
	{ "Cyan"_s, D2D1::ColorF::Cyan },
	{ "Dark Blue"_s, D2D1::ColorF::DarkBlue },
	{ "Dark Cyan"_s, D2D1::ColorF::DarkCyan },
	{ "Dark Goldenrod"_s, D2D1::ColorF::DarkGoldenrod },
	{ "Dark Gray"_s, D2D1::ColorF::DarkGray },
	{ "Dark Green"_s, D2D1::ColorF::DarkGreen },
	{ "Dark Khaki"_s, D2D1::ColorF::DarkKhaki },
	{ "Dark Magenta"_s, D2D1::ColorF::DarkMagenta },
	{ "Dark Olive Green"_s, D2D1::ColorF::DarkOliveGreen },
	{ "Dark Orange"_s, D2D1::ColorF::DarkOrange },
	{ "Dark Orchid"_s, D2D1::ColorF::DarkOrchid },
	{ "Dark Rred"_s, D2D1::ColorF::DarkRed },
	{ "Dark Salmon"_s, D2D1::ColorF::DarkSalmon },
	{ "Dark Sea Green"_s, D2D1::ColorF::DarkSeaGreen },
	{ "Dark Slate Blue"_s, D2D1::ColorF::DarkSlateBlue },
	{ "Dark Slate Gray"_s, D2D1::ColorF::DarkSlateGray },
	{ "Dark Turquoise"_s, D2D1::ColorF::DarkTurquoise },
	{ "Dark Violet"_s, D2D1::ColorF::DarkViolet },
	{ "Deep Pink"_s, D2D1::ColorF::DeepPink },
	{ "Deep Skyblue"_s, D2D1::ColorF::DeepSkyBlue },
	{ "Dim Gray"_s, D2D1::ColorF::DimGray },
	{ "Dodger Blue"_s, D2D1::ColorF::DodgerBlue },
	{ "Firebrick"_s, D2D1::ColorF::Firebrick },
	{ "Floral White"_s, D2D1::ColorF::FloralWhite },
	{ "Forest Green"_s, D2D1::ColorF::ForestGreen },
	{ "Fuchsia"_s, D2D1::ColorF::Fuchsia },
	{ "Gainsboro"_s, D2D1::ColorF::Gainsboro },
	{ "Ghost White"_s, D2D1::ColorF::GhostWhite },
	{ "Gold"_s, D2D1::ColorF::Gold },
	{ "Goldenrod"_s, D2D1::ColorF::Goldenrod },
	{ "Gray"_s, D2D1::ColorF::Gray },
	{ "Green"_s, D2D1::ColorF::Green },
	{ "Green Yellow"_s, D2D1::ColorF::GreenYellow },
	{ "Honeydew"_s, D2D1::ColorF::Honeydew },
	{ "Hot Pink"_s, D2D1::ColorF::HotPink },
	{ "Indian Red"_s, D2D1::ColorF::IndianRed },
	{ "Indigo"_s, D2D1::ColorF::Indigo },
	{ "Ivory"_s, D2D1::ColorF::Ivory },
	{ "Khaki"_s, D2D1::ColorF::Khaki },
	{ "Lavender"_s, D2D1::ColorF::Lavender },
	{ "Lavender Blush"_s, D2D1::ColorF::LavenderBlush },
	{ "Lawn Green"_s, D2D1::ColorF::LawnGreen },
	{ "Lemon Chiffon"_s, D2D1::ColorF::LemonChiffon },
	{ "Light Blue"_s, D2D1::ColorF::LightBlue },
	{ "Light Coral"_s, D2D1::ColorF::LightCoral },
	{ "Light Cyan"_s, D2D1::ColorF::LightCyan },
	{ "Light Goldenrod Yellow"_s, D2D1::ColorF::LightGoldenrodYellow },
	{ "Light Green"_s, D2D1::ColorF::LightGreen },
	{ "Light Gray"_s, D2D1::ColorF::LightGray },
	{ "Light Pink"_s, D2D1::ColorF::LightPink },
	{ "Light Salmon"_s, D2D1::ColorF::LightSalmon },
	{ "Light Sea Green"_s, D2D1::ColorF::LightSeaGreen },
	{ "Light Sky Blue"_s, D2D1::ColorF::LightSkyBlue },
	{ "Light Slate Gray"_s, D2D1::ColorF::LightSlateGray },
	{ "Light Steel Blue"_s, D2D1::ColorF::LightSteelBlue },
	{ "Light Yellow"_s, D2D1::ColorF::LightYellow },
	{ "Lime"_s, D2D1::ColorF::Lime },
	{ "Lime Green"_s, D2D1::ColorF::LimeGreen },
	{ "Linen"_s, D2D1::ColorF::Linen },
	{ "Magenta"_s, D2D1::ColorF::Magenta },
	{ "Maroon"_s, D2D1::ColorF::Maroon },
	{ "Medium Aquamarine"_s, D2D1::ColorF::MediumAquamarine },
	{ "Medium Blue"_s, D2D1::ColorF::MediumBlue },
	{ "Medium Orchid"_s, D2D1::ColorF::MediumOrchid },
	{ "Medium Purple"_s, D2D1::ColorF::MediumPurple },
	{ "Medium Sea Green"_s, D2D1::ColorF::MediumSeaGreen },
	{ "Medium Slate Blue"_s, D2D1::ColorF::MediumSlateBlue },
	{ "Medium Spring Green"_s, D2D1::ColorF::MediumSpringGreen },
	{ "Medium Turquoise"_s, D2D1::ColorF::MediumTurquoise },
	{ "Medium Violet Red"_s, D2D1::ColorF::MediumVioletRed },
	{ "Midnight Blue"_s, D2D1::ColorF::MidnightBlue },
	{ "Mint Cream"_s, D2D1::ColorF::MintCream },
	{ "Misty Rose"_s, D2D1::ColorF::MistyRose },
	{ "Moccasin"_s, D2D1::ColorF::Moccasin },
	{ "Navajo White"_s, D2D1::ColorF::NavajoWhite },
	{ "Navy"_s, D2D1::ColorF::Navy },
	{ "Old Lace"_s, D2D1::ColorF::OldLace },
	{ "Olive"_s, D2D1::ColorF::Olive },
	{ "Olive Drab"_s, D2D1::ColorF::OliveDrab },
	{ "Orange"_s, D2D1::ColorF::Orange },
	{ "Orange Red"_s, D2D1::ColorF::OrangeRed },
	{ "Orchid"_s, D2D1::ColorF::Orchid },
	{ "Pale Goldenrod"_s, D2D1::ColorF::PaleGoldenrod },
	{ "Pale Green"_s, D2D1::ColorF::PaleGreen },
	{ "Pale Turquoise"_s, D2D1::ColorF::PaleTurquoise },
	{ "Pale Violet Red"_s, D2D1::ColorF::PaleVioletRed },
	{ "Papaya Whip"_s, D2D1::ColorF::PapayaWhip },
	{ "Peach Puff"_s, D2D1::ColorF::PeachPuff },
	{ "Peru"_s, D2D1::ColorF::Peru },
	{ "Pink"_s, D2D1::ColorF::Pink },
	{ "Plum"_s, D2D1::ColorF::Plum },
	{ "Powder Blue"_s, D2D1::ColorF::PowderBlue },
	{ "Purple"_s, D2D1::ColorF::Purple },
	{ "Red"_s, D2D1::ColorF::Red },
	{ "Rosy Brown"_s, D2D1::ColorF::RosyBrown },
	{ "Royal Blue"_s, D2D1::ColorF::RoyalBlue },
	{ "Saddle Brown"_s, D2D1::ColorF::SaddleBrown },
	{ "Salmon"_s, D2D1::ColorF::Salmon },
	{ "Sandy Brown"_s, D2D1::ColorF::SandyBrown },
	{ "Sea Green"_s, D2D1::ColorF::SeaGreen },
	{ "Sea Shell"_s, D2D1::ColorF::SeaShell },
	{ "Sienna"_s, D2D1::ColorF::Sienna },
	{ "Silver"_s, D2D1::ColorF::Silver },
	{ "Sky Blue"_s, D2D1::ColorF::SkyBlue },
	{ "Slate Blue"_s, D2D1::ColorF::SlateBlue },
	{ "Slate Gray"_s, D2D1::ColorF::SlateGray },
	{ "Snow"_s, D2D1::ColorF::Snow },
	{ "Spring Green"_s, D2D1::ColorF::SpringGreen },
	{ "Steel Blue"_s, D2D1::ColorF::SteelBlue },
	{ "Tan"_s, D2D1::ColorF::Tan },
	{ "Teal"_s, D2D1::ColorF::Teal },
	{ "Thistle"_s, D2D1::ColorF::Thistle },
	{ "Tomato"_s, D2D1::ColorF::Tomato },
	{ "Turquoise"_s, D2D1::ColorF::Turquoise },
	{ "Violet"_s, D2D1::ColorF::Violet },
	{ "Wheat"_s, D2D1::ColorF::Wheat },
	{ "White"_s, D2D1::ColorF::White },
	{ "White Smoke"_s, D2D1::ColorF::WhiteSmoke },
	{ "Yellow"_s, D2D1::ColorF::Yellow },
	{ "Yellow Green"_s, D2D1::ColorF::YellowGreen },
};

color::color(const colors type, const uint8_t alpha) noexcept
{
	const auto index = size_t(type);
	if (index < std::size(colors_array))
		bgra = colors_array[index].second;
	if (index)
		a = alpha;
}
color color::from_hsv(const color_hsv& hsv, const uint8_t alpha) noexcept
{
	const auto h = int(hsv.hue / 60);
	const auto v_val = uint8_t(255 * hsv.val);
	const auto v_min = uint8_t((1.0F - hsv.sat) * hsv.val);
	const auto v_tmp = uint8_t((v_val - v_min) * (int(hsv.hue) % 60) / 60.0F);
	const auto v_inc = uint8_t(v_min + v_tmp);
	const auto v_dec = uint8_t(v_val - v_tmp);

	uint8_t red = 0;
	uint8_t green = 0;
	uint8_t blue = 0;

	switch (h % 6)
	{
	case 0: red = v_val; green = v_inc; blue = v_min; break;
	case 1: red = v_dec; green = v_val; blue = v_min; break;
	case 2: red = v_min; green = v_val; blue = v_inc; break;
	case 3: red = v_min; green = v_dec; blue = v_val; break;
	case 4: red = v_inc; green = v_min; blue = v_val; break;
	case 5: red = v_val; green = v_min; blue = v_dec; break;
	}
	return color::from_rgba(red, green, blue, alpha);
}
float color::operator-(const color& rhs) const noexcept
{
	const auto lab_lhs = color_lab(*this);
	const auto lab_rhs = color_lab(rhs);
	return sqr(lab_lhs.a - lab_rhs.a) + sqr(lab_lhs.b - lab_rhs.b) + sqr(lab_lhs.l - lab_rhs.l);
}
color_lab::color_lab(const color color) noexcept
{
	float rgba[4];
	color.to_rgbaf(rgba);
	for (auto&& value : rgba)
	{
		if (value > 0.04045F) value = std::pow(value + 0.055F, 2.2F);
		else value /= 12.92F;
	}

	const auto x = rgba[0] * 0.4124F + rgba[1] * 0.3576F + rgba[2] * 0.1805F;
	const auto y = rgba[0] * 0.2126F + rgba[1] * 0.7152F + rgba[2] * 0.0722F;
	const auto z = rgba[0] * 0.0193F + rgba[1] * 0.1192F + rgba[2] * 0.9505F;

	const auto fxyz = [](const float t)
	{
		if (t > 0.008856F) return std::pow(t, 1.0F / 3.0F);
		else return 7.787F * t + 16.0F / 116.0F;
	};
	l = 116.0F * fxyz(y) - 16.0F;
	a = 500.0F * (fxyz(x / 0.9505F) - fxyz(y));
	b = 200.0F * (fxyz(y) - fxyz(z / 1.089F));
}
color_hsv::color_hsv(const color& color) noexcept
{
	auto K = 0.0F;
	static const auto c = 1.0F / 255.0F;
	static const auto eps = 1e-20F;
	auto r = color.r * c;
	auto g = color.g * c;
	auto b = color.a * c;

	if (g < b)
	{
		std::swap(g, b);
		K = -1.0F;
	}

	if (r < g)
	{
		std::swap(r, g);
		K = -2.0F / 6.0F - K;
	}

	auto chroma = r - std::min(g, b);
	hue = std::fabs(K + (g - b) / (6.0F * chroma + eps));
	sat = chroma / (r + eps);
	val = r;
}


string_view icy::to_string(const colors value) noexcept
{
	const auto index = size_t(value);
	if (index < std::size(colors_array))
		return colors_array[index].first;
	return string_view();
}
error_type icy::to_string(const color value, string& str) noexcept
{
	ICY_ERROR(icy::to_string(value.bgra, 0x10, str));
	ICY_ERROR(str.replace("0x"_s, "#"_s));
	return error_type();
}