#pragma once

#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_event.hpp>

class auth_config;

class auth_console_thread : public icy::thread
{
public:
    struct pair_type
    {
        icy::string help;
        icy::event_type type = icy::event_type::none;
    };
public:
    icy::error_type initialize(const auth_config& config) noexcept;
private:
    void cancel() noexcept override;
    icy::error_type run() noexcept override;
private:
    icy::map<icy::string_view, pair_type> m_commands;
};