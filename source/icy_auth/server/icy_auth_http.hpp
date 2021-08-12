#pragma once

#include "icy_auth_config.hpp"
#include <icy_engine/core/icy_thread.hpp>
#include <icy_engine/core/icy_event.hpp>

class auth_database;

class auth_network_system
{
public:
    auth_network_system(auth_database& dbase) noexcept : m_dbase(dbase)
    {

    }
    icy::error_type initialize(const auth_config_network& config, const size_t cores, const icy::const_array_view<icy::auth_request_type> types) noexcept;
    icy::thread& thread() noexcept
    {
        return *m_thread;
    }
    icy::error_type status(icy::string& msg) const noexcept;
    icy::shared_ptr<icy::event_system> system_ptr() noexcept
    {
        return m_network;
    }
    icy::error_type process(const icy::string_view str_request, const icy::network_address& address, icy::json& json_response) noexcept;
private:
    auth_database& m_dbase;
    auth_config_network m_config;
    icy::array<icy::auth_request_type> m_types;
    //icy::network_system m_network;
    //icy::http_system m_http;
    icy::shared_ptr<icy::network_system_http_server> m_network;
    icy::shared_ptr<icy::event_thread> m_thread;
};