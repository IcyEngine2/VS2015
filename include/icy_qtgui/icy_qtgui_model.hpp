#pragma once

#include "icy_qtgui_core.hpp"

namespace icy
{
    class gui_queue;
    enum
    {
        gui_model_max_row = (1 << 0x10),
        gui_model_max_col = (1 << 0x08),
        gui_model_max_idx = (1 << 0x14),
    };
    struct gui_node
    {
        gui_node() : idx(0), par(0), row(0), col(0)
        {

        }
        explicit operator bool() const noexcept
        {
            return !!idx;
        }
        uint64_t idx : 0x14;
        uint64_t par : 0x14;
        uint64_t row : 0x10;
        uint64_t col : 0x08;
    };
    static_assert(0x14 + 0x14 + 0x10 + 0x08 == 0x40 && sizeof(gui_node) == sizeof(uint64_t), "INVALID GUI NODE SIZE");

    enum class gui_role : uint32_t
    {
        view,
        edit,
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
        using vtbl_type = struct
        {
            init_func init = nullptr;
            sort_func sort = nullptr;
            bind_func bind = nullptr;
            update_func update = nullptr;
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