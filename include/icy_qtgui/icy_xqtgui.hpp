#pragma once

#include "icy_qtgui.hpp"

namespace icy
{
    extern weak_ptr<gui_queue> global_gui;

    class xgui_widget : public gui_widget
    {
    public:
        xgui_widget() noexcept = default;
        xgui_widget(xgui_widget&& rhs) noexcept
        {
            std::swap(index, rhs.index);
        }
        ICY_DEFAULT_MOVE_ASSIGN(xgui_widget);
        ~xgui_widget() noexcept
        {
            if (*this)
            {
                if (auto gui = shared_ptr<gui_queue>(global_gui))
                    gui->destroy(*this);
            }
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::auto_insert) noexcept
        {
            gui_widget widget;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                ICY_ERROR(gui->create(widget, type, parent, flags));
                if (*this)
                    gui->destroy(*this);

            }
            index = widget.index;
            return error_type();
        }
        error_type initialize(HWND__* const win32, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::auto_insert) noexcept
        {
            error_type error;
            gui_widget widget;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                ICY_ERROR(gui->create(widget, win32, parent, flags));
                if (*this)
                    error = gui->destroy(*this);
            }
            index = widget.index;
            return error;
        }
        error_type insert(const gui_insert args) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->insert(*this, args);
            return error_type();
        }
        error_type insert(const gui_action action) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->insert(*this, action);
            return error_type();
        }
        error_type show(const bool value) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->show(*this, value);
            return error_type();
        }
        error_type text(const string_view text) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->text(*this, text);
            return error_type();
        }
        error_type enable(const bool value) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->enable(*this, value);
            return error_type();
        }
        error_type icon(const gui_image icon) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->icon(*this, icon);
            return error_type();
        }
        error_type bind(const gui_node node) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->bind(*this, node);
            return error_type();
        }
        error_type modify(const string_view args) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->modify(*this, args);
            return error_type();
        }
        //  for menu
        error_type exec(gui_action& action) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->exec(*this, action);
            return error_type();
        }
        error_type scroll(const gui_node node) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->scroll(*this, node);
            return error_type();
        }
        error_type scroll(const gui_widget widget) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->scroll(*this, widget);
            return error_type();
        }
        error_type input(const input_message& msg) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->input(*this, msg);
            return error_type();
        }
    };
    class xgui_text_edit : public xgui_widget
    {
    public:
        error_type initialize(gui_widget parent, gui_widget_flag flags = gui_widget_flag::auto_insert) noexcept
        {
            return xgui_widget::initialize(gui_widget_type::text_edit, parent, flags);
        }
        error_type append(const string_view text) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->append(*this, text);
            return error_type();
        }
    };
    class xgui_image : public icy::gui_image
    {
    public:
        xgui_image() noexcept = default;
        xgui_image(xgui_image&& rhs) noexcept
        {
            std::swap(index, rhs.index);
        }
        ICY_DEFAULT_MOVE_ASSIGN(xgui_image);
        ~xgui_image() noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                if (*this)
                    gui->destroy(*this);
            }
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const const_matrix_view<color> colors) noexcept
        {
            error_type error;
            gui_image image;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                ICY_ERROR(gui->create(image, colors));
                if (*this)
                    error = gui->destroy(*this);
            }
            index = image.index;
            return error;
        }
    };
    class xgui_action : public icy::gui_action
    {
    public:
        xgui_action() noexcept = default;
        xgui_action(xgui_action&& rhs) noexcept
        {
            std::swap(index, rhs.index);
        }
        ICY_DEFAULT_MOVE_ASSIGN(xgui_action);
        ~xgui_action() noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                if (*this)
                    gui->destroy(*this);
            }
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const string_view text) noexcept
        {
            gui_action action;
            error_type error;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                ICY_ERROR(gui->create(action, text));
                if (*this)
                    error = gui->destroy(*this);
            }
            index = action.index;
            return error;
        }
        error_type enable(const bool value) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->enable(*this, value);
            return error_type();
        }
        error_type icon(const gui_image value) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->icon(*this, value);
            return error_type();
        }
        error_type bind(const gui_widget menu) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->bind(*this, menu);
            return error_type();
        }
        error_type keybind(const input_message& key) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->keybind(*this, key);
            return error_type();
        }
        error_type keybind(const gui_shortcut shortcut) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->keybind(*this, shortcut);
            return error_type();
        }
    };
    class xgui_node : public icy::gui_node
    {
    public:
        xgui_node() noexcept = default;
        xgui_node(icy::gui_node node) noexcept : gui_node(node)
        {

        }
        xgui_node(xgui_node&& rhs) noexcept : gui_node(std::move(rhs))
        {

        }
        xgui_node(const xgui_node& rhs) noexcept : gui_node(rhs)
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(xgui_node);
        ICY_DEFAULT_MOVE_ASSIGN(xgui_node);
        using gui_node::udata;
        explicit operator bool() const noexcept
        {
            return _ptr != nullptr;
        }
        error_type insert_rows(const size_t offset, const size_t count) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->insert_rows(*this, offset, count);
            return error_type();
        }
        error_type insert_cols(const size_t offset, const size_t count) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->insert_cols(*this, offset, count);
            return error_type();
        }
        error_type remove_rows(const size_t offset, const size_t count) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->remove_rows(*this, offset, count);
            return error_type();
        }
        error_type remove_cols(const size_t offset, const size_t count) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->remove_cols(*this, offset, count);
            return error_type();
        }
        error_type move_rows(const size_t offset_src, const size_t count, const size_t offset_dst) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->move_rows(*this, offset_src, count, *this, offset_dst);
            return error_type();
        }
        error_type move_cols(const size_t offset_src, const size_t count, const size_t offset_dst) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->move_cols(*this, offset_src, count, *this, offset_dst);
            return error_type();
        }
        xgui_node node(const size_t row, const size_t col) const noexcept
        {
            xgui_node node;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                auto find = gui->node(*this, row, col);
                static_cast<gui_node&>(node) = find;
            }
            return node;
        }
        error_type text(const string_view text) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->text(*this, text);
            return error_type();
        }
        error_type udata(const gui_variant& var) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->udata(*this, var);
            return error_type();
        }
        error_type icon(const gui_image image) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->icon(*this, image);
            return error_type();
        }
        xgui_node parent() const noexcept
        {
            xgui_node node;
            static_cast<gui_node&>(node) = gui_node::parent();
            return node;
        }
    };
    class xgui_model : public xgui_node
    {
    public:
        xgui_model() noexcept = default;
        xgui_model(xgui_model&& rhs) noexcept : xgui_node(std::move(rhs))
        {

        }
        xgui_model(const xgui_model& rhs) noexcept = delete;
        ICY_DEFAULT_MOVE_ASSIGN(xgui_model);
        ~xgui_model() noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                if (*this)
                    gui->destroy(*this);
            }
        }
        explicit operator bool() const noexcept
        {
            return _ptr != nullptr;
        }
        error_type initialize() noexcept
        {
            gui_node model;
            error_type error;
            if (auto gui = shared_ptr<gui_queue>(global_gui))
            {
                ICY_ERROR(gui->create(model));
                if (*this)
                    error = gui->destroy(*this);
            }
            static_cast<gui_node&>(*this) = model;
            return error;
        }
        error_type vheader(const uint32_t index, const string_view text) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->vheader(*this, index, text);
            return error_type();
        }
        error_type hheader(const uint32_t index, const string_view text) noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->hheader(*this, index, text);
            return error_type();
        }
        error_type clear() noexcept
        {
            if (auto gui = shared_ptr<gui_queue>(global_gui))
                return gui->clear(*this);
            return error_type();
        }
        uint64_t model() const noexcept
        {
            return _ptr ? _ptr->model() : 0;
        }
    };
    class xgui_submenu
    {
    public:
        error_type initialize(const string_view text) noexcept
        {
            ICY_ERROR(widget.initialize(gui_widget_type::menu, {}, gui_widget_flag::none));
            ICY_ERROR(action.initialize(text));
            ICY_ERROR(action.bind(widget));
            return error_type();
        }
        xgui_widget widget;
        xgui_action action;
    };

    inline error_type xgui_rename(string& str) noexcept
    {
        xgui_widget dialog;
        ICY_ERROR(dialog.initialize(gui_widget_type::dialog_input_line, gui_widget()));
        ICY_ERROR(dialog.text(str));
        shared_ptr<event_queue> loop;
        ICY_ERROR(create_event_system(loop, event_type::gui_update));
        ICY_ERROR(dialog.show(true));
        event event;
        ICY_ERROR(loop->pop(event));
        if (event->type == event_type::gui_update)
        {
            auto& event_data = event->data<gui_event>();
            if (event_data.widget == dialog)
            {
                const auto new_str = event_data.data.as_string();
                if (!new_str.empty())
                    ICY_ERROR(to_string(new_str, str));
            }
        }
        return error_type();
    }
}

