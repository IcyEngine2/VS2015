#pragma once

#include <icy_module/icy_server.hpp>
#include <icy_module/icy_auth.hpp>
#include <icy_engine/icy_event.hpp>
#include <icy_engine/icy_config.hpp>

using icy::operator""_s;

static constexpr auto auth_config_default_file_path = "auth_config.txt"_s;

class auth_config_dbase : public icy::server_config_dbase
{
public:
    struct key
    {
        //  integer (ms)
        static constexpr icy::string_view timeout = "Timeout"_s;
        static constexpr icy::string_view clients = "Clients"_s;
        static constexpr icy::string_view modules = "Modules"_s;
    };
public:
    icy::error_type initialize() noexcept
    {
        return server_config_dbase::initialize({ key::clients, key::modules });
    }
    icy::error_type from_json(const icy::json& input) noexcept
    {
        ICY_ERROR(server_config_dbase::from_json(input));
        icy::json_type_integer timeout_ms;
        input.get(key::timeout, timeout_ms);
        if (!timeout_ms)
            return icy::make_config_error(icy::config_error_code::is_required);
        timeout = std::chrono::milliseconds(timeout_ms);
        return {};
    }
    icy::error_type to_json(icy::json& output) const noexcept
    {
        ICY_ERROR(server_config_dbase::to_json(output));
        ICY_ERROR(output.insert(key::timeout, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()));
        return {};
    }
    static icy::error_type copy(const auth_config_dbase& src, auth_config_dbase& dst) noexcept
    {
        ICY_ERROR(server_config_dbase::copy(src, dst));
        dst.timeout = src.timeout;
        return {};
    }
    size_t clients() const noexcept
    {
        if (const auto ptr = capacity.try_find(key::clients))
            return *ptr;
        return 0;
    }
    size_t modules() const noexcept
    {
        if (const auto ptr = capacity.try_find(key::modules))
            return *ptr;
        return 0;
    }
public:
    icy::auth_clock::duration timeout = {};
};
class auth_config
{
public:
    struct default_values
    {
        static constexpr auto gheap_size = 64 << 20;
    };
    struct key
    {
        static constexpr icy::string_view gheap_size = "Heap Size"_s;
        static constexpr icy::string_view dbase = "Database"_s;
        static constexpr icy::string_view client = "Client"_s;
        static constexpr icy::string_view module = "Module"_s;
        static constexpr icy::string_view admin = "Admin"_s;
    };
public:
    icy::error_type initialize() noexcept
    {
        return dbase.initialize();
    }
    icy::error_type from_json(const icy::json& input) noexcept
    {
        icy::config config;

        const auto json_dbase = input.find(key::dbase);
        const auto json_client = input.find(key::client);
        const auto json_module = input.find(key::module);
        const auto json_admin = input.find(key::admin);
        if (!json_client || !json_module || !json_admin || !json_dbase)
            return icy::make_config_error(icy::config_error_code::is_required);

        ICY_ERROR(dbase.from_json(*json_dbase));
        ICY_ERROR(client.from_json(*json_client));
        ICY_ERROR(module.from_json(*json_module));
        ICY_ERROR(admin.from_json(*json_admin));

        ICY_ERROR(config.bind(key::gheap_size, icy::json_type_integer(default_values::gheap_size), icy::config::optional));
        ICY_ERROR(config(input, icy::hash64(key::gheap_size), gheap_size));
        return {};
    }
    icy::error_type to_json(icy::json& output) const noexcept
    {
        output = icy::json_type::object;
        icy::json json_dbase;
        icy::json json_client;
        icy::json json_module;
        icy::json json_admin;
        ICY_ERROR(dbase.to_json(json_dbase));
        ICY_ERROR(client.to_json(json_client));
        ICY_ERROR(module.to_json(json_module));
        ICY_ERROR(admin.to_json(json_admin));
        ICY_ERROR(output.insert(key::dbase, std::move(json_dbase)));
        ICY_ERROR(output.insert(key::client, std::move(json_client)));
        ICY_ERROR(output.insert(key::module, std::move(json_module)));
        ICY_ERROR(output.insert(key::admin, std::move(json_admin)));
        ICY_ERROR(output.insert(key::gheap_size, icy::json_type_integer(gheap_size)));
        return {};
    }
    static icy::error_type copy(const auth_config& src, auth_config& dst) noexcept
    {
        dst.gheap_size = src.gheap_size;
        ICY_ERROR(auth_config_dbase::copy(src.dbase, dst.dbase));
        ICY_ERROR(icy::server_config_network::copy(src.client, dst.client));
        ICY_ERROR(icy::server_config_network::copy(src.module, dst.module));
        ICY_ERROR(icy::server_config_network::copy(src.admin, dst.admin));
        return {};
    }
public:
    size_t gheap_size = default_values::gheap_size;
    auth_config_dbase dbase;
    icy::server_config_network client = icy::error_source_auth;
    icy::server_config_network module = icy::error_source_auth;
    icy::server_config_network admin = icy::error_source_auth;
};

namespace auth_event_type
{
    static constexpr auto stop_http_client = icy::event_user(0x00);
    static constexpr auto stop_http_module = icy::event_user(0x01);
    static constexpr auto stop_http_admin = icy::event_user(0x02);
    static constexpr auto run_http_client = icy::event_user(0x03);
    static constexpr auto run_http_module = icy::event_user(0x04);
    static constexpr auto run_http_admin = icy::event_user(0x05);
    static constexpr auto print_config = icy::event_user(0x06);
    static constexpr auto print_clients = icy::event_user(0x07);
    static constexpr auto print_modules = icy::event_user(0x08);
    static constexpr auto print_status = icy::event_user(0x09);
    static constexpr auto console_write = icy::event_user(0x0A);
    static constexpr auto console_error = icy::event_user(0x0B);
    static constexpr auto clear_clients = icy::event_user(0x0C);
    static constexpr auto clear_modules = icy::event_user(0x0D);
}