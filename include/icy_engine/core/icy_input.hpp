#pragma once

#include "icy_core.hpp"
#include "icy_key.hpp"
#include "icy_string_view.hpp"
#include <bitset>

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union
#pragma warning(disable:4582)

struct tagMSG;

namespace icy
{
    enum class input_type : uint32_t
    {
        none,
        key_release,
        key_hold,
        key_press,
        mouse_move,
        mouse_wheel,
        mouse_release,
        mouse_press,
        mouse_hold,
        mouse_double,
        text,
        active,
        _total,
    };
    enum class key_mod : uint32_t
    {
        none    =   0x00,
        lctrl   =   0x01,
        lalt    =   0x02,
        lshift  =   0x04,
        rctrl   =   0x08,
        ralt    =   0x10,
        rshift  =   0x20,
            
        lmb     =   0x40,
        rmb     =   0x80,
        mmb     =   0x100,
        x1mb    =   0x200,
        x2mb    =   0x400,
            
        _max    =   lctrl | lalt | lshift | rctrl | ralt | rshift | lmb | rmb | mmb | x1mb | x2mb,
    };

    inline constexpr key_mod operator|(const key_mod lhs, const key_mod rhs) noexcept
    {
        return key_mod(uint32_t(lhs) | uint32_t(rhs));
    }
    inline constexpr uint32_t operator&(const key_mod lhs, const key_mod rhs) noexcept
    {
        return uint32_t(lhs) & uint32_t(rhs);
    }

	struct input_message
	{
		enum
		{
			u16_max = 2,
		};
		explicit operator bool() const noexcept
		{
			return type != input_type::none;
		}
		input_message() : type(input_type::none)
		{
            memset(this, 0, sizeof(*this));
		}
		input_message(const input_type type, const key key, const key_mod mods) noexcept : 
            type(type), key(key), mods(uint64_t(mods)), point_x(0), point_y(0), wheel(0)
		{

		}
        explicit input_message(const bool active) noexcept : type(input_type::active), active(active)
        {

        }
        explicit input_message(const wchar_t* const wstring) noexcept : type(input_type::text), text{}
		{
			for (auto k = 0_z; k < u16_max && wstring && wstring[k]; ++k)
				text[k] = wstring[k];
		}
		ICY_DEFAULT_COPY_ASSIGN(input_message);
        const input_type type;
        union
        {
            wchar_t text[u16_max];
            const bool active;
            key key;
        };
        struct
        {
            int64_t mods    : 0x10;
            int64_t point_x : 0x10;
            int64_t point_y : 0x10;
            int64_t wheel   : 0x10;
        };
	};
    inline void key_mod_set(key_mod& mod, const std::bitset<256>& buffer) noexcept
    {
        if (buffer[size_t(key::left_ctrl)]) mod = mod | key_mod::lctrl;
        if (buffer[size_t(key::left_alt)]) mod = mod | key_mod::lalt;
        if (buffer[size_t(key::left_shift)]) mod = mod | key_mod::lshift;
        if (buffer[size_t(key::right_ctrl)]) mod = mod | key_mod::rctrl;
        if (buffer[size_t(key::right_alt)]) mod = mod | key_mod::ralt;
        if (buffer[size_t(key::right_shift)]) mod = mod | key_mod::rshift;
    }
    namespace detail
    {       
		key scan_vk_to_key(uint16_t vkey, uint16_t scan, const bool isE0) noexcept;
		uint16_t from_winapi(input_message& msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256>& buffer) noexcept;
		uint16_t from_winapi(input_message& key, const tagMSG& msg, const std::bitset<256> & buffer) noexcept;
		void from_winapi(input_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const uint32_t msg, const size_t wParam, const ptrdiff_t lParam, const std::bitset<256> & buffer) noexcept;
		void from_winapi(input_message& mouse, const uint32_t offset_x, const uint32_t offset_y, const tagMSG& msg, const std::bitset<256>& buffer) noexcept;
		tagMSG to_winapi(const input_message& input) noexcept;
        void to_winapi(const input_message& input, uint32_t& msg, size_t& wParam, ptrdiff_t& lParam) noexcept;
    }
	inline auto key_mod_and(const key_mod lhs, const key_mod rhs) noexcept
	{
        auto need_any_ctrl = lhs & (key_mod::lctrl | key_mod::rctrl);
        auto need_any_alt = lhs & (key_mod::lalt| key_mod::ralt);
        auto need_any_shift = lhs & (key_mod::lshift | key_mod::rshift);
        if (need_any_ctrl)
        {
            if ((key_mod(need_any_ctrl) & rhs) == 0)
                return false;
        }
        if (need_any_alt)
        {
            if ((key_mod(need_any_alt) & rhs) == 0)
                return false;
        }
        if (need_any_shift)
        {
            if ((key_mod(need_any_shift) & rhs) == 0)
                return false;
        }
        return need_any_ctrl || need_any_alt || need_any_shift ? true : rhs == key_mod::none;
	}
    string_view to_string(const input_type type) noexcept;
	error_type to_string(const input_message& key, string& str) noexcept;
    error_type to_string(const key_mod mod, string& str) noexcept;
    string_view to_string(const key key) noexcept;
    
    extern const uint32_t wheel_delta;
}

#pragma warning(pop)