namespace icy
{  
    inline error_type show_error(const error_type error, const string_view text) noexcept
    {
        string msg;
        if (error)
        {
            ICY_ERROR(to_string("Error: %4 - %1 code [%2]: %3", msg, error.source, long(error.code), error, text));
        }
        else
        {
            ICY_ERROR(to_string("Error: %1"_s, msg, text));
        }

        if (auto gui = shared_ptr<gui_queue>(global_gui))
        {
            xgui_widget window;
            ICY_ERROR(window.initialize(gui_widget_type::message, gui_widget()));
            ICY_ERROR(window.text(msg));
            ICY_ERROR(window.show(true));
            shared_ptr<event_queue> loop;
            ICY_ERROR(create_event_system(loop, event_type::gui_update | event_type::gui_action));
            while (gui->is_running())
            {
                event event;
                const auto error = loop->pop(event, std::chrono::milliseconds(200));
                if (error == make_stdlib_error(std::errc::timed_out))
                    continue;
                ICY_ERROR(error);

                if (event->type == event_type::global_quit)
                    return error_type();

                if (event->type == event_type::gui_update || event->type == event_type::gui_action)
                {
                    auto& event_data = event->data<gui_event>();
                    if (event_data.widget == window)
                        return error_type();
                }
            }
        }
        ICY_ERROR(win32_message(msg, "Error"_s));
        return error_type();
    }
}