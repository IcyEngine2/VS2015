#include <icy_engine/core/icy_event.hpp>
#include <icy_engine/core/icy_string_view.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG
class event_loop_data : public event_loop
{
public:
    error_type initialize(const uint64_t mask) noexcept
    {
        ICY_ERROR(m_mutex.initialize());
        filter(mask);
        return {};
    }
private:
    error_type loop(event& event, const duration_type timeout) noexcept override 
    {
        while (true)
        {
            event = pop();
            if (event)
                break;
            ICY_ERROR(m_cvar.wait(m_mutex, timeout));
        }
        return {};
    }
    error_type signal(const event_data&) noexcept override
    {
        m_cvar.wake();
        return {};
    }
private:
    mutex m_mutex;
    cvar m_cvar;
};
ICY_STATIC_NAMESPACE_END

error_type event::post(event_queue* const source, const event_type type) noexcept
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
    ICY_LOCK_GUARD_READ(event_queue::g_lock);
    auto next = event_queue::g_list;
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
error_type event_data::create(const event_type type, event_queue* const source, const size_t type_size, event_data*& new_event) noexcept
{
    ICY_ASSERT((1ui64 << detail::log2(uint64_t(type))) == type, "INVALID EVENT TYPE");
    new_event = static_cast<event_data*>(icy::realloc(0, sizeof(event_data) + type_size));
    if (!new_event)
        return make_stdlib_error(std::errc::not_enough_memory);

    new (new_event) event_data(type, make_shared_from_this(source));
    return {};
}
void event_queue::filter(const uint64_t mask) noexcept
{
    if (mask == m_mask)
        return;

    ICY_LOCK_GUARD_WRITE(g_lock);
    if (mask)
    {
        m_prev = g_list;
        g_list = this;
        m_mask = mask | event_type::global_quit;
    }
    else
    {
        event_queue* prev = nullptr;
        event_queue* next = g_list;
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
event event_queue::pop() noexcept
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
error_type event_queue::post(event_queue* const source, const event_type type) noexcept
{
    event_data* new_event = nullptr;
    ICY_ERROR(event_data::create(type, source, 0, new_event));
    const auto error = post(*new_event);
    new_event->release();
    return error;
}
error_type event_queue::post(event_data& event) noexcept
{
    auto new_ptr = allocator_type::allocate<event_queue::event_ptr>(1);
    if (!new_ptr)
        return make_stdlib_error(std::errc::not_enough_memory);

    new_ptr->value = &event;
    event.m_ref.fetch_add(1, std::memory_order_release);
    m_queue.push(new_ptr);
    ICY_ERROR(signal(event));
    return {};
}
detail::rw_spin_lock<> event_queue::g_lock;
event_queue* event_queue::g_list;
error_type event_loop::create(shared_ptr<event_loop>& loop, const uint64_t mask) noexcept
{
    shared_ptr<event_loop_data> new_ptr;
    ICY_ERROR(make_shared(new_ptr));
    ICY_ERROR(new_ptr->initialize(mask));
    loop = new_ptr;
    return {};
}
string_view icy::to_string(const event_type type) noexcept
{
    switch (type)
    {
    case event_type::window_active:
        return "window active"_s;
    case event_type::window_close:
        return "window _close"_s;
    case event_type::window_input:
        return "window input"_s;
    case event_type::window_minimized:
        return "window minimized"_s;
    case event_type::window_resize:
        return "window resize"_s;
    default:
        break;
    }
    return ""_s;
}