#pragma once

#include <icy_engine/core/icy_string_view.hpp>
#include <icy_engine/core/icy_json.hpp>

using icy::operator""_s;

class mbox_config
{
public:
    struct key
    {
        static constexpr icy::string_view applications = "Applications"_s;
        static constexpr icy::string_view ipaddr = "IP Address"_s;
        static constexpr icy::string_view inject = "Inject"_s;
        static constexpr icy::string_view maxconn = "Max Connections"_s;
    };
public:
    icy::error_type from_json(const icy::json& input) noexcept
    {
        using namespace icy;
        if (const auto json_apps = input.find(key::applications))
        {
            for (auto k = 0u; k < json_apps->keys().size(); ++k)
            {
                const string_view key = json_apps->keys()[k];
                const string_view val = json_apps->vals()[k].get();
                if (!val.empty() && !key.empty())
                    ICY_ERROR(emplace(applications, key, val));
            }
        }
        input.get(key::inject, inject);
        input.get(key::ipaddr, ipaddr);
        input.get(key::maxconn, maxconn);
        return {};
    }
    icy::error_type to_json(icy::json& output) const noexcept
    {
        using namespace icy;
        output = json_type::object;
        json json_apps = json_type::object;
        for (auto&& pair : applications)
            ICY_ERROR(json_apps.insert(pair.key, pair.value));
        ICY_ERROR(output.insert(key::inject, inject));
        ICY_ERROR(output.insert(key::ipaddr, ipaddr));
        ICY_ERROR(output.insert(key::maxconn, maxconn));
        return {};
    }
    static icy::error_type copy(const mbox_config& src, mbox_config& dst) noexcept
    {
        dst.applications.clear();
        for (auto&& pair : src.applications)
            ICY_ERROR(emplace(dst.applications, pair.key, pair.value));
        ICY_ERROR(to_string(src.inject, dst.inject));
        ICY_ERROR(to_string(src.ipaddr, dst.ipaddr));
        dst.maxconn = src.maxconn;
        return {};
    }
public:
    icy::map<icy::string, icy::string> applications;
    icy::string ipaddr;
    icy::string inject;
    uint16_t maxconn;
};