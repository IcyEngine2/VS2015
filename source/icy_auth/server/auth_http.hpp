#pragma once

#include "auth_config.hpp"
#include <icy_engine/core/icy_thread.hpp>

class auth_database;

class auth_network_system;
class auth_network_thread : public icy::thread
{
public:
    auth_network_thread(auth_network_system& system) noexcept : m_system(system)
    {

    }
private:
    icy::error_type run() noexcept override;
private:
    auth_network_system& m_system;
};
class auth_network_system
{
    friend auth_network_thread;
public:
    auth_network_system(auth_database& dbase) noexcept : m_dbase(dbase)
    {

    }
    icy::error_type initialize(const auth_config_network& config, const size_t cores, const icy::const_array_view<icy::auth_request_type> types) noexcept;
    icy::error_type wait() noexcept;
    icy::error_type launch() noexcept;
    icy::error_type status(icy::string& msg) const noexcept;
private:
    icy::error_type process(const icy::string_view str_request, const icy::network_address& address, icy::json& json_response) noexcept;
private:
    auth_database& m_dbase;
    auth_config_network m_config;
    icy::array<icy::auth_request_type> m_types;
    icy::network_system m_network;
    icy::http_system m_http;
    icy::array<auth_network_thread> m_threads;
};