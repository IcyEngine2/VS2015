#pragma once

#include <icy_engine/core/icy_string_view.hpp>

enum class dcl_text : uint32_t
{
    none,
    directory,
    version_client,
    version_server,
    network,
    connect,
    update,
    upload,
    options,
    open,
    open_in_new_tab,
    rename,
    advanced,
    move_to,
    hide,
    show,
    lock,
    unlock,
    destroy,
    restore,
    access,
    create,
    locale,
    path,
    name,
    okay,
    cancel,
};
namespace icy { class string_view; }
icy::string_view to_string(const dcl_text text) noexcept;
icy::string_view to_string_enUS(const dcl_text text) noexcept;