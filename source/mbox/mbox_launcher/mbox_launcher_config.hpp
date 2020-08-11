#pragma once

#include "../mbox_dll.h"

struct mbox_config_type
{
public:
    struct game_type
    {
        icy::error_type to_json(icy::json& json) const noexcept;
        icy::error_type from_json(const icy::json& json) noexcept;
        icy::string tags;
        icy::string path;
    };
    struct focus_type
    {
        icy::error_type to_json(icy::json& json) const noexcept;
        icy::error_type from_json(const icy::json& json) noexcept;
        icy::input_message input;
        uint32_t index = 0;
    };
public:
    icy::error_type load() noexcept;
    icy::error_type edit() noexcept;
    icy::error_type save() const noexcept;
public:
    icy::map<icy::string, game_type> games;
    icy::array<mbox::mbox_pause> pause;
    icy::array<focus_type> focus;
    icy::string default_script;
    icy::guid default_party;
    icy::string default_game;
};