#pragma once

#include "icy_qtgui_core.hpp"

namespace icy
{
    class gui_queue;
   
    enum class gui_role : uint32_t
    {
        view,
        edit,
        check,
        user,
    };
    enum class gui_check_state : uint32_t
    {
        unchecked,
        partial,
        checked,
    };
    struct gui_model_view
    {
        void* ptr = nullptr;
    };
    class gui_model_base
    {
        friend gui_queue;
    public:
        using init_func = uint32_t(*)(const gui_model_view model, gui_node& node);
        using sort_func = uint32_t(*)(const gui_model_view model, const gui_node node);
        using bind_func = uint32_t(*)(const gui_model_view model, const gui_node node, const gui_widget& widget);
        using update_func = uint32_t(*)(const gui_model_view model, const gui_node node);
        using clear_func = uint32_t(*)(const gui_model_view model);
        using vtbl_type = struct
        {
            init_func init = nullptr;
            sort_func sort = nullptr;
            bind_func bind = nullptr;
            update_func update = nullptr;
            clear_func clear = nullptr;
        };
    public:
        virtual void assign(const vtbl_type& vtbl) noexcept = 0;
        virtual void add_ref() noexcept = 0;
        virtual void release() noexcept = 0;
        virtual int compare(const gui_node lhs, const gui_node rhs) const noexcept
        {
            return lhs.idx < rhs.idx;
        }
        virtual bool filter(const gui_node) const noexcept
        {
            return true;
        }
        virtual uint32_t data(const gui_node node, const gui_role role, gui_variant& value) const noexcept = 0;
    };
}