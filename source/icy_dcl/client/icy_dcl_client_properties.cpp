#include "icy_dcl_client_properties.hpp"

using namespace icy;

uint32_t dcl_client_properties_model::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    if (node.row < m_data.size())
    {
        auto& base = m_data[node.row];
        if (role == gui_role::view)
        {
            return make_variant(value, node.col ? base.val : base.key).code;
        }
        else if (role == gui_role::user)
        {
            value = base.index;
            return 0;
        }
    }
    return uint32_t(std::errc::invalid_argument);
}
