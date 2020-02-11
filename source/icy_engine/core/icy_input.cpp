#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <Windows.h>
#include <windowsx.h>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct key_pair
{
    string_view str;
    key key;
};
const key_pair key_array[109] =
{
    { "Left Mouse Button"_s, key::mouse_left },
    { "Right Mouse Button"_s, key::mouse_right},
    { "Middle Mouse Button"_s, key::mouse_mid },
    { "X1 Mouse Button"_s, key::mouse_x1 },
    { "X2 Mouse Button"_s, key::mouse_x2 },

    { "Backspace"_s, key::back },
    { "Tab"_s, key::tab },
    { "Enter"_s, key::enter },
    { "Pause"_s, key::pause },
    { "Caps Lock"_s, key::caps_lock },
    { "Esc"_s, key::esc },
    { "Space"_s, key::space },
    { "PgUp"_s, key::page_up },
    { "PgDown"_s, key::page_down },
    { "End"_s, key::end },
    { "Home"_s, key::home },
    { "Left"_s, key::left },
    { "Up"_s, key::up },
    { "Right"_s, key::right },
    { "Down"_s, key::down },
    { "Print Screen"_s, key::print_screen },
    { "Insert"_s, key::insert },
    { "Delete"_s, key::del },
    { "Left Win"_s, key::left_win },
    { "Right Win"_s, key::right_win },
    { "Num *"_s, key::num_mul },
    { "Num +"_s, key::num_add },
    { "Num ,"_s, key::num_separator },
    { "Num -"_s, key::num_sub },
    { "Num ."_s, key::num_decimal },
    { "Num /"_s, key::num_div },
    { "Num Enter"_s, key::num_enter },
    { "Num Lock"_s, key::num_lock },
    { "Scr Lock"_s, key::scr_lock },
    { "Left Shift"_s, key::left_shift },
    { "Right Shift"_s, key::right_shift },
    { "Left Ctrl"_s, key::left_ctrl },
    { "Right Ctrl"_s, key::right_ctrl },
    { "Left Alt"_s, key::left_alt },
    { "Right Alt"_s, key::right_alt },
    { ":"_s, key::colon },
    { "+"_s, key::plus },
    { ","_s, key::comma },
    { "-"_s, key::minus },
    { "."_s, key::period },
    { "/"_s, key::slash },
    { "~"_s, key::tilde },
    { "["_s, key::bracket_open },
    { "\\"_s, key::back_slash },
    { "]"_s, key::bracket_close },
    { "\""_s, key::quote },
    { "Num 0"_s, key::num_0 },
    { "Num 1"_s, key::num_1 },
    { "Num 2"_s, key::num_2 },
    { "Num 3"_s, key::num_3 },
    { "Num 4"_s, key::num_4 },
    { "Num 5"_s, key::num_5 },
    { "Num 6"_s, key::num_6 },
    { "Num 7"_s, key::num_7 },
    { "Num 8"_s, key::num_8 },
    { "Num 9"_s, key::num_9 },
    { "0"_s, key::_0 },
    { "1"_s, key::_1 },
    { "2"_s, key::_2 },
    { "3"_s, key::_3 },
    { "4"_s, key::_4 },
    { "5"_s, key::_5 },
    { "6"_s, key::_6 },
    { "7"_s, key::_7 },
    { "8"_s, key::_8 },
    { "9"_s, key::_9 },
    { "A"_s, key::a },
    { "B"_s, key::b },
    { "C"_s, key::c },
    { "D"_s, key::d },
    { "E"_s, key::e },
    { "F"_s, key::f },
    { "G"_s, key::g },
    { "H"_s, key::h },
    { "I"_s, key::i },
    { "J"_s, key::j },
    { "K"_s, key::k },
    { "L"_s, key::l },
    { "M"_s, key::m },
    { "N"_s, key::n },
    { "O"_s, key::o },
    { "P"_s, key::p },
    { "Q"_s, key::q },
    { "R"_s, key::r },
    { "S"_s, key::s },
    { "T"_s, key::t },
    { "U"_s, key::u },
    { "V"_s, key::v },
    { "W"_s, key::w },
    { "X"_s, key::x },
    { "Y"_s, key::y },
    { "Z"_s, key::z },
    { "F1"_s, key::F1 },
    { "F2"_s, key::F2 },
    { "F3"_s, key::F3 },
    { "F4"_s, key::F4 },
    { "F5"_s, key::F5 },
    { "F6"_s, key::F6 },
    { "F7"_s, key::F7 },
    { "F8"_s, key::F8 },
    { "F9"_s, key::F9 },
    { "F10"_s, key::F10 },
    { "F11"_s, key::F11 },
    { "F12"_s, key::F12 },
};
ICY_STATIC_NAMESPACE_END

