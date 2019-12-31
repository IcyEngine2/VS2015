#include "icy_dcl_client_core.hpp"
#include <icy_dcl/icy_dcl.hpp>

using namespace icy;

error_type dcl_client_config::from_json(const json& input) noexcept
{
    input.get(key::hostname, hostname);
    input.get(key::username, username);
    input.get(key::password, password);
    input.get(key::filepath, filepath);
    input.get(key::filesize, filesize);
    string locale_str;
    input.get(key::locale, locale_str);
    if (locale_str.to_value(locale))
        locale = icy::dcl_locale_default;
    
    filesize *= 1_mb;
    return {};
}
error_type dcl_client_config::to_json(json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key::hostname, hostname));
    ICY_ERROR(output.insert(key::username, username));
    ICY_ERROR(output.insert(key::password, password));
    ICY_ERROR(output.insert(key::filepath, filepath));
    ICY_ERROR(output.insert(key::filesize, filesize / 1_mb));

    string locale_str;
    ICY_ERROR(to_string(locale, locale_str));
    ICY_ERROR(output.insert(key::locale, locale_str));

    return {};
}
error_type dcl_client_config::copy(const dcl_client_config& src, dcl_client_config& dst) noexcept
{
    ICY_ERROR(to_string(src.hostname, dst.hostname));
    dst.username = src.username;
    ICY_ERROR(to_string(src.password, dst.password));
    ICY_ERROR(to_string(src.filepath, dst.filepath));
    dst.filesize = src.filesize;
    dst.locale = src.locale;
    return {};
}