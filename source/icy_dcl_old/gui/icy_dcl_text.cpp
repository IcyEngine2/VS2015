#include "icy_dcl_gui.hpp"

using namespace icy;

dcl_gui_locale::type dcl_gui_locale::global = dcl_gui_locale::enUS;

static string_view to_string_enUS(const dcl_type type) noexcept
{
    switch (type)
    {
    case dcl_type::directory: return "Directory"_s;
    case dcl_type::name: return "Name"_s;
    case dcl_type::user: return "User"_s;
    case dcl_type::locale: return "Locale"_s;
    case dcl_type::enum_class: return "Enumeration"_s;
    case dcl_type::flags_class: return "Flags"_s;
    case dcl_type::resource_class: return "Resource"_s;

    case dcl_type::flags_value: return "Value"_s;
    case dcl_type::enum_value: return "Value"_s;
    }
    return ""_s;
}

string_view icy::to_string(const dcl_type type) noexcept
{
    /*switch (dcl_gui_locale::global)
    {

    }*/
    return to_string_enUS(type);
}

ICY_DECLARE_GLOBAL(dcl_gui_text::File) = 
{ 
    { dcl_gui_locale::enUS, u8"File"_s } 
};

ICY_DECLARE_GLOBAL(dcl_gui_text::New) =
{
    { dcl_gui_locale::enUS, u8"New"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Open) =
{
    { dcl_gui_locale::enUS, u8"Open"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Close) =
{
    { dcl_gui_locale::enUS, u8"Close"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Save) =
{
    { dcl_gui_locale::enUS, u8"Save"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::SaveAs) =
{
    { dcl_gui_locale::enUS, u8"Save As"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Context) =
{
    { dcl_gui_locale::enUS, u8"Context Menu"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Type) =
{
    { dcl_gui_locale::enUS, u8"Type"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Other) =
{
    { dcl_gui_locale::enUS, u8"Other"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Edit) =
{
    { dcl_gui_locale::enUS, u8"Edit"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Rename) =
{
    { dcl_gui_locale::enUS, u8"Rename"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Delete) =
{
    { dcl_gui_locale::enUS, u8"Delete"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Hide) =
{
    { dcl_gui_locale::enUS, u8"Hide"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Show) =
{
    { dcl_gui_locale::enUS, u8"Show"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Lock) =
{
    { dcl_gui_locale::enUS, u8"Lock"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Unlock) =
{
    { dcl_gui_locale::enUS, u8"Unlock"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::ShowLog) =
{
    { dcl_gui_locale::enUS, u8"Show Log"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Index) =
{
    { dcl_gui_locale::enUS, u8"Index"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Name) =
{
    { dcl_gui_locale::enUS, u8"Name"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Parent) =
{
    { dcl_gui_locale::enUS, u8"Parent"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Cancel) =
{
    { dcl_gui_locale::enUS, u8"Cancel"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Properties) =
{
    { dcl_gui_locale::enUS, u8"Properties"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Undo) =
{
    { dcl_gui_locale::enUS, u8"Undo"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Redo) =
{
    { dcl_gui_locale::enUS, u8"Redo"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Actions) =
{
    { dcl_gui_locale::enUS, u8"Actions"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Database) =
{
    { dcl_gui_locale::enUS, u8"Database"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Patch) =
{
    { dcl_gui_locale::enUS, u8"Patch"_s }
};

ICY_DECLARE_GLOBAL(dcl_gui_text::Apply) =
{
    { dcl_gui_locale::enUS, u8"Apply"_s }
};