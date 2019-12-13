#pragma once

#include <icy_engine/icy_input.hpp>
#include <icy_engine/icy_string.hpp>
#include <icy_engine/icy_win32.hpp>
#include <icy_engine/icy_utility.hpp>
#include <Windows.h>
#include <windowsx.h>

using namespace icy;

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
	case VK_CONTROL:    key = isE0 ? key::right_ctl     : key::left_ctl;    break;
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
uint16_t icy::detail::from_winapi(key_message& msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept
{
	fill_key_mod(msg, buffer);
	const auto key = scan_vk_to_key(LOWORD(wParam), WORD((lParam >> 16) & 0x00FF), !!((lParam >> 24) & 0x01));
	const auto event =
		(lParam >> 31) & 0x01 ? key_event::release :
		(lParam >> 30) & 0x01 ? key_event::hold :
		key_event::press;
	msg = key_message(key, event, msg.ctl, msg.alt, msg.shf);
	return LOWORD(lParam);
}
uint16_t icy::detail::from_winapi(key_message& key, const MSG& msg, const std::bitset<256> & buffer) noexcept
{
	return from_winapi(key, msg.wParam, msg.lParam, buffer);
}
void icy::detail::from_winapi(mouse_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const uint32_t msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept
{
	const auto word = LOWORD(wParam);
	mouse.lt = (word & MK_LBUTTON) ? 1 : 0;
	mouse.rt = (word & MK_RBUTTON) ? 1 : 0;
	mouse.md = (word & MK_MBUTTON) ? 1 : 0;
	mouse.x1 = (word & MK_XBUTTON1) ? 1 : 0;
	mouse.x2 = (word & MK_XBUTTON2) ? 1 : 0;

	fill_key_mod(mouse, buffer);

	auto point = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (msg == WM_MOUSEWHEEL)
    {
        point.x -= offset_x;
        point.y -= offset_y;
    }
	mouse.point = { float(point.x), float(point.y) };

	if (msg == WM_MOUSEMOVE)
	{
		mouse.event = mouse_event::move;
	}
	else if (msg == WM_MOUSEWHEEL)
	{
		mouse.wheel = GET_WHEEL_DELTA_WPARAM(wParam) / double(WHEEL_DELTA);
		mouse.event = mouse_event::wheel;
	}
	else
	{
		mouse.event = mouse_event::btn_press;

		switch (msg)
		{
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
			mouse.event = mouse_event::btn_release;
			break;
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
			mouse.event = mouse_event::btn_double;
			break;
		}

		mouse.button = mouse_button::lt;
		switch (msg)
		{
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
			mouse.button = mouse_button::rt;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			mouse.button = mouse_button::md;
			break;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
			mouse.button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ?
				mouse_button::x1 : mouse_button::x2;
		}
	}
}
void icy::detail::from_winapi(mouse_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const MSG& msg, const std::bitset<256> & buffer) noexcept
{
	return from_winapi(mouse, offset_x, offset_y, msg.message, msg.wParam, msg.lParam, buffer);
}
tagMSG icy::detail::to_winapi(const key_message& key) noexcept
{
	MSG msg = {};
    library lib("user32.dll");
    if (lib.initialize() == error_type{})
    {
        if (const auto func = ICY_FIND_FUNC(lib, MapVirtualKeyW))
        {
            msg.wParam = WPARAM(key.key);
            const auto scan = BYTE(func(unsigned(msg.wParam), MAPVK_VK_TO_VSC));
            msg.lParam = 0x0001 | // repeat
                (scan << 16) | // scan
                ((key.event == key_event::hold) << 30) |    // was down
                ((key.event == key_event::release) << 31);  // is up
            if (key_mod{ key.alt })
                msg.message = unsigned(key.event == key_event::release ? WM_SYSKEYUP : WM_SYSKEYDOWN);
            else
                msg.message = unsigned(key.event == key_event::release ? WM_KEYUP : WM_KEYDOWN);
        }
    }
	return msg;
}
tagMSG icy::detail::to_winapi(const mouse_message& mouse) noexcept
{
	uint32_t map[mouse_button::_total][mouse_event::_total]{};
	static const bool _init_once = [&map]
	{
		map[mouse_button::lt][mouse_event::btn_press] = WM_LBUTTONDOWN;
		map[mouse_button::lt][mouse_event::btn_hold] = WM_LBUTTONDOWN;
		map[mouse_button::lt][mouse_event::btn_release] = WM_LBUTTONUP;
		map[mouse_button::lt][mouse_event::btn_double] = WM_LBUTTONDBLCLK;

		map[mouse_button::rt][mouse_event::btn_press] = WM_RBUTTONDOWN;
		map[mouse_button::rt][mouse_event::btn_hold] = WM_RBUTTONDOWN;
		map[mouse_button::rt][mouse_event::btn_release] = WM_RBUTTONUP;
		map[mouse_button::rt][mouse_event::btn_double] = WM_RBUTTONDBLCLK;

		map[mouse_button::md][mouse_event::btn_press] = WM_MBUTTONDOWN;
		map[mouse_button::md][mouse_event::btn_hold] = WM_MBUTTONDOWN;
		map[mouse_button::md][mouse_event::btn_release] = WM_MBUTTONUP;
		map[mouse_button::md][mouse_event::btn_double] = WM_MBUTTONDBLCLK;

		map[mouse_button::x1][mouse_event::btn_press] = WM_XBUTTONDOWN;
		map[mouse_button::x1][mouse_event::btn_hold] = WM_XBUTTONDOWN;
		map[mouse_button::x1][mouse_event::btn_release] = WM_XBUTTONUP;
		map[mouse_button::x1][mouse_event::btn_double] = WM_XBUTTONDBLCLK;

		map[mouse_button::x2][mouse_event::btn_press] = WM_XBUTTONDOWN;
		map[mouse_button::x2][mouse_event::btn_hold] = WM_XBUTTONDOWN;
		map[mouse_button::x2][mouse_event::btn_release] = WM_XBUTTONUP;
		map[mouse_button::x2][mouse_event::btn_double] = WM_XBUTTONDBLCLK;

		return true;
	}();

	auto msg = MSG{};
	msg.lParam = MAKELONG(mouse.point.x, mouse.point.y);

	const auto shf = uint32_t(mouse.shf != 0);
	const auto ctl = uint32_t(mouse.ctl != 0);

	msg.wParam = 0
		| (mouse.lt << 0)
		| (mouse.rt << 1)
		| (mouse.md << 4)
		| (mouse.x1 << 5)
		| (mouse.x2 << 6)
		| (shf << 2)
		| (ctl << 3);

	switch (mouse.event)
	{
	case mouse_event::move:
	{
		msg.message = WM_MOUSEMOVE;
		break;
	}
	case mouse_event::wheel:
	{
		msg.message = WM_MOUSEWHEEL;
		const auto wheel = int16_t(mouse.wheel * WHEEL_DELTA);

		msg.wParam |= uint64_t(wheel << 16);
		break;
	}
	default:
	{
		msg.message = map[mouse.button][mouse.event];
		msg.wParam |=
			(mouse.button == mouse_button::x1) << 16 |
			(mouse.button == mouse_button::x2) << 17;
		break;
	}
	}
	return msg;
}
tagMSG icy::detail::to_winapi(const input_message& input) noexcept
{
	switch (input.type)
	{
	case input_type::key:
		return to_winapi(input.key);
	case input_type::mouse:
		return to_winapi(input.mouse);
	}
	return{};
}
error_type icy::to_string(const key_message& key, string& str)
{
	if (key.key == key::none) 
		return {};

	const auto func = [&str](const uint32_t val, const string_view string) -> error_type
	{
		if (const auto mod = key_mod{ val })
		{
			if (!mod.lt())
			{
				ICY_ERROR(str.append("rt "_s));
			}
			else if (!mod.rt())
			{
				ICY_ERROR(str.append("lt "_s));
			}
			ICY_ERROR(str.append(string));
			ICY_ERROR(str.append(" + "_s));
		}
		return {};
	};
	ICY_ERROR(func(key.ctl, "Ctrl"_s));
	ICY_ERROR(func(key.shf, "Shift"_s));
	ICY_ERROR(func(key.alt, "Alt"_s));
	for (auto&& pair : detail::key_array)
	{
		if (pair.second == key.key)
			return str.append(pair.first);
	}
	return make_stdlib_error(std::errc::invalid_argument);
}

const icy::detail::key_pair icy::detail::key_array[104] =
{
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
	{ "lt shf"_s, key::left_shf },
	{ "rt shf"_s, key::right_shf },
	{ "lt ctl"_s, key::left_ctl },
	{ "rt ctl"_s, key::right_ctl },
	{ "lt Alt"_s, key::left_alt },
	{ "rt Alt"_s, key::right_alt },
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
