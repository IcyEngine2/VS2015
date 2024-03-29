#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_memory.hpp>
#include <icy_engine/core/icy_input.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/network/icy_http.hpp>
#include <icy_gui/icy_gui.hpp>


enum class mbox_type : uint32_t
{
    none,
    folder,
    device,
    application,
    group,          //  characters
    account,
    server,
    character,
    tag,
    action,
    script,
    macro,
    timer,
    event_key_press,
    event_key_release,
    _total,
};
enum class mbox_operation_type : uint32_t
{
    none,
    add_tag,
    del_tag,
    launch_timer,
    cancel_timer,
    send_input,
    run_macro,
    run_script,
    run_action,
    add_event,
    del_event,
    _total,
};
struct mbox_operation
{
    mbox_operation(const mbox_operation_type type = mbox_operation_type::none) noexcept : type(type)
    {

    }
    mbox_operation_type type;
    uint32_t link = 0u;
    icy::string text;   //  Input or Script.Function
};
struct mbox_base
{
public:
    using node_map = icy::map<icy::weak_ptr<icy::gui_data_write_model>, icy::array<icy::gui_node>>;
public:
    mbox_base(const mbox_type type = mbox_type::none) noexcept : type(type)
    {

    }
    mbox_base(mbox_base&& rhs) noexcept = default;
    ICY_DEFAULT_MOVE_ASSIGN(mbox_base);
    virtual ~mbox_base() noexcept
    {

    }
public:
    mbox_type type;
    uint32_t index = 0;
    uint32_t parent = 0;
    icy::string name;
    icy::map<uint32_t, mbox_base*> children;
    icy::string text;
    node_map tree;
    icy::array<mbox_operation> operations;
    /*icy::string device_ip;
    icy::array<mbox_device_app_config> device_app_config;
    icy::array<mbox_character_config> character_group;
    icy::string text;*/
};

extern const icy::error_source error_source_mbox;
extern const icy::error_type mbox_error_corrupted_data;
extern const icy::error_type mbox_error_invalid_index;
extern const icy::error_type mbox_error_invalid_parent;
extern const icy::error_type mbox_error_invalid_type;
extern const icy::error_type mbox_error_invalid_value;

class mbox_server_database
{
public:
    icy::error_type load(const icy::string_view path) noexcept;
    icy::error_type save(const icy::string_view path) noexcept;
    icy::error_type tree_view(const uint32_t index, icy::gui_data_write_model& model, const icy::gui_node node) noexcept;
    icy::error_type info_view(const uint32_t index, icy::gui_data_write_model& model, const icy::gui_node node) noexcept;
    icy::error_type create_base(const uint32_t parent, const mbox_type type, const icy::string_view name, uint32_t& index) noexcept;
private:
    icy::map<uint32_t, icy::unique_ptr<mbox_base>> m_data;
    struct
    {
        icy::weak_ptr<icy::gui_data_write_model> model;
        icy::gui_node node;
        uint32_t index;
    } m_info;
};
icy::error_type mbox_try_create(const mbox_base& pval, const mbox_type type) noexcept;