key icy::detail::scan_vk_to_key(uint16_t vkey, uint16_t scan, const bool isE0) noexcept
{
    if (vkey == VK_SHIFT || vkey == VK_NUMLOCK)
    {
        library lib("user32.dll");
        if (lib.initialize() == error_type{})
        {
            if (const auto func = ICY_FIND_FUNC(lib, MapVirtualKeyW))
            {
                if (vkey == VK_SHIFT)
                    vkey = WORD(func(scan, MAPVK_VSC_TO_VK_EX));
                else if (vkey == VK_NUMLOCK)
                    scan = WORD(func(vkey, MAPVK_VK_TO_VSC) | 0x100);
            }
        }
    }
	auto key = icy::key(vkey);
	switch (vkey)
	{
	case VK_CONTROL:    key = isE0 ? key::right_ctrl    : key::left_ctrl;   break;
	case VK_MENU:       key = isE0 ? key::right_alt		: key::left_alt;    break;
	case VK_INSERT:     key = isE0 ? key::insert		: key::num_0;       break;
	case VK_DELETE:     key = isE0 ? key::del			: key::num_decimal; break;
	case VK_HOME:       key = isE0 ? key::home			: key::num_7;       break;
	case VK_END:        key = isE0 ? key::end			: key::num_1;       break;
	case VK_PRIOR:      key = isE0 ? key::page_up		: key::num_9;       break;
	case VK_NEXT:       key = isE0 ? key::page_down		: key::num_3;       break;
	case VK_LEFT:       key = isE0 ? key::left          : key::num_4;       break;
	case VK_RIGHT:      key = isE0 ? key::right			: key::num_6;       break;
	case VK_UP:         key = isE0 ? key::up			: key::num_8;       break;
	case VK_DOWN:       key = isE0 ? key::down			: key::num_2;       break;
	case VK_CLEAR:      key = isE0 ? key				: key::num_5;       break;
	case VK_RETURN:     key = isE0 ? key::num_enter		: key::enter;       break;
	}
	return key;
}
uint16_t icy::detail::from_winapi(input_message& msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept
{
    auto mod = key_mod::none;
    key_mod_set(mod, buffer);
	const auto key = scan_vk_to_key(LOWORD(wParam), WORD((lParam >> 16) & 0x00FF), !!((lParam >> 24) & 0x01));
	const auto event =
		((lParam >> 31) & 0x01) ? input_type::key_release :
		((lParam >> 30) & 0x01) ? input_type::key_hold :
		input_type::key_press;
	msg = input_message(event, key, mod);
	return LOWORD(lParam);
}
uint16_t icy::detail::from_winapi(input_message& key, const MSG& msg, const std::bitset<256> & buffer) noexcept
{
	return from_winapi(key, msg.wParam, msg.lParam, buffer);
}
void icy::detail::from_winapi(input_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const uint32_t msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept
{
	const auto word = LOWORD(wParam);
	auto mods = key_mod::none;
	key_mod_set(mods, buffer);

    if (word & MK_LBUTTON) mods = mods | key_mod::lmb;
    if (word & MK_RBUTTON) mods = mods | key_mod::rmb;
    if (word & MK_MBUTTON) mods = mods | key_mod::mmb;
    if (word & MK_XBUTTON1) mods = mods | key_mod::x1mb;
    if (word & MK_XBUTTON2) mods = mods | key_mod::x2mb;
    
	auto point = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (msg == WM_MOUSEWHEEL)
    {
        point.x -= offset_x;
        point.y -= offset_y;
    }
    auto type = input_type::none;
    auto wheel = 0u;
    auto key = key::none;

	if (msg == WM_MOUSEMOVE)
	{
		type = input_type::mouse_move;
	}
	else if (msg == WM_MOUSEWHEEL)
	{
        wheel = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
		type = input_type::mouse_wheel;
	}
	else
	{
		type = input_type::mouse_press;

		switch (msg)
		{
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
			type = input_type::mouse_release;
			break;
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			type = input_type::mouse_double;
			break;
		}

        key = key::mouse_left;
		switch (msg)
		{
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
			key = key::mouse_right;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			key = key::mouse_mid;
			break;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
			key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? key::mouse_x1 : key::mouse_x2;
            break;
		}
	}
    mouse = input_message(type, key, mods);
    mouse.wheel = wheel;
    mouse.point_x = point.x;
    mouse.point_y = point.y;
}
void icy::detail::from_winapi(input_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const MSG& msg, const std::bitset<256> & buffer) noexcept
{
	return from_winapi(mouse, offset_x, offset_y, msg.message, msg.wParam, msg.lParam, buffer);
}
tagMSG icy::detail::to_winapi(const input_message& input) noexcept
{
    MSG msg = {};
    switch (input.type)
    {
    case input_type::key_press:
    case input_type::key_hold:
    case input_type::key_release:
    {
        msg.wParam = WPARAM(input.key);
        library lib("user32.dll");
        if (lib.initialize() == error_type())
        {           
            if (const auto func = ICY_FIND_FUNC(lib, MapVirtualKeyW))
            {
                const auto scan = BYTE(func(unsigned(msg.wParam), MAPVK_VK_TO_VSC));
                msg.lParam = 0x0001 | // repeat
                    (scan << 16) | // scan
                    ((input.type == input_type::key_hold) << 30) |    // was down
                    ((input.type == input_type::key_release) << 31);  // is up
            }
        }
        if (key_mod(input.mods) & (key_mod::lalt | key_mod::ralt))
            msg.message = unsigned(input.type == input_type::key_release ? WM_SYSKEYUP : WM_SYSKEYDOWN);
        else
            msg.message = unsigned(input.type == input_type::key_release ? WM_KEYUP : WM_KEYDOWN);
        break;
    }
    case input_type::mouse_move:
    case input_type::mouse_wheel:
    case input_type::mouse_release:
    case input_type::mouse_press:
    case input_type::mouse_hold:
    case input_type::mouse_double:
    {
        msg.lParam = MAKELONG(input.point_x, input.point_y);
        const auto mods = key_mod(input.mods);

        auto loword = 0u;
        if (mods & key_mod::lmb) loword |= MK_LBUTTON;
        if (mods & key_mod::rmb) loword |= MK_RBUTTON;
        if (mods & key_mod::mmb) loword |= MK_MBUTTON;
        if (mods & key_mod::x1mb) loword |= MK_XBUTTON1;
        if (mods & key_mod::x2mb) loword |= MK_XBUTTON2;
        if (mods & (key_mod::lshift | key_mod::rshift)) loword |= MK_SHIFT;
        if (mods & (key_mod::lctrl | key_mod::rctrl)) loword |= MK_CONTROL;
        
        auto hiword = 0u;
        switch (input.type)
        {
        case input_type::mouse_move:
        {
            msg.message = WM_MOUSEMOVE;
            break;
        }
        case input_type::mouse_wheel:
        {
            msg.message = WM_MOUSEWHEEL;
            hiword = input.wheel;
            break;
        }
        case input_type::mouse_press:
        case input_type::mouse_hold:
        {
            if (input.key == key::mouse_left)
                msg.message = WM_LBUTTONDOWN;
            else if (input.key == key::mouse_right)
                msg.message = WM_RBUTTONDOWN;
            else if (input.key == key::mouse_mid)
                msg.message = WM_MBUTTONDOWN;
            else if (input.key == key::mouse_x1 || input.key == key::mouse_x2)
                msg.message = WM_XBUTTONDOWN;
            break;
        }
        case input_type::mouse_release:
        {
            if (input.key == key::mouse_left)
                msg.message = WM_LBUTTONUP;
            else if (input.key == key::mouse_right)
                msg.message = WM_RBUTTONUP;
            else if (input.key == key::mouse_mid)
                msg.message = WM_MBUTTONUP;
            else if (input.key == key::mouse_x1 || input.key == key::mouse_x2)
                msg.message = WM_XBUTTONUP;
            break;
        }
        case input_type::mouse_double:
        {
            if (input.key == key::mouse_left)
                msg.message = WM_LBUTTONDBLCLK;
            else if (input.key == key::mouse_right)
                msg.message = WM_RBUTTONDBLCLK;
            else if (input.key == key::mouse_mid)
                msg.message = WM_MBUTTONDBLCLK;
            else if (input.key == key::mouse_x1 || input.key == key::mouse_x2)
                msg.message = WM_XBUTTONDBLCLK;
            break;
        }
        }

        if (input.key == key::mouse_x1)
            hiword |= 0x01;
        else if (input.key == key::mouse_x2)
            hiword |= 0x02;

        msg.wParam = MAKELONG(loword, hiword);
        break;
    }
    }
	return msg;
}
void icy::detail::to_winapi(const input_message& input, uint32_t& msg, size_t& wParam, ptrdiff_t& lParam) noexcept
{
    auto output = detail::to_winapi(input);
    msg = output.message;
    wParam = output.wParam;
    lParam = output.lParam;
}
error_type icy::to_string(const icy::key_mod mod, string& str) noexcept
{
    const auto func = [&str, &mod](const key_mod left, const key_mod right, const string_view string) -> error_type
    {
        if (mod & (left | right))
        {
            if ((mod & left) == 0)
            {
                ICY_ERROR(str.append("Right "_s));
            }
            else if ((mod & right) == 0)
            {
                ICY_ERROR(str.append("Left "_s));
            }
            ICY_ERROR(str.append(string));
            ICY_ERROR(str.append(" + "_s));
        }
        return {};
    };
    ICY_ERROR(func(key_mod::lctrl, key_mod::rctrl, "Ctrl"_s));
    ICY_ERROR(func(key_mod::lalt, key_mod::ralt, "Alt"_s));
    ICY_ERROR(func(key_mod::lshift, key_mod::rshift, "Shift"_s));
    return {};
}
error_type icy::to_string(const input_message& input, string& str) noexcept
{
	if (input.type == input_type::active || input.type == input_type::text || input.key == key::none)
		return {};

    ICY_ERROR(to_string(key_mod(input.mods), str));
	for (auto&& pair : key_array)
	{
		if (pair.key == input.key)
			return str.append(pair.str);
	}
	return make_stdlib_error(std::errc::invalid_argument);
}
string_view icy::to_string(const input_type type) noexcept
{
    switch (type)
    {
    case input_type::key_release:
        return "Key Release"_s;
    case input_type::key_hold:
        return "Key Hold"_s;
    case input_type::key_press:
        return "Key Press"_s;
    case input_type::mouse_move:
        return "Mouse Move"_s;
    case input_type::mouse_wheel:
        return "Mouse Wheel"_s;
    case input_type::mouse_release:
        return "Mouse Release"_s;
    case input_type::mouse_press:
        return "Mouse Press"_s;
    case input_type::mouse_hold:
        return "Mouse Hold"_s;
    case input_type::mouse_double:
        return "Mouse Double"_s;
    case input_type::text:
        return "Text"_s;
    case input_type::active:
        return "Activate"_s;
    }
    return ""_s;
}
string_view icy::to_string(const key key) noexcept
{
    for (auto&& pair : key_array)
    {
        if (pair.key == key)
            return pair.str;
    }
    return ""_s;
}

const uint32_t icy::wheel_delta = WHEEL_DELTA;