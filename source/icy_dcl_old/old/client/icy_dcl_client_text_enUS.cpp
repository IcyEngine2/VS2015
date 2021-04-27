#include "icy_dcl_client_text.hpp"
#include <icy_engine/core/icy_string_view.hpp>

using namespace icy;

string_view to_string(const dcl_text text) noexcept
{   
    if (false);
    else if (text == dcl_text::directory) return "Directory"_s;
    else if (text == dcl_text::version_client) return "Version (Client)"_s;
    else if (text == dcl_text::version_server) return "Version (Server)"_s;
    else if (text == dcl_text::network) return "Network"_s;
    else if (text == dcl_text::connect) return "Connect"_s;
    else if (text == dcl_text::update) return "Update"_s;
    else if (text == dcl_text::upload) return "Upload"_s;
    else if (text == dcl_text::options) return "Options"_s;
    else if (text == dcl_text::open) return "Open"_s;
    else if (text == dcl_text::open_in_new_tab) return "Open in new tab"_s;
    else if (text == dcl_text::rename) return "Rename"_s;
    else if (text == dcl_text::advanced) return "Advanced"_s;
    else if (text == dcl_text::move_to) return "Move to"_s;
    else if (text == dcl_text::hide) return "Hide"_s;
    else if (text == dcl_text::show) return "Show"_s;
    else if (text == dcl_text::lock) return "Lock"_s;
    else if (text == dcl_text::unlock) return "Unlock"_s;
    else if (text == dcl_text::destroy) return "Destroy"_s;
    else if (text == dcl_text::restore) return "Restore"_s;
    else if (text == dcl_text::access) return "Access"_s;
    else if (text == dcl_text::create) return "Create"_s;
    else if (text == dcl_text::locale) return "Locale"_s;
    else if (text == dcl_text::path) return "Path"_s;
    else if (text == dcl_text::name) return "Name"_s;
    else if (text == dcl_text::okay) return "Okay"_s;
    else if (text == dcl_text::cancel) return "Cancel"_s;
    else return ""_s;
}