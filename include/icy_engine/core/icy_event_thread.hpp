#pragma once

#include "icy_event.hpp"
#include "icy_thread.hpp"
#include "icy_function.hpp"
#include <tuple>

namespace icy
{
    enum class event_thread_state : int
    {
        error = -1,
        none = 0,
        exec = 1,
        done = 2,
    };
    template<typename system_type>
    class event_thread : public thread
    {
        template<typename system_type, typename... arg_types>
        friend error_type create_event_thread(shared_ptr<event_thread<system_type>>& thread, arg_types&&... args) noexcept;
        struct tag { };
    public:
        event_thread(tag) noexcept
        {

        }
        error_type run() noexcept override
        {
            auto error = m_create(system);
            if (!error)
            {
                m_ready = true;
                if (error = system->exec())
                    event::post(nullptr, event_type::global_quit);
            }
            return error;
        }
        error_type launch() noexcept
        {
            ICY_ERROR(thread::launch());
            while (true)
            {
                const auto s = state();
                if (s == thread_state::done || m_ready)
                    break;
                else
                    sleep({});
            }
            return error();
        }
    public:
        shared_ptr<system_type> system;
    private:
        icy::function<error_type(shared_ptr<system_type>&)> m_create;
        std::atomic<bool> m_ready = false;
    };
    template<typename system_type, typename... arg_types>
    error_type create_event_thread(shared_ptr<event_thread<system_type>>& thread, arg_types&&... args) noexcept
    {
        shared_ptr<event_thread<system_type>> ptr;
        ICY_ERROR(make_shared(ptr, event_thread<system_type>::tag()));
        ICY_ERROR(ptr->m_create.initialize([args...](shared_ptr<system_type>& system) mutable 
        { 
            return create_event_system(system, std::move(args)...);
        }));
        thread = std::move(ptr);
        return error_type();
    }
}