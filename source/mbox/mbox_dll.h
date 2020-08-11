#pragma once

#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_json.hpp>

#if _USRDLL
extern "C" __declspec(dllexport) ptrdiff_t __stdcall GetMessageHook(int code, size_t wParam, ptrdiff_t lParam);
#else
static constexpr auto GetMessageHook = "GetMessageHook";
#endif

struct HWND__;

namespace mbox
{
    enum class mbox_pause_type : uint32_t
    {
        none,
        toggle,
        enable,
        disable,
    };
    struct mbox_pause
    {
        icy::error_type to_json(icy::json& json) const noexcept
        {
            using namespace icy;
            json = json_type::object;
            ICY_ERROR(json.insert("pause.type"_s, uint32_t(type)));
            ICY_ERROR(json.insert("input.type"_s, uint32_t(input.type)));
            ICY_ERROR(json.insert("input.key"_s, uint32_t(input.key)));
            ICY_ERROR(json.insert("input.mods"_s, uint32_t(input.mods)));
            return error_type();
        }
        icy::error_type from_json(const icy::json& json) noexcept
        {
            using namespace icy;
            if (json.type() != json_type::object)
                return make_stdlib_error(std::errc::invalid_argument);
            auto int_pause_type = 0u;
            auto int_input_type = 0u;
            auto int_input_key = 0u;
            auto int_input_mods = 0u;
            ICY_ERROR(json.get("pause.type"_s, int_pause_type));
            ICY_ERROR(json.get("input.type"_s, int_input_type));
            ICY_ERROR(json.get("input.key"_s, int_input_key));
            ICY_ERROR(json.get("input.mods"_s, int_input_mods));
            type = mbox_pause_type(int_pause_type);
            input = input_message(input_type(int_input_type), key(int_input_key), key_mod(int_input_mods));
            return error_type();
        }

        mbox_pause_type type = mbox_pause_type::none;
        icy::input_message input;
    };
    struct mbox_dll_config
    {
        icy::error_type save(const uint32_t process) noexcept;
        icy::error_type load() noexcept;
        HWND__* hwnd = nullptr;
        mbox_pause pause_array[0x010];
        icy::input_message event_array[0x100];
        size_t pause_count = 0;
        size_t event_count = 0;
    };
}