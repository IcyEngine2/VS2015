#pragma once

#if _USRDLL
extern "C" __declspec(dllexport) ptrdiff_t __stdcall GetMessageHook(int code, size_t wParam, ptrdiff_t lParam);
#else
static constexpr auto GetMessageHook = "GetMessageHook";
#endif

namespace mbox
{
    static const auto config_path = "mbox_config/";
    static const auto config_version = "1.2";

    static const auto json_key_character = "character";
    static const auto json_key_connect = "connect";
    static const auto json_key_script_path = "script.path";
    static const auto json_key_script_group = "script.group";
    static const auto json_key_multicast = "multicast";
    static const auto json_key_pause_toggle = "pause.toggle";
    static const auto json_key_pause_begin = "pause.begin";
    static const auto json_key_pause_end = "pause.end";
    static const auto json_key_index = "index";
    static const auto json_key_name = "name";
    static const auto json_key_version = "version";
    static const auto json_key_type = "type";
    static const auto json_key_mods = "mods";
    static const auto json_key_key = "key";
    static const auto json_key_point_x = "point_x";
    static const auto json_key_point_y = "point_y";
    static const auto json_key_wheel = "wheel";
    static const auto json_key_input = "input";

    
}