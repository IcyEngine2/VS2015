#include "icy_dcl_client_explorer.hpp"

using namespace icy;

error_type dcl_client_explorer_model::append(dcl_read_txn& txn, const dcl_index& key,
    const dcl_base_val& val, const uint64_t parent, uint64_t& row) noexcept
{
    if (val.type != dcl_directory)
        return {};

    pair pair;
    pair.index = key;
    pair.type = val.type;
    ICY_ERROR(txn.flags(key, pair.flags));

    string_view str;
    ICY_ERROR(txn.text(key, m_locale, str));
    ICY_ERROR(to_string(str, pair.name));

    gui_node node;
    node.par = parent;
    node.row = row++;
    ICY_ERROR(init(node));
    ICY_ERROR(m_data.push_back(std::move(pair)));
    
    const_array_view<dcl_index> indices;
    ICY_ERROR(txn.data_link(key, indices));

    auto next_row = 0ui64;
    for (auto&& index : indices)
    {
        dcl_base_val child;
        ICY_ERROR(txn.data_main(index, child));
        if (child.parent != key)
            continue;

        ICY_ERROR(append(txn, index, child, node.idx, next_row));
    }

    return {};
}
error_type dcl_client_explorer_model::initialize() noexcept
{
    m_data.clear();
    ICY_ERROR(m_data.resize(1));

    dcl_read_txn txn;
    ICY_ERROR(txn.initialize(m_dbase));

    dcl_base_val root;
    root.type = dcl_directory;
    root.value.index = dcl_directory;
    uint64_t row = 0;
    ICY_ERROR(append(txn, dcl_root, root, 0, row));
    ICY_ERROR(update({}));
    return {};
}
uint32_t dcl_client_explorer_model::data(const gui_node node, const gui_role role, gui_variant& value) const noexcept
{
    if (node.idx < m_data.size())
    {
        auto& base = m_data[node.idx];
        if (role == gui_role::view)
        {
            return make_variant(value, base.name).code;
        }
        else if (role == gui_role::user)
        {
            value = base.index;
            return 0;
        }
    }
    return uint32_t(std::errc::invalid_argument);
}
