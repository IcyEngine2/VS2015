#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_qtgui/icy_qtgui.hpp>

struct mbox_process
{
    static icy::error_type copy(const mbox_process& src, mbox_process& dst) noexcept
    {
        dst.key = src.key;
        dst.profile = src.profile;
        ICY_ERROR(to_string(src.computer_name, dst.computer_name));
        ICY_ERROR(to_string(src.window_name, dst.window_name));
        ICY_ERROR(to_string(src.application, dst.application));
        ICY_ERROR(to_string(src.address, dst.address));
        return {};
    }
    mbox::key key;
    icy::string computer_name;
    icy::string window_name;
    icy::guid profile;
    icy::string application;
    icy::string address;
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
        _col_count,
    };
public:
    icy::error_type initialize(icy::gui_queue& gui, mbox::library& library, icy::gui_widget parent)
    {
        using namespace icy;
        m_gui = &gui;
        m_library = &library;

        ICY_ERROR(m_gui->create(m_widget, gui_widget_type::grid_view, parent));
        ICY_ERROR(m_gui->create(m_root));
        ICY_ERROR(m_gui->insert_cols(m_root, 0, _col_count));

        ICY_ERROR(m_gui->hheader(m_root, col_application, "Application"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_profile, "Profile"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_address, "Address"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_window_name, "Window"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_computer_name, "Computer"_s));
        ICY_ERROR(m_gui->hheader(m_root, col_process_id, "Process"_s));

        ICY_ERROR(m_gui->bind(m_widget, m_root));

        return {};
    }
    icy::error_type update(const mbox_process& new_proc)
    {
        auto index = 0u;
        for (; index < m_data.size(); ++index)
        {
            if (new_proc.key == m_data[index].key)
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
            else
            {
                ICY_ERROR(mbox_process::copy(new_proc, m_data[index]));
            }
        }
        icy::string str_proc_id;
        const auto& proc = m_data[index];
        ICY_ERROR(to_string(proc.key.pid, str_proc_id));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_application), proc.application));

        icy::string str_profile;
        if (proc.profile != icy::guid())
            m_library->path(proc.profile, str_profile);

        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_profile), str_profile));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_address), proc.address));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_window_name), proc.window_name));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_computer_name), proc.computer_name));
        ICY_ERROR(m_gui->text(m_gui->node(m_root, index, col_process_id), str_proc_id));
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
    mbox::library* m_library = nullptr;
    icy::gui_widget m_widget;
    icy::gui_node m_root;
    icy::array<mbox_process> m_data;
};