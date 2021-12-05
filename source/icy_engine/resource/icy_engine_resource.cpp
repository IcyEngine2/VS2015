#include <icy_engine/resource/icy_engine_resource.hpp>
#include <icy_engine/utility/icy_database.hpp>
#include <icy_engine/core/icy_file.hpp>
#include <icy_engine/core/icy_json.hpp>

#if 1
#include <icy_engine/graphics/icy_render_core.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#pragma comment(lib, "assimpd")
#pragma comment(lib, "zlibd")


#define ASSIMP_CHECK(X) switch (const auto code = (X)) { \
case aiReturn_SUCCESS: break; \
case aiReturn_OUTOFMEMORY: error = make_stdlib_error(std::errc::not_enough_memory); return error_type();}

#endif

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
struct internal_message
{
    resource_header header;
    string path;
    array<uint8_t> bytes;
};
class assimp_scene
{
public:
    struct animation_tuple : resource_header, render_animation
    {
    };
    struct texture_tuple : resource_header, render_texture
    {
        uint32_t ref = 0;
    };
    struct material_tuple : resource_header, render_material
    {
        uint32_t ref = 0;
    };
    struct mesh_tuple : resource_header, render_mesh
    {
        uint32_t ref = 0;
    };
    struct node_tuple : resource_header, render_node
    {
    };
public:
    static error_type init_header(resource_header& header, const resource_type type, const aiString& name) noexcept
    {
        header.index = guid::create();
        if (!header.index)
            return last_system_error();
        header.type = type;

        string_view str;
        ICY_ERROR(to_string(const_array_view<char>(name.data, name.length), str));
        ICY_ERROR(copy(str, header.name));

        return error_type();
    }
    error_type import_material(const aiMaterial& old_material) noexcept;
    error_type import_mesh(const aiMesh& old_mesh) noexcept;
    error_type import_node(const aiNode& old_node, const guid& parent, guid& new_index) noexcept;
    error_type import_animation(const aiAnimation& old_animation) noexcept;
    error_type import_light(const aiLight& old_light) noexcept;
    error_type import_camera(const aiCamera& old_camera) noexcept;
public:
    const aiScene* source = nullptr;
    guid root;
    error_type error;
    array<animation_tuple> animations;
    map<string, texture_tuple> textures;
    array<material_tuple> materials;
    array<mesh_tuple> meshes;
    map<guid, node_tuple> nodes;
};
static render_vec4 make_quat(const aiVector3D u, const aiVector3D v) noexcept
{
    auto half = (u + v);
    half *= 0.5;
    half.NormalizeSafe();
    const auto cross = u ^ half;
    render_vec4 angle;
    angle.x = cross.x;
    angle.y = cross.y;
    angle.z = cross.z;
    angle.w = half.x * u.x + half.y * u.y + half.z * u.z;
    return angle;
}

static const auto key_time_create = "timeC"_s;
static const auto key_time_update = "timeU"_s;
static const auto key_hash = "hash"_s;
static const auto key_type = "type"_s;
static const auto key_name = "name"_s;
static const auto key_data = "data"_s;
mutex instance_lock;
resource_system* instance_ptr = nullptr;
class resource_system_data : public resource_system
{
public:
    struct txn_read
    {
        error_type initialize(const resource_system_data& system) noexcept
        {
            ICY_ERROR(txn.initialize(system.m_dbase));
            ICY_ERROR(cur_header.initialize(txn, system.m_dbi_header));
            ICY_ERROR(cur_binary.initialize(txn, system.m_dbi_binary));
            return error_type();
        }
        error_type load(guid index, resource_data& data) noexcept
        {
            string_view str;
            ICY_ERROR(cur_header.get_str_by_type(index, str, database_oper_read::none));
            icy::json json;
            ICY_ERROR(to_value(str, json));
            if (json.type() == json_type::object) 
                return data.from_json(json);
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        }
        database_txn_read txn;
        database_cursor_read cur_header;
        database_cursor_read cur_binary;
    };
    struct txn_write
    {
        error_type initialize(const resource_system_data& system) noexcept
        {
            ICY_ERROR(txn.initialize(system.m_dbase));
            ICY_ERROR(cur_header.initialize(txn, system.m_dbi_header));
            ICY_ERROR(cur_binary.initialize(txn, system.m_dbi_binary));
            return error_type();
        }
        error_type load(guid index, resource_data& data) noexcept
        {
            string_view str;
            ICY_ERROR(cur_header.get_str_by_type(index, str, database_oper_read::none));
            icy::json json;
            ICY_ERROR(to_value(str, json));
            if (json.type() == json_type::object)
                return data.from_json(json);
            return make_stdlib_error(std::errc::illegal_byte_sequence);
        }
        error_type store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept;
        database_txn_write txn;
        database_cursor_write cur_header;
        database_cursor_write cur_binary;
    };
public:
    ~resource_system_data() noexcept override
    {
        ICY_LOCK_GUARD(instance_lock);
        if (instance_ptr == this)
            instance_ptr = nullptr;

        if (m_thread)
            m_thread->wait();
        filter(0);
    }
    error_type initialize(const string_view path, const size_t capacity) noexcept;
    error_type exec() noexcept override;
    error_type signal(const event_data*) noexcept override
    {
        return m_sync.wake();
    }
private:
    const icy::thread& thread() const noexcept
    {
        return *m_thread;
    }
    error_type load(const resource_header& header) noexcept override;
    error_type store(const resource_header& header, const string_view path) noexcept override;
    error_type store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept override;
    error_type store(const guid& index, const const_matrix_view<color> colors) noexcept override;
    error_type load(const resource_header& header, resource_event& new_event) const noexcept;
    error_type list(map<guid, resource_data>& output) const noexcept override;
    error_type list(const resource_locale locale, array<resource_event>& output) const noexcept override;
private:
    error_type parse_image(const const_array_view<uint8_t> input, matrix<color>& output) const noexcept;
    error_type import_assimp(const const_array_view<uint8_t> input, assimp_scene& output) noexcept;
    error_type store_assimp(const assimp_scene& output, txn_write& txn) noexcept;
private:
    sync_handle m_sync;
    mutex m_lock;
    shared_ptr<event_thread> m_thread;
    database_system_write m_dbase;
    database_dbi m_dbi_header;
    database_dbi m_dbi_binary;
    map<guid, matrix<color>> m_images;
};
ICY_STATIC_NAMESPACE_END

