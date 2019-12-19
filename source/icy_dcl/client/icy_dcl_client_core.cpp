#include "icy_dcl_client_core.hpp"

using namespace icy;

error_type dcl_client_config::from_json(const json& input) noexcept
{
    input.get(key::hostname, hostname);
    input.get(key::username, username);
    input.get(key::password, password);
    input.get(key::system_path, system_path);
    input.get(key::system_size, system_size);
    input.get(key::user_path, user_path);
    input.get(key::user_size, user_size);
    system_size *= 1_mb;
    user_size *= 1_mb;
    return {};
}
error_type dcl_client_config::to_json(json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key::hostname, hostname));
    ICY_ERROR(output.insert(key::username, username));
    ICY_ERROR(output.insert(key::password, password));
    ICY_ERROR(output.insert(key::system_path, system_path));
    ICY_ERROR(output.insert(key::user_path, user_path));
    ICY_ERROR(output.insert(key::system_size, system_size / 1_mb));
    ICY_ERROR(output.insert(key::user_size, user_size / 1_mb));
    return {};
}
error_type dcl_client_config::copy(const dcl_client_config& src, dcl_client_config& dst) noexcept
{
    ICY_ERROR(to_string(src.hostname, dst.hostname));
    dst.username = src.username;
    ICY_ERROR(to_string(src.password, dst.password));
    ICY_ERROR(to_string(src.system_path, dst.system_path));
    ICY_ERROR(to_string(src.user_path, dst.user_path));
    dst.system_size = src.system_size;
    dst.user_size = src.user_size;
    return {};
}