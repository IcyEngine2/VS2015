#include <ecsdb/ecsdb_core.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/core/icy_smart_pointer.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
error_type make_ecsdb_error(const ecsdb_error_code code) noexcept
{
	return error_type(uint32_t(code), error_source_ecsdb);
}
error_type ecsdb_error_to_string(const unsigned code, const string_view, string& str) noexcept
{
	return make_stdlib_error(std::errc::invalid_argument);
}
class ecsdb_system_data : public ecsdb_system
{
public:
	error_type initialize(const string_view path, const size_t capacity) noexcept;
private:
	error_type copy(const string_view path) noexcept override;
	error_type check(const ecsdb_transaction& txn) noexcept override;
	error_type exec(ecsdb_transaction& txn) noexcept override;
	error_type exec(const ecsdb_transaction& txn, database_txn_write& write) noexcept;
	error_type enum_directories(array<ecsdb_directory>& list) noexcept override;
	error_type enum_components(array<ecsdb_component>& list) noexcept override;
	error_type enum_entities(array<guid>& list) noexcept override;
	error_type find_entity(const guid index, ecsdb_entity& entity) noexcept override;
private:
	database_system_write m_file;
	database_dbi m_dbi_user;
	database_dbi m_dbi_action;
	database_dbi m_dbi_binary;
	database_dbi m_dbi_directory;
	database_dbi m_dbi_component;
	database_dbi m_dbi_entity;
};
ICY_STATIC_NAMESPACE_END

error_type ecsdb_system_data::initialize(const string_view path, const size_t capacity) noexcept
{
	ICY_ERROR(m_file.initialize(path, capacity));
	database_txn_write txn;
	ICY_ERROR(txn.initialize(m_file));
	ICY_ERROR(m_dbi_user.initialize_create_any_key(txn, "user"_s));
	ICY_ERROR(m_dbi_action.initialize_create_int_key(txn, "action"_s));
	ICY_ERROR(m_dbi_binary.initialize_create_any_key(txn, "binary"_s));
	ICY_ERROR(m_dbi_directory.initialize_create_any_key(txn, "directory"_s));
	ICY_ERROR(m_dbi_component.initialize_create_any_key(txn, "component"_s));
	ICY_ERROR(m_dbi_entity.initialize_create_any_key(txn, "entity"_s));
	ICY_ERROR(txn.commit());
	return error_type();
}

error_type ecsdb_system_data::copy(const string_view path) noexcept
{
	return error_type();
}
error_type ecsdb_system_data::check(const ecsdb_transaction& txn) noexcept
{
	return error_type();

}
error_type ecsdb_system_data::exec(ecsdb_transaction& txn) noexcept
{
	return error_type();


}
error_type ecsdb_system_data::exec(const ecsdb_transaction& txn, database_txn_write& write) noexcept
{
	return error_type();

}
error_type ecsdb_system_data::enum_directories(array<ecsdb_directory>& list) noexcept
{

	return error_type();
}
error_type ecsdb_system_data::enum_components(array<ecsdb_component>& list) noexcept
{
	return error_type();

}
error_type ecsdb_system_data::enum_entities(array<guid>& list) noexcept
{
	return error_type();

}
error_type ecsdb_system_data::find_entity(const guid index, ecsdb_entity& entity) noexcept
{

	return error_type();
}

error_type create_ecsdb_system(shared_ptr<ecsdb_system>& system, const string_view path, const size_t capacity) noexcept
{
	shared_ptr<ecsdb_system_data> new_system;
	ICY_ERROR(make_shared(new_system));
	ICY_ERROR(new_system->initialize(path, capacity));
	system = std::move(new_system);
	return error_type();
}
string_view to_string(const ecsdb_action_type input) noexcept
{
	switch (input)
	{
	case ecsdb_action_type::create_directory:
		return "mkDir"_s;
	default:
		break;
	}
	return ""_s;
}
const error_source icy::error_source_ecsdb = register_error_source("ecsdb"_s, ecsdb_error_to_string);

/*
error_type to_json(const ecsdb_base& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert("flags"_s, uint32_t(input.flags)));
	ICY_ERROR(output.insert("name"_s, input.name));
	ICY_ERROR(output.insert("version"_s, input.version));
	return error_type();
}
error_type to_json(const ecsdb_folder& input, json& output) noexcept
{
	ICY_ERROR(to_json(static_cast<const ecsdb_base&>(input), output));
	string folder_string;
	ICY_ERROR(to_string(input.folder, folder_string));
	ICY_ERROR(output.insert("folder"_s, folder_string));
	return error_type();
}
error_type to_json(const ecsdb_value& input, json& output) noexcept
{
	output = json_type::object;
	return error_type();
}
error_type to_json(const ecsdb_attribute& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert("flags"_s, uint32_t(input.flags)));
	ICY_ERROR(output.insert("version"_s, input.version));

	string type_string;
	ICY_ERROR(to_string(input.type, type_string));
	ICY_ERROR(output.insert("type"_s, type_string));
	ICY_ERROR(output.insert("name"_s, input.name));

	if (input.link)
	{
		string link_string;
		ICY_ERROR(to_string(input.link, link_string));
		ICY_ERROR(output.insert("link"_s, link_string));
	}

	return error_type();
}
error_type to_json(const ecsdb_entity& input, json& output) noexcept
{
	ICY_ERROR(to_json(static_cast<const ecsdb_base&>(input), output));
	string folder_string;
	ICY_ERROR(to_string(input.folder, folder_string));
	ICY_ERROR(output.insert("folder"_s, folder_string));
	json data = json_type::object;
	for (auto&& pair : input.data)
	{
		string key;
		ICY_ERROR(to_string(pair.key, key));
		json val;
		ICY_ERROR(to_json(pair.value, val));
		ICY_ERROR(data.insert(key, std::move(val)));
	}
	ICY_ERROR(output.insert("data"_s, std::move(data)));

	json vals = json_type::array;
	for (auto&& val : input.vals)
	{
		json jval;
		ICY_ERROR(to_json(val, jval));
		ICY_ERROR(data.push_back(std::move(jval)));
	}
	ICY_ERROR(output.insert("vals"_s, std::move(vals)));

	return error_type();
}
error_type to_json(const ecsdb_action& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert("action"_s, to_string(input.action)));
	string index_string;
	string folder_string;
	ICY_ERROR(to_string(input.index, index_string));
	ICY_ERROR(to_string(input.folder, folder_string));
	ICY_ERROR(output.insert("index"_s, index_string));
	if (input.folder != guid()) { ICY_ERROR(output.insert("folder"_s, folder_string)); }
	if (!input.name.empty()) { ICY_ERROR(output.insert("name"_s, input.name)); }

	return error_type();
}
error_type to_json(const ecsdb_transaction& input, json& output) noexcept
{
	output = json_type::object;
	string user_string;
	ICY_ERROR(to_string(input.user, user_string));
	ICY_ERROR(output.insert("user"_s, user_string));
	ICY_ERROR(output.insert("time"_s, input.time));
	json actions = json_type::array;
	for (auto&& action : input.actions)
	{
		json jaction;
		ICY_ERROR(to_json(action, jaction));
		ICY_ERROR(actions.push_back(std::move(jaction)));
	}
	ICY_ERROR(output.insert("data"_s, std::move(actions)));
	return error_type();
}*/

