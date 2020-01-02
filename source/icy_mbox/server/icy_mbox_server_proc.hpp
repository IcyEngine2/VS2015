#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_qtgui/icy_qtgui.hpp>

struct mbox_process
{
    static icy::error_type copy(const mbox_process& src, mbox_process& dst) noexcept
    {
        dst.process_id = src.process_id;
        dst.uid = src.uid;
        ICY_ERROR(to_string(src.computer_name, dst.computer_name));
        ICY_ERROR(to_string(src.window_name, dst.window_name));
        ICY_ERROR(to_string(src.profile, dst.profile));
        ICY_ERROR(to_string(src.application, dst.application));
        ICY_ERROR(to_string(src.address, dst.address));
        return {};
    }
    uint32_t process_id = 0;
    icy::string computer_name;
    icy::string window_name;
    icy::string profile;
    icy::string application;
    icy::string address;
    icy::guid uid;
};
class mbox_processes
{
    enum
    {
        col_application,
        col_profile,
        col_address,
        col_window_name,
        col_computer_name,
        col_process_id,
        col_uid,
        _col_count,
    };
public:
    icy::error_type initialize(icy::gui_queue& gui, icy::gui_widget parent)
    {
        using namespace icy;
        m_gui = &gui;
        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::grid_view, parent));
        ICY_ERROR(m_gui->create(m_root));
        ICY_ERROR(m_gui->insert_cols(m_root, 0, _col_count));
        ICY_ERROR(m_gui->bind(m_widget, m_root));
        return {};
    }
    /*icy::error_type remove(const mbox_process& proc) noexcept
    {
        using namespace icy;
        if (!node || node.row() >= m_data.size())
            return make_stdlib_error(std::errc::invalid_argument);

        for (auto k = node.row() + 1; k < m_data.size(); ++k)
            m_data[k - 1] = std::move(m_data[k]);
        m_data.pop_back();
        ICY_ERROR(m_gui->remove_rows(m_root, node.row(), 1));
        return {};
    }*/
    icy::error_type update(const mbox_process& new_proc)
    {
        auto index = 0u;
        for (; index < m_data.size(); ++index)
        {
            if (new_proc.process_id == m_data[index].process_id && new_proc.computer_name == m_data[index].computer_name)
                break;
        }
        if (index == m_data.size())
        {
            mbox_process copy;
            ICY_ERROR(mbox_process::copy(new_proc, copy));
            ICY_ERROR(m_data.push_back(std::move(copy)));
            ICY_ERROR(m_gui->insert_rows(m_root, m_data.size(), 1));
        }
        else
        {
            if (new_proc.application.empty())
            {
                ICY_ERROR(m_gui->remove_rows(m_root, index, 1));
                for (auto k = index + 1; k < m_data.size(); ++k)
                    m_data[k - 1] = std::move(m_data[k]);
                m_data.pop_back();
                return {};
            }
        }
        icy::string str_proc_id;
        icy::string str_proc_uid;
        const auto& proc = m_data[index];
        ICY_ERROR(to_string(proc.process_id, str_proc_id));
        ICY_ERROR(to_string(proc.uid, str_proc_uid));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_application), proc.application));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_profile), proc.profile));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_address), proc.address));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_window_name), proc.window_name));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_computer_name), proc.computer_name));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_process_id), str_proc_id));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_uid), str_proc_uid));
        return {};
    }
    icy::gui_widget widget() const noexcept
    {
        return m_widget;
    }
    icy::const_array_view<mbox_process> data() const noexcept
    {
        return m_data;
    }
private:
    icy::gui_queue* m_gui = nullptr;
    icy::gui_widget m_widget;
    icy::gui_node m_root;
    icy::array<mbox_process> m_data;
};