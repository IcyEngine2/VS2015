#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_map.hpp>
#include <icy_engine/core/icy_json.hpp>
#include <icy_engine/utility/icy_variant.hpp>

// {BCC6110B-E4A9-49EE-9131-F3176F4CA67C}
static const icy::guid ecsdb_root = { 0xbcc6110be4a949ee, 0x9131f3176f4ca67c };
// {8E097DF3-63F1-4E66-832F-F8FE2AC01993}
static const icy::guid ecsdb_property_type_binary = { 0x8e097df363f14e66, 0x832ff8fe2ac01993 };
// {402D4882-430D-4BF4-9BC9-849502CE9BE3}
static const icy::guid ecsdb_property_type_string = { 0x402d4882430d4bf4, 0x9bc9849502ce9be3 };


enum class ecsdb_flag : uint32_t
{
    none,
    deleted,
};
enum class ecsdb_action_type : uint32_t
{
    none,

    create_user,
    delete_user,
    rename_user,

    create_directory,
    delete_directory,
    rename_directory,
    move_directory,

    create_component,
    delete_component,
    rename_component,
    move_component,
    
    create_component_property,
    delete_component_property,
    rename_component_property,
    
    create_entity,
    remove_entity,
    rename_entity,
    move_entity,

    create_entity_property,
    delete_entity_property,
    
    create_entity_value,
    modify_entity_value,
    delete_entity_value,


};
enum class ecsdb_error_code : uint32_t
{
    success,
    invalid_project,
    unknown_user,
    unknown_action_type,
    index_already_exists,
    directory_not_found,
};
struct ecsdb_entry
{
    icy::guid index;
    ecsdb_flag flags = ecsdb_flag::none;
    uint32_t version = 0;
};
struct ecsdb_directory : ecsdb_entry
{
    icy::guid directory;
    icy::string name;
};
struct ecsdb_component_property : ecsdb_entry
{
    icy::guid type;
    icy::string name;
};
struct ecsdb_component : ecsdb_entry
{
    icy::guid directory;
    icy::string name;
    icy::array<ecsdb_component_property> data;
};
struct ecsdb_entity_value : ecsdb_entry
{
    icy::gui_variant value;
};
struct ecsdb_entity_property : ecsdb_entry
{
    icy::array<ecsdb_entity_value> data;
};
struct ecsdb_entity : ecsdb_entry
{
    icy::guid directory;
    icy::string name;
    icy::array<ecsdb_entity_property> data;
};

struct ecsdb_action
{
    ecsdb_action_type action = ecsdb_action_type::none;
    icy::guid index;
    icy::guid directory;
    icy::string name;
    icy::gui_variant value;
};
struct ecsdb_transaction
{
    icy::guid user;
    time_t time = 0;
    uint32_t index = 0;
    icy::array<ecsdb_action> actions;
};

icy::string_view to_string(const ecsdb_action_type input) noexcept;
icy::error_type to_json(const ecsdb_directory& input, icy::json& output) noexcept;
icy::error_type to_json(const ecsdb_entity& input, icy::json& output) noexcept;
icy::error_type to_json(const ecsdb_action& input, icy::json& output) noexcept;
icy::error_type to_json(const ecsdb_transaction& input, icy::json& output) noexcept;

namespace icy
{
    extern const error_source error_source_ecsdb;
}

struct ecsdb_system
{
    virtual ~ecsdb_system() noexcept = 0
    {

    }
    virtual icy::error_type copy(const icy::string_view path) noexcept = 0;
    virtual icy::error_type check(const ecsdb_transaction& txn) noexcept = 0;
    virtual icy::error_type exec(ecsdb_transaction& txn) noexcept = 0;
    virtual icy::error_type enum_directories(icy::array<ecsdb_directory>& list) noexcept = 0;
    virtual icy::error_type enum_components(icy::array<ecsdb_component>& list) noexcept = 0;
    virtual icy::error_type enum_entities(icy::array<icy::guid>& list) noexcept = 0;
    virtual icy::error_type find_entity(const icy::guid index, ecsdb_entity& entity) noexcept = 0;
};

extern icy::error_type create_ecsdb_system(icy::shared_ptr<ecsdb_system>& system, const icy::string_view path, const size_t capacity) noexcept;