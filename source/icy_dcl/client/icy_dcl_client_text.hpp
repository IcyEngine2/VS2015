#pragma once

#include <icy_engine/core/icy_string_view.hpp>

enum class text : uint32_t;
extern icy::string_view to_string_enUS(const text text) noexcept;
extern icy::string_view to_string(const text text) noexcept;

enum class text : uint32_t
{
    none,
    version_client,
    version_server,
    network,
    connect,
    update,
    upload,
    project,
    open,
    close,
    create,
    save,
    options,
    error_open_project,
    error_create_project,
    ok,
};