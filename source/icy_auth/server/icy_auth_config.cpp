#include "icy_auth_config.hpp"

using namespace icy;

error_type auth_config_dbase::from_json(const json& input) noexcept
{
    ICY_ERROR(to_string(input.get(key::file_path), file_path));
    ICY_ERROR(input.get(key::file_size, file_size));
    file_size *= 1_mb;
    ICY_ERROR(input.get(key::clients, clients));
    ICY_ERROR(input.get(key::modules, modules));
    auto timeout_sec = 0u;    
    ICY_ERROR(input.get(key::timeout, timeout_sec));
    timeout = std::chrono::seconds(timeout_sec);
    return error_type();
}
error_type auth_config_dbase::to_json(json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key::file_size, json_type_integer(file_size / 1_mb)));
    ICY_ERROR(output.insert(key::file_path, file_path));
    ICY_ERROR(output.insert(key::clients, json_type_integer(clients)));
    ICY_ERROR(output.insert(key::modules, json_type_integer(modules)));
    ICY_ERROR(output.insert(key::timeout, std::chrono::duration_cast<std::chrono::seconds>(timeout).count()));
    return error_type();
}
error_type auth_config_dbase::copy(const auth_config_dbase& src, auth_config_dbase& dst) noexcept
{
    dst.file_size = src.file_size;
    ICY_ERROR(to_string(src.file_path, dst.file_path));
    dst.timeout = src.timeout;
    dst.clients = src.clients;
    dst.modules = src.modules;
    return error_type();
}

error_type auth_config_network::from_json(const json& input) noexcept
{
    input.get(key::file_size, file_size);
    input.get(key::file_path, file_path);
    file_size *= 1_mb;

    if (const auto json_http = input.find(key::http))
    {
        ICY_ERROR(to_value(*json_http, http));
    }
    else
    {
        return make_stdlib_error(std::errc::invalid_argument);
    }

    if (const auto json_errors = input.find(key::errors))
    {
        if (json_errors->type() != json_type::array)
            return make_stdlib_error(std::errc::invalid_argument);

        if (json_errors->size() == 0)
        {
            any_error = true;
        }
        else
        {
            for (auto&& json_error : json_errors->vals())
            {
                json_type_integer code = 0;
                if (const auto error = json_error.get(code))
                    return make_stdlib_error(std::errc::invalid_argument);

                if (code)
                {
                    string str;
                    ICY_ERROR(to_string(error_type(uint32_t(code), error_source_auth), str));
                }
                ICY_ERROR(errors.push_back(uint32_t(code)));
            }
        }
    }
    return error_type();
}
error_type auth_config_network::to_json(json& output) const noexcept
{
    output = json_type::object;

    string str_file_path;
    ICY_ERROR(to_string(file_path, str_file_path));
    ICY_ERROR(output.insert(key::file_size, json_type_integer(file_size / 1_mb)));
    ICY_ERROR(output.insert(key::file_path, std::move(str_file_path)));

    json json_http;
    ICY_ERROR(icy::to_json(http, json_http));
    ICY_ERROR(output.insert(key::http, std::move(json_http)));

    json json_errors = json_type::array;
    for (auto&& code : errors)
        ICY_ERROR(json_errors.push_back(json_type_integer(code)));

    if (!errors.empty())
    {
        ICY_ERROR(output.insert(key::errors, std::move(json_errors)));
    }
    return error_type();
}
error_type auth_config_network::copy(const auth_config_network& src, auth_config_network& dst) noexcept
{
    dst.file_size = src.file_size;
    ICY_ERROR(to_string(src.file_path, dst.file_path));
    ICY_ERROR(dst.errors.assign(src.errors));
    ICY_ERROR(icy::copy(src.http, dst.http));
    dst.any_error = src.any_error;
    return error_type();
}

error_type auth_config::from_json(const json& input) noexcept
{
    const auto json_dbase = input.find(key::dbase);
    const auto json_client = input.find(key::client);
    const auto json_module = input.find(key::module);
    const auto json_admin = input.find(key::admin);
    if (!json_client || !json_module || !json_admin || !json_dbase)
        return icy::make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(dbase.from_json(*json_dbase));
    ICY_ERROR(client.from_json(*json_client));
    ICY_ERROR(module.from_json(*json_module));
    ICY_ERROR(admin.from_json(*json_admin));

    input.get(key::gheap_size, gheap_size);
    if (!gheap_size) gheap_size = default_values::gheap_size;

    return error_type();
}
error_type auth_config::to_json(json& output) const noexcept
{
    output = icy::json_type::object;
    json json_dbase;
    json json_client;
    json json_module;
    json json_admin;
    ICY_ERROR(dbase.to_json(json_dbase));
    ICY_ERROR(client.to_json(json_client));
    ICY_ERROR(module.to_json(json_module));
    ICY_ERROR(admin.to_json(json_admin));
    ICY_ERROR(output.insert(key::dbase, std::move(json_dbase)));
    ICY_ERROR(output.insert(key::client, std::move(json_client)));
    ICY_ERROR(output.insert(key::module, std::move(json_module)));
    ICY_ERROR(output.insert(key::admin, std::move(json_admin)));
    ICY_ERROR(output.insert(key::gheap_size, gheap_size));
    return error_type();
}
error_type auth_config::copy(const auth_config& src, auth_config& dst) noexcept
{
    dst.gheap_size = src.gheap_size;
    ICY_ERROR(auth_config_dbase::copy(src.dbase, dst.dbase));
    ICY_ERROR(auth_config_network::copy(src.client, dst.client));
    ICY_ERROR(auth_config_network::copy(src.module, dst.module));
    ICY_ERROR(auth_config_network::copy(src.admin, dst.admin));
    return error_type();
}