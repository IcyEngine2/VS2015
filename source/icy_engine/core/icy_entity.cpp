#include <icy_engine/core/icy_entity.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
mutex instance_lock;
entity_system* instance_ptr = nullptr;
class entity_system_data : public entity_system
{
public:
    ~entity_system_data() noexcept override
    {
        ICY_LOCK_GUARD(instance_lock);
        if (instance_ptr == this)
            instance_ptr = nullptr;
        filter(0);
    }
    error_type initialize() noexcept
    {
        ICY_ERROR(m_lock.initialize());
        ICY_ERROR(m_data.insert(0, entity_data()));
        filter(event_type::system_internal);
        return error_type();
    }
private:
    error_type exec() noexcept override
    {
        return make_stdlib_error(std::errc::function_not_supported);
    }
    error_type signal(const event_data* event) noexcept override
    {
        return error_type();
    }
    error_type create_entity(const entity parent, const component_type type, entity& output) noexcept override;
    error_type remove_entity(const entity entity) noexcept override;
    error_type query_entity(const entity entity, entity_data& data) const noexcept override;
    error_type modify_component(const entity entity, const component_type type, const variant& value) noexcept override;
private:
    mutable mutex m_lock;
    uint32_t m_index = 0;
    map<uint64_t, entity_data> m_data;
};
ICY_STATIC_NAMESPACE_END

error_type entity_system_data::create_entity(const entity parent, const component_type type, entity& output) noexcept
{
    {
        ICY_LOCK_GUARD(m_lock);

        auto it = m_data.find(parent.index());
        if (it == m_data.end())
            return make_stdlib_error(std::errc::invalid_argument);
        ICY_ERROR(it->value.children.reserve(it->value.children.size() + 1));

        output = entity(m_index + 1, type);
        ICY_ERROR(it->value.children.push_back(output));

        entity_data new_data;
        new_data.parent = parent;
        if (const auto error = m_data.insert(output.index(), std::move(new_data)))
        {
            it->value.children.pop_back();
            return error;
        }
        m_index += 1;
    }
    entity_event new_event;
    new_event.entity = output;
    new_event.type = entity_event_type::create_entity;
    new_event.value = parent;
    ICY_ERROR(event::post(this, event_type::entity_event, new_event));
    return error_type();
}
error_type entity_system_data::remove_entity(const entity entity) noexcept
{
    if (entity.index() == 0)
        return make_stdlib_error(std::errc::invalid_argument);
    
    const auto it = m_data.find(entity.index());
    if (it == m_data.end())
        return make_stdlib_error(std::errc::invalid_argument);

    m_data.erase(it);
    auto children = std::move(it->value.children);
    for (auto&& child : children)
    {
        ICY_ERROR(remove_entity(child));
    }
    entity_event new_event;
    new_event.entity = entity;
    new_event.type = entity_event_type::remove_entity;
    ICY_ERROR(event::post(this, event_type::entity_event, new_event));
    return error_type();
}
error_type entity_system_data::query_entity(const entity entity, entity_data& output) const noexcept
{
    ICY_LOCK_GUARD(m_lock);

    auto it = m_data.find(entity.index());
    if (it == m_data.end())
        return make_stdlib_error(std::errc::invalid_argument);

    ICY_ERROR(copy(it->value.data, output.data));
    ICY_ERROR(copy(it->value.children, output.children));
    output.parent = it->value.parent;
    return error_type();
}
error_type entity_system_data::modify_component(const entity entity, const component_type type, const variant& value) noexcept
{
    if (!(uint32_t(entity.types()) & uint32_t(type)) || !entity.index())
        return make_stdlib_error(std::errc::invalid_argument);

    {
        ICY_LOCK_GUARD(m_lock);
        const auto it = m_data.find(entity.index());
        if (it == m_data.end())
            return make_stdlib_error(std::errc::invalid_argument);

        auto jt = it->value.data.find(type);
        if (!value)
        {
            if (jt != it->value.data.end())
            {
                it->value.data.erase(jt);
            }
        }
        else
        {
            if (jt == it->value.data.end())
            {
                variant tmp = value;
                ICY_ERROR(it->value.data.insert(type, std::move(tmp)));
            }
            else
            {
                jt->value = value;
            }
        }
    }
    entity_event new_event;
    new_event.entity = entity;
    new_event.type = entity_event_type::modify_component;
    new_event.value = value;
    new_event.ctype = type;
    ICY_ERROR(event::post(this, event_type::entity_event, new_event));
    return error_type();
}


error_type icy::create_entity_system(shared_ptr<entity_system>& system) noexcept
{
    shared_ptr<entity_system_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize());

    system = std::move(new_ptr);

    ICY_LOCK_GUARD(instance_lock);
    if (instance_ptr == nullptr)
        instance_ptr = system.get();
    return error_type();
}

shared_ptr<entity_system> entity_system::global() noexcept
{
    ICY_LOCK_GUARD(instance_lock);
    return make_shared_from_this(instance_ptr);
}

static detail::global_init_entry g_init([] { return instance_lock.initialize(); });