resource_event::~resource_event() noexcept
{
    if (!bytes)
        return;
    switch (header.type)
    {
    case resource_type::user:
        allocator_type::destroy(&user());
        break;
    case resource_type::text:
        allocator_type::destroy(&text());
        break;
    case resource_type::image:
        allocator_type::destroy(&image());
        break;
    case resource_type::animation:
        allocator_type::destroy(&animation());
        break;
    case resource_type::texture:
        allocator_type::destroy(&texture());
        break;
    case resource_type::material:
        allocator_type::destroy(&material());
        break;
    case resource_type::mesh:
        allocator_type::destroy(&mesh());
        break;
    case resource_type::node:
        allocator_type::destroy(&node());
        break;
    }
    allocator_type::deallocate(bytes);
}


error_type resource_binary::to_json(icy::json& output) const noexcept
{
    output = json_type::object;
    ICY_ERROR(output.insert(key_time_create, time_create));
    ICY_ERROR(output.insert(key_time_update, time_update));
    ICY_ERROR(output.insert(key_hash, hash));
    return error_type();
}
error_type resource_binary::from_json(const icy::json& input) noexcept
{
    if (input.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(input.get(key_time_create, time_create));
    ICY_ERROR(input.get(key_time_update, time_update));
    ICY_ERROR(input.get(key_hash, hash));
    return error_type();
}
error_type resource_data::to_json(icy::json& output) const noexcept
{
    output = json_type::object;

    ICY_ERROR(output.insert(key_type, to_string(type)));
    ICY_ERROR(output.insert(key_name, name));
    icy::json map = json_type::object;
    for (auto&& pair : binary)
    {
        icy::json binary;
        ICY_ERROR(pair.value.to_json(binary));
        ICY_ERROR(map.insert(to_string(resource_locale(pair.key)), std::move(binary)));
    }
    ICY_ERROR(output.insert(key_data, std::move(map)));
    return error_type();
}
error_type resource_data::from_json(const icy::json& input) noexcept
{
    if (input.type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    const auto type_str = input.get(key_type);
    for (auto k = 1u; k < uint32_t(resource_type::_total); ++k)
    {
        if (type_str == to_string(resource_type(k)))
        {
            type = resource_type(k);
            break;
        }
    }
    ICY_ERROR(copy(input.get(key_name), name));
    const auto data = input.find(key_data);
    if (!data || data->type() != json_type::object)
        return make_stdlib_error(std::errc::invalid_argument);

    for (auto k = 0u; k < data->size(); ++k)
    {
        const auto& key = data->keys()[k];
        const auto& val = data->vals()[k];

        resource_binary new_val;
        ICY_ERROR(new_val.from_json(val));

        for (auto n = 0u; n < uint32_t(resource_locale::_total); ++n)
        {
            if (key == to_string(resource_locale(n)))
            {
                ICY_ERROR(binary.insert(n, std::move(new_val)));
                break;
            }
        }
    }
    return error_type();
}

error_type resource_system_data::txn_write::store(const resource_header& header, const const_array_view<uint8_t> write) noexcept
{
    resource_data resource;
    if (const auto error = load(header.index, resource))
    {
        if (error == database_error_not_found)
            ;
        else
            return error;
    }
    if (!header.name.empty())
    {
        ICY_ERROR(copy(header.name, resource.name));
    }
    if (resource.type != resource_type::none)
    {
        if (header.type != resource.type)
            return make_stdlib_error(std::errc::invalid_argument);
    }
    else
    {
        resource.type = header.type;
    }
    
    array_view<uint8_t> output;

    auto it = resource.binary.find(uint32_t(header.locale));
    if (it == resource.binary.end())
    {
        ICY_ERROR(resource.binary.insert(uint32_t(header.locale), resource_binary(), &it));
    }
    it->value.hash = hash64(write.data(), write.size());
    const auto now = clock_type::now();
    if (it->value.time_create == clock_type::time_point())
        it->value.time_create = now;
    it->value.time_update = now;
    ICY_ERROR(cur_binary.put_var_by_type(it->value.hash, write.size(), database_oper_write::none, output));
    memcpy(output.data(), write.data(), write.size());

    json json;
    ICY_ERROR(resource.to_json(json));
    string str;
    ICY_ERROR(to_string(json, str));
    ICY_ERROR(cur_header.put_str_by_type(header.index, database_oper_write::none, str));
    
    return error_type();
}
error_type resource_system_data::initialize(const string_view path, const size_t capacity) noexcept
{
    ICY_ERROR(m_sync.initialize());
    ICY_ERROR(m_lock.initialize());
    if (path.empty())
    {

    }
    else
    {
        ICY_ERROR(m_dbase.initialize(path, capacity));
        database_txn_write txn;
        ICY_ERROR(txn.initialize(m_dbase));
        ICY_ERROR(m_dbi_header.initialize_create_any_key(txn, "header"_s));
        ICY_ERROR(m_dbi_binary.initialize_create_any_key(txn, "binary"_s));
        ICY_ERROR(txn.commit());
    }
    ICY_ERROR(make_shared(m_thread));
    m_thread->system = this;
    filter(event_type::system_internal);
    return error_type();
}
error_type resource_system_data::exec() noexcept
{
    while (*this)
    {
        while (auto event = pop())
        {
            if (event->type != event_type::system_internal)
                continue;

            const auto& event_data = event->data<internal_message>();
            
            resource_event new_event;
            new_event.header.index = event_data.header.index;
            new_event.header.locale = event_data.header.locale;
            new_event.header.type = event_data.header.type;

            array<uint8_t> bytes;
            const_array_view<uint8_t> write = event_data.bytes;

            if (!event_data.path.empty())
            {
                file input;
                input.open(event_data.path, file_access::read, file_open::open_existing, file_share::read, file_flag::none);
                auto size = input.info().size;
                if (!size) new_event.error = make_stdlib_error(std::errc::no_such_file_or_directory);
                if (!new_event.error) new_event.error = bytes.resize(size);
                if (!new_event.error) new_event.error = input.read(bytes.data(), size);
                if (!new_event.error) new_event.error = bytes.resize(size);
                if (!new_event.error) write = bytes;
            }

            if (!write.empty())
            {
                txn_write txn;
                if (!new_event.error) new_event.error = txn.initialize(*this);

                switch (new_event.header.type)
                {
                case resource_type::node:
                {
                    assimp_scene scene;
                    scene.root = event_data.header.index;
                    if (!new_event.error)
                    {
                        ICY_ERROR(import_assimp(bytes, scene));
                        new_event.error = scene.error;
                    }
                    if (!new_event.error)
                        new_event.error = store_assimp(scene, txn);
                    break;
                }
                case resource_type::image:
                case resource_type::text:
                case resource_type::user:
                {
                    if (!new_event.error)
                        new_event.error = txn.store(event_data.header, write);
                    break;
                }
                default:
                    new_event.error = make_stdlib_error(std::errc::invalid_argument);
                    break;
                }

                if (!new_event.error)
                {
                    txn.cur_binary = {};
                    txn.cur_header = {};
                    new_event.error = txn.txn.commit();
                }
                ICY_ERROR(event::post(this, event_type::resource_store, std::move(new_event)));
            }
            else
            {
                ICY_ERROR(load(event_data.header, new_event));
                ICY_ERROR(event::post(this, event_type::resource_load, std::move(new_event)));
            }          
        }
        ICY_ERROR(m_sync.wait());
    }
    return error_type();
}
error_type resource_system_data::load(const resource_header& header) noexcept
{
    if (!header)
        return make_stdlib_error(std::errc::invalid_argument);

    if (header.type == resource_type::image)
    {
        ICY_LOCK_GUARD(m_lock);
        const auto image = m_images.try_find(header.index);
        if (image)
        {
            resource_event new_event;
            new_event.header.index = header.index;
            new_event.header.type = header.type;

            auto new_image = make_unique<matrix<color>>();
            if (!new_image)
                return make_stdlib_error(std::errc::not_enough_memory);
            
            ICY_ERROR(copy(const_matrix_view<color>(*image), *new_image));
            new_event.bytes = new_image.release();
            ICY_ERROR(event::post(this, event_type::resource_load, std::move(new_event)));
            return error_type();
        }

    }
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type resource_system_data::load(const resource_header& header, resource_event& new_event) const noexcept
{
    txn_read txn;
    if (!new_event.error)
        new_event.error = txn.initialize(*this);

    resource_data resource;
    if (!new_event.error)
        new_event.error = txn.load(new_event.header.index, resource);

    const_array_view<uint8_t> input;
    if (!new_event.error)
    {
        if (const auto ptr = resource.binary.try_find(uint32_t(header.locale)))
        {
            if (const auto error = txn.cur_binary.get_var_by_type(ptr->hash, input, database_oper_read::none))
            {
                if (error != database_error_not_found)
                    new_event.error = error;
            }
            else if (!input.empty())
            {
                new_event.hash = ptr->hash;
                new_event.time_create = ptr->time_create;
                new_event.time_update = ptr->time_update;
            }
        }
    }
    if (!new_event.error)
    {
        new_event.error = make_stdlib_error(std::errc::invalid_argument);
        switch (header.type)
        {
        case resource_type::text:
        {
            if (resource.type == resource_type::text)
            {
                string_view input_str;
                new_event.error = to_string(input, input_str);
                if (!new_event.error)
                {
                    string tmp_str;
                    ICY_ERROR(to_string(input_str, tmp_str));
                    auto new_text = make_unique<string>(std::move(tmp_str));
                    if (!new_text)
                        ICY_ERROR(make_stdlib_error(std::errc::not_enough_memory));
                    new_event.bytes = new_text.release();
                }
            }
            break;
        }
        case resource_type::image:
        {
            if (resource.type == resource_type::image)
            {
                matrix<color> image;
                unique_ptr<matrix<color>> new_image;
                new_event.error = parse_image(input, image);
                if (!new_event.error)
                    new_event.error = make_unique(std::move(image), new_image);
                if (!new_event.error)
                    new_event.bytes = new_image.release();
            }
            break;
        }
        case resource_type::user:
        {
            auto new_user = make_unique<array<uint8_t>>();
            if (!new_user)
                ICY_ERROR(make_stdlib_error(std::errc::not_enough_memory));
            ICY_ERROR(new_user->assign(input));
            new_event.bytes = new_user.release();
            break;
        }
        default:
        {
            string_view input_str;
            new_event.error = to_string(input, input_str);

            json json;
            if (!new_event.error)
                new_event.error = to_value(input_str, json);

            const auto func = [&new_event, &json](auto&& new_value)
            {
                if (!new_value)
                    ICY_ERROR(make_stdlib_error(std::errc::not_enough_memory));
                new_event.error = to_value(json, *new_value);
                if (!new_event.error)
                    new_event.bytes = new_value.release();
                return error_type();
            };
            if (header.type == resource_type::animation && json.get(key_type) == to_string(resource_type::animation))
            {
                ICY_ERROR(func(make_unique<render_animation>()));
            }
            else if (header.type == resource_type::texture && json.get(key_type) == to_string(resource_type::texture))
            {
                ICY_ERROR(func(make_unique<render_texture>()));
            }
            else if (header.type == resource_type::material && json.get(key_type) == to_string(resource_type::material))
            {
                ICY_ERROR(func(make_unique<render_material>()));
            }
            else if (header.type == resource_type::mesh && json.get(key_type) == to_string(resource_type::mesh))
            {
                ICY_ERROR(func(make_unique<render_mesh>()));
            }
            else if (header.type == resource_type::node && json.get(key_type) == to_string(resource_type::node))
            {
                ICY_ERROR(func(make_unique<render_node>()));
            }
            break;
        }
        }
    }
    if (!new_event.bytes && !new_event.error)
        new_event.error = make_stdlib_error(std::errc::invalid_argument);

    return error_type();
}
error_type resource_system_data::store(const resource_header& header, const string_view path) noexcept
{
    if (!header || path.empty())
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(copy(header.name, msg.header.name));
    ICY_ERROR(copy(path, msg.path));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();

}
error_type resource_system_data::store(const resource_header& header, const const_array_view<uint8_t> bytes) noexcept
{
    if (!header || bytes.empty())
        return make_stdlib_error(std::errc::invalid_argument);
    internal_message msg;
    msg.header.index = header.index;
    msg.header.locale = header.locale;
    msg.header.type = header.type;
    ICY_ERROR(copy(header.name, msg.header.name));
    ICY_ERROR(msg.bytes.assign(bytes));
    ICY_ERROR(event_system::post(nullptr, event_type::system_internal, std::move(msg)));
    return error_type();
}
error_type resource_system_data::store(const guid& index, const const_matrix_view<color> colors) noexcept
{
    if (!index || colors.empty())
        return make_stdlib_error(std::errc::invalid_argument);

    matrix<color> tmp;
    ICY_ERROR(copy(colors, tmp));
    ICY_LOCK_GUARD(m_lock);
    ICY_ERROR(m_images.insert(index, std::move(tmp)));
    return error_type();
}
error_type resource_system_data::list(map<guid, resource_data>& output) const noexcept
{
    database_txn_read txn;
    database_cursor_read cur_header;
    ICY_ERROR(txn.initialize(m_dbase));
    ICY_ERROR(cur_header.initialize(txn, m_dbi_header));

    guid key;
    string_view str;
    auto error = cur_header.get_str_by_type(key, str, database_oper_read::first);
    while (!error)
    {
        icy::json json;
        ICY_ERROR(to_value(str, json));
        resource_data new_resource;
        ICY_ERROR(new_resource.from_json(json));
        ICY_ERROR(output.insert(key, std::move(new_resource)));
        error = cur_header.get_str_by_type(key, str, database_oper_read::next);
    }
    if (error != database_error_not_found)
        return error;
    return error_type();
}
error_type resource_system_data::list(const resource_locale locale, array<resource_event>& output) const noexcept
{
    map<guid, resource_data> list_resource;
    ICY_ERROR(list(list_resource));

    for (auto&& pair : list_resource)
    {
        resource_event new_event;
        auto& header = new_event.header;
        new_event.header.index = pair.key;
        new_event.header.locale = locale;
        new_event.header.type = pair.value.type;
        ICY_ERROR(load(header, new_event));
        ICY_ERROR(output.push_back(std::move(new_event)));
    }
    return error_type();
}
error_type resource_system_data::parse_image(const const_array_view<uint8_t> input, matrix<color>& output) const noexcept
{
    return error_type();
}
error_type resource_system_data::import_assimp(const const_array_view<uint8_t> input, assimp_scene& scene) noexcept
{
    try
    {
        Assimp::Importer importer;
        const auto old_scene = scene.source = importer.ReadFileFromMemory(input.data(), input.size(), aiProcessPreset_TargetRealtime_MaxQuality);
        ICY_SCOPE_EXIT{ scene.source = nullptr; };

        if (!old_scene || (old_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
        {
            scene.error = make_stdlib_error(std::errc::invalid_argument);
            return error_type();
        }
        if (old_scene->HasMaterials())
        {
            for (auto k = 0u; k < old_scene->mNumMaterials && !scene.error; ++k)
                ICY_ERROR(scene.import_material(*old_scene->mMaterials[k]));
        }
        if (old_scene->HasMeshes())
        {
            for (auto k = 0u; k < old_scene->mNumMeshes && !scene.error; ++k)
                ICY_ERROR(scene.import_mesh(*old_scene->mMeshes[k]));
        }
        if (!scene.error)
            ICY_ERROR(scene.import_node(*old_scene->mRootNode, guid(), scene.root));
        
        if (old_scene->HasCameras())
        {
            for (auto k = 0u; k < old_scene->mNumCameras && !scene.error; ++k)
                ICY_ERROR(scene.import_camera(*old_scene->mCameras[k]));
        }
        if (old_scene->HasLights())
        {
            for (auto k = 0u; k < old_scene->mNumLights && !scene.error; ++k)
                ICY_ERROR(scene.import_light(*old_scene->mLights[k]));
        }
        if (old_scene->HasAnimations())
        {
            for (auto k = 0u; k < old_scene->mNumAnimations && !scene.error; ++k)
                ICY_ERROR(scene.import_animation(*old_scene->mAnimations[k]));
        }
    }
    catch (...)
    {
        scene.error = make_stdlib_error(std::errc::not_enough_memory);
    }
    return error_type();
}
error_type resource_system_data::store_assimp(const assimp_scene& scene, txn_write& txn) noexcept
{
    const auto func = [&txn](const auto& value)
    {
        icy::json json;
        ICY_ERROR(to_json(value, json));
        ICY_ERROR(json.insert(key_type, to_string(static_cast<const resource_header&>(value).type)));
        string str;
        ICY_ERROR(to_string(json, str));
        ICY_ERROR(txn.store(value, str.ubytes()));
        return error_type();
    };
    for (auto&& texture : scene.textures)
    {
        if (texture.value.ref == 0)
            continue;
        ICY_ERROR(func(texture.value));
    }
    for (auto&& material : scene.materials)
    {
        if (material.ref == 0)
            continue;
        ICY_ERROR(func(material));
    }
    for (auto&& mesh : scene.meshes)
    {
        if (mesh.ref == 0)
            continue;
        ICY_ERROR(func(mesh));
    }
    for (auto&& node : scene.nodes)
    {
        ICY_ERROR(func(node.value));
    }
    for (auto&& animation : scene.animations)
    {
        ICY_ERROR(func(animation));
    }
    return error_type();
}

error_type icy::create_resource_system(shared_ptr<resource_system>& system, const string_view path, const size_t capacity) noexcept
{
    shared_ptr<resource_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(path, capacity));
    system = std::move(new_ptr);

    ICY_LOCK_GUARD(instance_lock);
    if (instance_ptr == nullptr)
        instance_ptr = system.get();
    return error_type();
}

shared_ptr<resource_system> resource_system::global() noexcept
{
    ICY_LOCK_GUARD(instance_lock);
    return make_shared_from_this(instance_ptr);
}

static detail::global_init_entry g_init([] { return instance_lock.initialize(); });

error_type assimp_scene::import_material(const aiMaterial& old_material) noexcept
{
    aiString name;
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_NAME, name));

    material_tuple new_material;
    if (error = init_header(new_material, resource_type::material, name))
        return error_type();

    for (auto n = aiTextureType_DIFFUSE; n < aiTextureType::aiTextureType_UNKNOWN; n = aiTextureType(uint32_t(n) + 1))
    {
        for (auto index = 0u; index < old_material.GetTextureCount(n); ++index)
        {
            aiString path;
            auto mapping = _aiTextureMapping_Force32Bit;
            auto uv = 0u;
            auto blend = ai_real(0);
            auto oper = _aiTextureOp_Force32Bit;
            aiTextureMapMode mode[3] = {};
            ASSIMP_CHECK(old_material.GetTexture(n, index, &path, &mapping, &uv, &blend, &oper, mode));

            string_view str;
            to_string(const_array_view<char>(path.data, path.length), str);

            auto it = textures.find(str);
            if (it == textures.end())
            {
                texture_tuple new_texture;
                if (error = init_header(new_texture, resource_type::texture, path))
                    return error_type();

                new_texture.blend = blend;
                for (auto m = 0u; m < 3u; ++m)
                {
                    switch (mode[m])
                    {
                    case aiTextureMapMode_Wrap: new_texture.map_mode[m] = render_texture_map_mode::wrap; break;
                    case aiTextureMapMode_Clamp: new_texture.map_mode[m] = render_texture_map_mode::clamp; break;
                    case aiTextureMapMode_Decal: new_texture.map_mode[m] = render_texture_map_mode::decal; break;
                    case aiTextureMapMode_Mirror: new_texture.map_mode[m] = render_texture_map_mode::mirror; break;
                    }
                }
                switch (mapping)
                {
                case aiTextureMapping_UV: new_texture.map_type = render_texture_map_type::uv; break;
                case aiTextureMapping_SPHERE: new_texture.map_type = render_texture_map_type::sphere; break;
                case aiTextureMapping_CYLINDER: new_texture.map_type = render_texture_map_type::cylinder; break;
                case aiTextureMapping_BOX: new_texture.map_type = render_texture_map_type::box; break;
                case aiTextureMapping_PLANE: new_texture.map_type = render_texture_map_type::plane; break;
                }

                auto type = render_texture_type::none;
                switch (n)
                {
                case aiTextureType_DIFFUSE: type = render_texture_type::diffuse; break;
                case aiTextureType_SPECULAR: type = render_texture_type::specular; break;
                case aiTextureType_AMBIENT: type = render_texture_type::ambient; break;
                case aiTextureType_EMISSIVE: type = render_texture_type::diffuse; break;
                case aiTextureType_HEIGHT: type = render_texture_type::height; break;
                case aiTextureType_NORMALS: type = render_texture_type::normals; break;
                case aiTextureType_SHININESS: type = render_texture_type::shininess; break;
                case aiTextureType_OPACITY: type = render_texture_type::opacity; break;
                case aiTextureType_DISPLACEMENT: type = render_texture_type::displacement; break;
                case aiTextureType_LIGHTMAP: type = render_texture_type::lightmap; break;
                case aiTextureType_REFLECTION: type = render_texture_type::reflection; break;
                case aiTextureType_BASE_COLOR: type = render_texture_type::pbr_base_color; break;
                case aiTextureType_NORMAL_CAMERA: type = render_texture_type::pbr_normal_camera; break;
                case aiTextureType_EMISSION_COLOR: type = render_texture_type::pbr_emission_color; break;
                case aiTextureType_METALNESS: type = render_texture_type::pbr_metalness; break;
                case aiTextureType_DIFFUSE_ROUGHNESS: type = render_texture_type::pbr_diffuse_roughness; break;
                case aiTextureType_AMBIENT_OCCLUSION: type = render_texture_type::pbr_ambient_occlusion; break;
                }
                static_cast<render_texture&>(new_texture).type = type;

                string path_str;
                ICY_ERROR(to_string(str, path_str));
                ICY_ERROR(textures.insert(std::move(path_str), std::move(new_texture), &it));
            }
            render_material::tuple new_texture;
            new_texture.index = it->value.index;
            switch (oper)
            {
            case aiTextureOp_Multiply: new_texture.oper = render_texture_oper::multiply; break;
            case aiTextureOp_Add: new_texture.oper = render_texture_oper::add; break;
            case aiTextureOp_Subtract: new_texture.oper = render_texture_oper::substract; break;
            case aiTextureOp_Divide: new_texture.oper = render_texture_oper::divide; break;
            case aiTextureOp_SmoothAdd: new_texture.oper = render_texture_oper::smooth_add; break;
            case aiTextureOp_SignedAdd: new_texture.oper = render_texture_oper::signed_add; break;
            }
            new_texture.type = static_cast<render_texture&>(it->value).type;
            ICY_ERROR(new_material.textures.push_back(std::move(new_texture)));
        }
        ICY_ERROR(materials.push_back(std::move(new_material)));
    }


    ASSIMP_CHECK(old_material.Get(AI_MATKEY_TWOSIDED, new_material.two_sided));

    auto mode = aiShadingMode_Flat;
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_SHADING_MODEL, mode));
    switch (mode)
    {
    case aiShadingMode_Flat:
        new_material.shading = render_shading_model::flat;
        break;
    case aiShadingMode_Gouraud:
        new_material.shading = render_shading_model::gouraud;
        break;
    case aiShadingMode_Phong:
        new_material.shading = render_shading_model::phong;
        break;
    case aiShadingMode_Blinn:
        new_material.shading = render_shading_model::blinn;
        break;
    case aiShadingMode_Toon:
        new_material.shading = render_shading_model::comic;
        break;
    case aiShadingMode_OrenNayar:
        new_material.shading = render_shading_model::oren_nayar;
        break;
    case aiShadingMode_Minnaert:
        new_material.shading = render_shading_model::minnaert;
        break;
    case aiShadingMode_CookTorrance:
        new_material.shading = render_shading_model::cook_torrance;
        break;
    case aiShadingMode_Fresnel:
        new_material.shading = render_shading_model::fresnel;
        break;
    }


    auto wireframe = false;
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_ENABLE_WIREFRAME, wireframe));
    if (wireframe)
        new_material.shading = render_shading_model::wire;

    ASSIMP_CHECK(old_material.Get(AI_MATKEY_OPACITY, new_material.opacity));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_TRANSPARENCYFACTOR, new_material.transparency));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_BUMPSCALING, new_material.bump_scaling));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_SHININESS, new_material.shininess));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_SHININESS_STRENGTH, new_material.shininess_percent));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_REFLECTIVITY, new_material.reflectivity));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_REFRACTI, new_material.refract));

    aiColor3D color_diffuse;
    aiColor3D color_ambient;
    aiColor3D color_specular;
    aiColor3D color_emissive;
    aiColor3D color_transparent;
    aiColor3D color_reflective;

    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_DIFFUSE, color_diffuse));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_AMBIENT, color_ambient));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_SPECULAR, color_specular));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_EMISSIVE, color_emissive));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_TRANSPARENT, color_transparent));
    ASSIMP_CHECK(old_material.Get(AI_MATKEY_COLOR_REFLECTIVE, color_reflective));

    new_material.color_diffuse = color::from_rgbaf(color_diffuse.r, color_diffuse.g, color_diffuse.b);
    new_material.color_ambient = color::from_rgbaf(color_ambient.r, color_ambient.g, color_ambient.b);
    new_material.color_specular = color::from_rgbaf(color_specular.r, color_specular.g, color_specular.b);
    new_material.color_emissive = color::from_rgbaf(color_emissive.r, color_emissive.g, color_emissive.b);
    new_material.color_transparent = color::from_rgbaf(color_transparent.r, color_transparent.g, color_transparent.b);
    new_material.color_reflective = color::from_rgbaf(color_reflective.r, color_reflective.g, color_reflective.b);

    ICY_ERROR(materials.push_back(std::move(new_material)));
    return error_type();
}
error_type assimp_scene::import_mesh(const aiMesh& old_mesh) noexcept
{
    mesh_tuple new_mesh;
    if (error = init_header(new_mesh, resource_type::mesh, old_mesh.mName))
        return error_type();

    if (old_mesh.HasBones())
    {
        for (auto k = 0u; k < old_mesh.mNumBones; ++k)
        {
            const auto& old_bone = *old_mesh.mBones[k];
            render_bone new_bone;

            string_view str;
            ICY_ERROR(to_string(const_array_view<char>(old_bone.mName.data, old_bone.mName.length), str));
            ICY_ERROR(to_string(str, new_bone.name));
            
            for (auto n = 0u; n < old_bone.mNumWeights; ++n)
            {
                render_bone::weight new_weight;
                new_weight.vertex = old_bone.mWeights[n].mVertexId;
                new_weight.value = old_bone.mWeights[n].mWeight;
                ICY_ERROR(new_bone.weights.push_back(new_weight));
            }
            aiVector3D world;
            aiVector3D scale;
            aiQuaternion angle;
            old_bone.mOffsetMatrix.Decompose(scale, angle, world);
            new_bone.transform.world = { world.x, world.y, world.z };
            new_bone.transform.scale = { scale.x, scale.y, scale.z };
            new_bone.transform.angle = { angle.x, angle.y, angle.z, angle.w };

            ICY_ERROR(new_mesh.bones.push_back(std::move(new_bone)));
        }
    }
    if (old_mesh.mVertices)
    {
        for (auto k = 0u; k < old_mesh.mNumVertices; ++k)
        {
            const auto vertex = old_mesh.mVertices[k];
            ICY_ERROR(new_mesh.world.push_back({ vertex.x, vertex.y, vertex.z }));
        }
    }
    if (old_mesh.mNormals)
    {
        for (auto k = 0u; k < old_mesh.mNumVertices; ++k)
        {
            const auto normal = old_mesh.mNormals[k];
            render_vec4 angle;
            if (old_mesh.mTangents)
            {
                const auto tangent = old_mesh.mTangents[k];
                angle = make_quat(normal, tangent);
            }
            else
            {
                angle = { normal.x, normal.y, normal.z, 1 };
            }
            ICY_ERROR(new_mesh.angle.push_back(angle));
        }
    }

    for (auto k = 0u; k < std::min(old_mesh.GetNumUVChannels(), old_mesh.GetNumColorChannels()); ++k)
    {
        for (auto n = 0u; n < old_mesh.mNumVertices; ++n)
        {
            const auto color = old_mesh.mColors[k][n];
            const auto tex = old_mesh.mTextureCoords[k][n];
            ICY_ERROR(new_mesh.channels[k].color.push_back(color::from_rgbaf(color.r, color.g, color.b, color.a)));
            ICY_ERROR(new_mesh.channels[k].tex.push_back({ tex.x, tex.y }));
        }

    }
    
    switch (old_mesh.mPrimitiveTypes)
    {
    case aiPrimitiveType_POINT: static_cast<render_mesh&>(new_mesh).type = render_mesh_type::point; break;
    case aiPrimitiveType_LINE: static_cast<render_mesh&>(new_mesh).type = render_mesh_type::line; break;
    case aiPrimitiveType_TRIANGLE: static_cast<render_mesh&>(new_mesh).type = render_mesh_type::triangle; break;
    default: error = make_stdlib_error(std::errc::invalid_argument); return error_type();
    }

    for (auto k = 0u; k < old_mesh.mNumFaces; ++k)
    {
        const auto face = old_mesh.mFaces[k];
        for (auto n = 0u; n < face.mNumIndices; ++n)
            ICY_ERROR(new_mesh.indices.push_back(face.mIndices[n]));
    }
    if (old_mesh.mMaterialIndex >= materials.size())
    {
        error = make_stdlib_error(std::errc::invalid_argument);
        return error_type();
    }
    new_mesh.material = materials[old_mesh.mMaterialIndex].index;
    ICY_ERROR(meshes.push_back(std::move(new_mesh)));
    return error_type();
}
error_type assimp_scene::import_node(const aiNode& old_node, const guid& parent, guid& new_index) noexcept
{
    node_tuple new_node;
    if (error = init_header(new_node, resource_type::node, old_node.mName))
        return error_type();
    if (&old_node == source->mRootNode)
        new_node.index = root;

    new_index = new_node.index;
    if (old_node.mChildren)
    {
        for (auto k = 0u; k < old_node.mNumChildren && !error; ++k)
        {
            guid child_index;
            ICY_ERROR(import_node(*old_node.mChildren[k], new_index, child_index));
            ICY_ERROR(new_node.nodes.push_back(child_index));
        }
    }
    if (old_node.mMeshes)
    {
        for (auto k = 0u; k < old_node.mNumMeshes && !error; ++k)
        {
            const auto mesh_index = old_node.mMeshes[k];
            if (mesh_index >= meshes.size())
            {
                error = make_stdlib_error(std::errc::invalid_argument);
                break;
            }
            ICY_ERROR(new_node.meshes.push_back(meshes[mesh_index].index));
            meshes[mesh_index].ref += 1;
            const auto material = source->mMeshes[old_node.mMeshes[k]]->mMaterialIndex;
            materials[material].ref += 1;
            for (auto&& texture_tuple : materials[material].textures)
            {
                for (auto&& texture : textures)
                {
                    if (texture.value.index == texture_tuple.index)
                        texture.value.ref += 1;
                }
            }
        }
    }
    aiVector3D world;
    aiVector3D scale;
    aiQuaternion angle;
    old_node.mTransformation.Decompose(scale, angle, world);
    new_node.transform.world = { world.x, world.y, world.z };
    new_node.transform.scale = { scale.x, scale.y, scale.z };
    new_node.transform.angle = { angle.x, angle.y, angle.z, angle.w };
    ICY_ERROR(nodes.insert(new_node.index, std::move(new_node)));
    return error_type();
}
error_type assimp_scene::import_light(const aiLight& old_light) noexcept
{
    string_view str;
    ICY_ERROR(to_string(const_array_view<char>(old_light.mName.data, old_light.mName.length), str));
    
    for (auto&& node : nodes)
    {
        if (node.value.name == str)
        {
            auto& new_light = node.value.light = make_unique<render_light>();
            if (!new_light)
                return make_stdlib_error(std::errc::not_enough_memory);

            switch (old_light.mType)
            {
            case aiLightSource_DIRECTIONAL: new_light->type = render_light_type::dir; break;
            case aiLightSource_POINT: new_light->type = render_light_type::point; break;
            case aiLightSource_SPOT: new_light->type = render_light_type::spot; break;
            case aiLightSource_AMBIENT: new_light->type = render_light_type::ambient; break;
            case aiLightSource_AREA: new_light->type = render_light_type::area; break;
            }
            new_light->size = { old_light.mSize.x, old_light.mSize.y };
            new_light->world = { old_light.mPosition.x, old_light.mPosition.y, old_light.mPosition.z };
            new_light->angle = { old_light.mDirection.x, old_light.mDirection.y, old_light.mDirection.z, 1 };
            new_light->factor_const = old_light.mAttenuationConstant;
            new_light->factor_linear = old_light.mAttenuationLinear;
            new_light->factor_quad = old_light.mAttenuationQuadratic;
            new_light->diffuse = color::from_rgbaf(old_light.mColorDiffuse.r, old_light.mColorDiffuse.g, old_light.mColorDiffuse.b);
            new_light->specular = color::from_rgbaf(old_light.mColorSpecular.r, old_light.mColorSpecular.g, old_light.mColorSpecular.b);
            new_light->ambient = color::from_rgbaf(old_light.mColorAmbient.r, old_light.mColorAmbient.g, old_light.mColorAmbient.b);
            new_light->angle_inner = old_light.mAngleInnerCone;
            new_light->angle_outer = old_light.mAngleOuterCone;
        }
    }
    return error_type();
}
error_type assimp_scene::import_camera(const aiCamera& old_camera) noexcept
{
    string_view str;
    ICY_ERROR(to_string(const_array_view<char>(old_camera.mName.data, old_camera.mName.length), str));
    
    for (auto&& node : nodes)
    {
        if (node.value.name == str)
        {
            auto& new_camera = node.value.camera = make_unique<render_camera>();
            if (!new_camera)
                return make_stdlib_error(std::errc::not_enough_memory);

            new_camera->aspect = old_camera.mAspect;
            new_camera->fov = old_camera.mHorizontalFOV;
            new_camera->zmin = old_camera.mClipPlaneNear;
            new_camera->zmax = old_camera.mClipPlaneFar;
            new_camera->world = { old_camera.mPosition.x, old_camera.mPosition.y, old_camera.mPosition.z };
            new_camera->angle = make_quat(old_camera.mLookAt, old_camera.mUp);
        }
    }
    
    return error_type();
}
error_type assimp_scene::import_animation(const aiAnimation& old_animation) noexcept
{
    animation_tuple new_animation;
    if (error = init_header(new_animation, resource_type::animation, old_animation.mName))
        return error_type();

    new_animation.frames = old_animation.mDuration;
    new_animation.fps = old_animation.mTicksPerSecond;

    for (auto k = 0u; k < old_animation.mNumChannels; ++k)
    {
        const auto& channel = *old_animation.mChannels[k];

        string_view str;
        ICY_ERROR(to_string(const_array_view<char>(channel.mNodeName.data, channel.mNodeName.length), str));

        for (auto&& node : nodes)
        {
            if (node.value.name != str)
                continue;

            render_animation_node new_channel;
            for (auto n = 0u; n < channel.mNumPositionKeys; ++n)
            {
                const auto key = channel.mPositionKeys[n];
                render_animation_node::step3 step;
                step.value = { key.mValue.x, key.mValue.y, key.mValue.z };
                step.time = key.mTime;
                ICY_ERROR(new_channel.world.push_back(step));
            }
            for (auto n = 0u; n < channel.mNumScalingKeys; ++n)
            {
                const auto key = channel.mScalingKeys[n];
                render_animation_node::step3 step;
                step.value = { key.mValue.x, key.mValue.y, key.mValue.z };
                step.time = key.mTime;
                ICY_ERROR(new_channel.scale.push_back(step));
            }
            for (auto n = 0u; n < channel.mNumRotationKeys; ++n)
            {
                const auto key = channel.mRotationKeys[n];
                render_animation_node::step4 step;
                step.value = { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w };
                step.time = key.mTime;
                ICY_ERROR(new_channel.angle.push_back(step));
            }
            switch (channel.mPreState)
            {
            case aiAnimBehaviour_DEFAULT: new_channel.prefix = render_animation_type::none; break;
            case aiAnimBehaviour_CONSTANT: new_channel.prefix = render_animation_type::first; break;
            case aiAnimBehaviour_LINEAR: new_channel.prefix = render_animation_type::linear; break;
            case aiAnimBehaviour_REPEAT: new_channel.prefix = render_animation_type::repeat; break;
            }
            switch (channel.mPostState)
            {
            case aiAnimBehaviour_DEFAULT: new_channel.suffix = render_animation_type::none; break;
            case aiAnimBehaviour_CONSTANT: new_channel.suffix = render_animation_type::first; break;
            case aiAnimBehaviour_LINEAR: new_channel.suffix = render_animation_type::linear; break;
            case aiAnimBehaviour_REPEAT: new_channel.suffix = render_animation_type::repeat; break;
            }
            ICY_ERROR(new_animation.nodes.insert(node.key, std::move(new_channel)));
            break;
        }
    }
    ICY_ERROR(animations.push_back(std::move(new_animation)));
    return error_type();
}