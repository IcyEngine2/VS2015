#pragma once

#include <icy_module/icy_server.hpp>
#include "auth_core.hpp"

class auth_database;

class auth_network_system : public icy::server_network_system
{
public:
    auth_network_system(auth_database& dbase) noexcept : m_dbase(dbase)
    {

    }
    icy::error_type initialize(const icy::server_config_network& config, const size_t cores, const icy::const_array_view<icy::auth_request_type> types) noexcept
    {
        ICY_ERROR(server_network_system::initialize(config, cores));
        return m_types.assign(types);
    }
private:
    icy::error_type exec(const icy::json& input, icy::json& output, const icy::network_address& address) noexcept override;
private:
    auth_database& m_dbase;
    icy::array<icy::auth_request_type> m_types;
};