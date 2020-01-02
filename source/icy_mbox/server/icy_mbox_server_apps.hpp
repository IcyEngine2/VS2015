#pragma once

#include <icy_qtgui/icy_qtgui.hpp>

class mbox_applications
{
public:
    icy::error_type initialize(icy::gui_queue& gui, icy::gui_widget parent)
    {
        using namespace icy;
        m_gui = &gui;
        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::combo_box, parent));
        ICY_ERROR(m_gui->create(m_root));
        ICY_ERROR(m_gui->insert_cols(m_root, 0, 1));
        ICY_ERROR(m_gui->bind(m_widget, m_root));
        return {};
    }
    icy::error_type append(const icy::string_view application)
    {
        using namespace icy;
        string str;
        ICY_ERROR(to_string(application, str));
        ICY_ERROR(m_data.push_back(std::move(str)));
        ICY_ERROR(m_gui->insert_rows(m_root, m_data.size(), 1));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, m_data.size() - 1, 0), application));
        return {};
    }
    icy::error_type remove(const icy::gui_node node) noexcept
    {
        using namespace icy;
        if (!node || node.row() >= m_data.size())
            return make_stdlib_error(std::errc::invalid_argument);
        
        for (auto k = node.row() + 1; k < m_data.size(); ++k)
            m_data[k - 1] = std::move(m_data[k]);
        m_data.pop_back();
        ICY_ERROR(m_gui->remove_rows(m_root, node.row(), 1));
        return {};
    }
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
    icy::string_view find(const icy::gui_node node) const noexcept
    {
        if (node.row() < m_data.size())
            return m_data[node.row()];
        else
            return ""_s;
    }
private:
    icy::gui_queue* m_gui = nullptr;
    icy::gui_widget m_widget;
    icy::gui_node m_root;
    icy::array<icy::string> m_data;
};