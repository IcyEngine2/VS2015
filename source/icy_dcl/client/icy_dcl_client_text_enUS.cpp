#include "icy_dcl_client_text.hpp"

using namespace icy;

string_view to_string_enUS(const text text) noexcept
{   
    switch (text)
    {
    case text::version_client: return "Version (Client)"_s;
    case text::version_server: return "Version (Server)"_s;
    case text::network: return "Network"_s;
    case text::connect: return "Connect"_s;
    case text::update: return "Update"_s;
    case text::upload: return "Upload"_s;
    case text::project: return "Project"_s;
    case text::open: return "Open"_s;
    case text::close: return "Close"_s;
    case text::create: return "Create"_s;
    case text::save: return "Save"_s;
    case text::options: return "Options"_s;
    case text::error_open_project: return "An error has occured while opening user project";
    case text::error_create_project: return "An error has occured while creating user project";
    case text::ok: return "Ok";
    }
    return {};
}