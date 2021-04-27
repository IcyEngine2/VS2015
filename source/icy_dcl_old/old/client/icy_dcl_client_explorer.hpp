#pragma once

#include <icy_qtgui/icy_qtgui.hpp>
#include "../icy_dcl_dbase.hpp"

class dcl_client_explorer_model : public icy::gui_model
{
public:
    dcl_client_explorer_model(const dcl_database& dbase, const icy::dcl_index locale) noexcept : m_dbase(dbase), m_locale(locale)
    {

    }
    icy::error_type initialize() noexcept;
    uint32_t data(const icy::gui_node node, const icy::gui_role role, icy::gui_variant& value) const noexcept override;
private:
    struct pair
    {
        icy::dcl_index index;
        icy::dcl_index type;
        icy::string name;
        dcl_flag flags = dcl_flag::none;
    };
private:
    icy::error_type append(dcl_read_txn& txn, const icy::dcl_index& key, const dcl_base_val& val, const uint64_t parent, uint64_t& row) noexcept;
private:
    const dcl_database& m_dbase;
    const icy::dcl_index m_locale;
    icy::array<pair> m_data;
};