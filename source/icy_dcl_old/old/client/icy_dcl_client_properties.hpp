#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_dcl_dbase.hpp"

class dcl_client_properties_model : public icy::gui_model
{
public:
    dcl_client_properties_model(const dcl_database& dbase, const icy::dcl_index locale) noexcept : 
        m_dbase(dbase), m_locale(locale)
    {

    }
    icy::error_type select(const icy::dcl_index& index) noexcept;
    uint32_t data(const icy::gui_node node, const icy::gui_role role, icy::gui_variant& value) const noexcept override;
private:
    struct pair
    {
        icy::dcl_index index;
        icy::string key;
        icy::string val;
    };
private:
    const dcl_database& m_dbase;
    const icy::dcl_index m_locale;
    icy::array<pair> m_data;
};