#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string_view.hpp>

using namespace icy;

ICY_DECLARE_GLOBAL(event_system::g_lock);
ICY_DECLARE_GLOBAL(event_system::g_list);
ICY_DECLARE_GLOBAL(event_system::g_error);

error_type event::post(event_system* const source, const event_type type) noexcept
{
    event_data* new_event = nullptr;
    ICY_ERROR(event_data::create(type, source, 0, new_event));
    const auto error = event_data::post(*new_event);
    new_event->release();
    return error;
}
void event_data::release() noexcept
{
    if (m_ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        if (m_destructor)
            m_destructor(reinterpret_cast<char*>(this) + sizeof(event_data));
        allocator_type::destroy(this);
        allocator_type::deallocate(this);
    }
}
error_type event_data::post(event_data& new_event) noexcept
{
    ICY_LOCK_GUARD(event_system::g_lock);
    auto next = event_system::g_list;
    auto error = error_type{};
    while (next)
    {
        if (next->m_mask & new_event.type)
        {
            auto next_error = next->post(new_event);
            if (!error && next_error)
                error = next_error;
        }
        next = next->m_prev;
    }
    return error;
}
error_type event_data::create(const event_type type, event_system* const source, const size_t type_size, event_data*& new_event) noexcept
{
    ICY_ASSERT((1ui64 << detail::log2(uint64_t(type))) == type, "INVALID EVENT TYPE");
    new_event = static_cast<event_data*>(icy::realloc(0, sizeof(event_data) + type_size));
    if (!new_event)
        return make_stdlib_error(std::errc::not_enough_memory);

    new (new_event) event_data(type, make_shared_from_this(source));
    return error_type();
}

error_type event_system::initialize() noexcept
{
    ICY_ERROR(g_lock.initialize());
    return error_type();
}
void event_system::filter(const uint64_t mask) noexcept
{
    if (mask == m_mask)
        return;

    ICY_LOCK_GUARD(g_lock);
    if (mask)
    {
        m_prev = g_list;
        g_list = this;
        m_mask = mask;
    }
    else
    {
        event_system* prev = nullptr;
        event_system* next = g_list;
        while (next)
        {
            if (next == this)
            {
                if (prev)
                    prev->m_prev = next->m_prev;
                else
                    g_list = next->m_prev;
                break;
            }
            else
            {
                prev = next;
                next = next->m_prev;
            }
        }
        //auto saved = make_shared_from_this(this);
        while (auto event = pop())
            ;
        m_mask = 0;
    }
}
event event_system::pop() noexcept
{
    const auto ptr = static_cast<event_ptr*>(m_queue.pop());
    event value;
    if (ptr)
    {
        value.m_ptr = ptr->value;
        icy::realloc(ptr, 0);
    }
    return value;
}
error_type event_system::post(event_system* const source, const event_type type) noexcept
{
    event_data* new_event = nullptr;
    ICY_ERROR(event_data::create(type, source, 0, new_event));
    const auto error = post(*new_event);
    new_event->release();
    return error;
}
error_type event_system::post(event_data& event) noexcept
{
    auto new_ptr = allocator_type::allocate<event_system::event_ptr>(1);
    if (!new_ptr)
        return make_stdlib_error(std::errc::not_enough_memory);

    new_ptr->value = &event;
    event.m_ref.fetch_add(1, std::memory_order_release);
    m_queue.push(new_ptr);
    ICY_ERROR(signal(&event));
    return error_type();
}
error_type icy::create_event_system(shared_ptr<event_queue>& queue, const uint64_t mask) noexcept
{
    shared_ptr<event_queue> new_queue;
    ICY_ERROR(make_shared(new_queue, event_queue::tag()));
    //ICY_ERROR(new_queue->m_mutex.initialize());
    ICY_ERROR(new_queue->m_cvar.initialize());
    new_queue->filter(mask);
    queue = std::move(new_queue);
    return error_type();
}
error_type event_queue::pop(event& event, const duration_type timeout) noexcept
{
    while (*this)
    {
        event = event_system::pop();
        if (event)
            break;
        if (timeout == duration_type() || !(*this))
            break;
        ICY_ERROR(m_cvar.wait(timeout));
    }
    return error_type();
}

string_view icy::to_string(const event_type type) noexcept
{
    switch (type)
    {
    case event_type::window_data:
        return "window data"_s;
    case event_type::window_state:
        return "window state"_s;
    case event_type::window_input:
        return "window input"_s;
    case event_type::window_resize:
        return "window resize"_s;
    default:
        break;
    }
    return ""_s;
}

event_type icy::next_event_user() noexcept
{
    static auto offset = 0ui64;
    return offset < event_type_enum::bitcnt_user ? event_user(offset++) : event_type::none;
}
error_type icy::post_quit_event() noexcept
{
    ICY_LOCK_GUARD(event_system::g_lock);
    auto next = event_system::g_list;
    error_type error;
    while (next)
    {
        next->m_quit.store(true, std::memory_order_release);
        auto next_error = next->signal(nullptr);

        if (!error && next_error)
            error = next_error;
        next = next->m_prev;
    }
    return error;
}
error_type icy::post_error_event(const error_type error) noexcept
{
    {
        ICY_LOCK_GUARD(event_system::g_lock);
        event_system::g_error = error;
    }
    return post_quit_event();
}
