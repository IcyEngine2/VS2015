#pragma once

#include "icy_qtgui.hpp"

namespace icy
{
    extern icy::gui_queue* global_gui;

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
                global_gui->destroy(*this);
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const gui_widget_type type, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::auto_insert) noexcept
        {
            gui_widget widget;
            ICY_ERROR(global_gui->create(widget, type, parent, flags));
            if (*this)
                global_gui->destroy(*this);
            index = widget.index;
            return {};            
        }
        error_type initialize(HWND__* const win32, const gui_widget parent, const gui_widget_flag flags = gui_widget_flag::auto_insert) noexcept
        {
            gui_widget widget;
            ICY_ERROR(global_gui->create(widget, win32, parent, flags));
            if (*this)
                global_gui->destroy(*this);
            index = widget.index;
            return {};
        }
        error_type insert(const gui_insert args) noexcept
        {
            return global_gui->insert(*this, args);
        }
        error_type insert(const gui_action action) noexcept
        {
            return global_gui->insert(*this, action);
        }
        error_type show(const bool value) noexcept
        {
            return global_gui->show(*this, value);
        }
        error_type text(const string_view text) noexcept
        {
            return global_gui->text(*this, text);
        }
        error_type enable(const bool value) noexcept
        {
            return global_gui->enable(*this, value);
        }
        error_type icon(const gui_image icon) noexcept
        {
            return global_gui->icon(*this, icon);
        }
        error_type bind(const gui_node node) noexcept
        {
            return global_gui->bind(*this, node);
        }
        error_type modify(const string_view args) noexcept
        {
            return global_gui->modify(*this, args);
        }
        //  for menu
        error_type exec(gui_action& action) noexcept
        {
            return global_gui->exec(*this, action);
        }
        error_type scroll(const gui_node node) noexcept
        {
            return global_gui->scroll(*this, node);
        }
        error_type scroll(const gui_widget widget) noexcept
        {
            return global_gui->scroll(*this, widget);
        }
        error_type input(const input_message& msg) noexcept
        {
            return global_gui->input(*this, msg);
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
            if (*this)
                global_gui->destroy(*this);
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const const_matrix_view<color> colors) noexcept
        {
            gui_image image;
            ICY_ERROR(global_gui->create(image, colors));
            if (*this)
                global_gui->destroy(*this);
            index = image.index;
            return {};
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
            if (*this)
                global_gui->destroy(*this);
        }
        explicit operator bool() const noexcept
        {
            return index != 0;
        }
        error_type initialize(const string_view text) noexcept
        {
            gui_action action;
            ICY_ERROR(global_gui->create(action, text));
            if (*this)
                global_gui->destroy(*this);
            index = action.index;
            return {};
        }
        error_type enable(const bool value) noexcept
        {
            return global_gui->enable(*this, value);
        }
        error_type bind(const gui_widget menu) noexcept
        {
            return global_gui->bind(*this, menu);
        }
    };
    class xgui_node : public icy::gui_node
    {
    public:
        using gui_node::udata;
        explicit operator bool() const noexcept
        {
            return _ptr != nullptr;
        }
        error_type insert_rows(const size_t offset, const size_t count) noexcept
        {
            return global_gui->insert_rows(*this, offset, count);
        }
        error_type insert_cols(const size_t offset, const size_t count) noexcept
        {
            return global_gui->insert_cols(*this, offset, count);
        }
        error_type remove_rows(const size_t offset, const size_t count) noexcept
        {
            return global_gui->remove_rows(*this, offset, count);
        }
        error_type remove_cols(const size_t offset, const size_t count) noexcept
        {
            return global_gui->remove_cols(*this, offset, count);
        }
        error_type move_rows(const size_t offset_src, const size_t count, const size_t offset_dst) noexcept
        {
            return global_gui->move_rows(*this, offset_src, count, *this, offset_dst);
        }
        error_type move_cols(const size_t offset_src, const size_t count, const size_t offset_dst) noexcept
        {
            return global_gui->move_cols(*this, offset_src, count, *this, offset_dst);
        }
        xgui_node node(const size_t row, const size_t col) const noexcept
        {
            xgui_node node;
            auto find = global_gui->node(*this, row, col);
            static_cast<gui_node&>(node) = find;
            return node;
        }
        error_type text(const string_view text) noexcept
        {
            return global_gui->text(*this, text);
        }
        error_type udata(const gui_variant& var) noexcept
        {
            return global_gui->udata(*this, var);
        }
        error_type icon(const gui_image image) noexcept
        {
            return global_gui->icon(*this, image);
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
            if (*this)
                global_gui->destroy(*this);
        }
        explicit operator bool() const noexcept
        {
            return _ptr != nullptr;
        }
        error_type initialize() noexcept
        {
            gui_node model;
            ICY_ERROR(global_gui->create(model));
            if (*this)
                global_gui->destroy(*this);
            static_cast<gui_node&>(*this) = model;
            return {};
        }
        error_type vheader(const uint32_t index, const string_view text) noexcept
        {
            return global_gui->vheader(*this, index, text);
        }
        error_type hheader(const uint32_t index, const string_view text) noexcept
        {
            return global_gui->hheader(*this, index, text);
        }
        error_type clear() noexcept
        {
            return global_gui->clear(*this);
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
            return {};
        }
        xgui_widget widget;
        xgui_action action;
    };
}