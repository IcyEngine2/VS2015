#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_string_view.hpp>

namespace icy
{
    extern const error_source error_source_minhook;

    class hook_base
    {
    public:
        hook_base() noexcept = default;
        hook_base(const hook_base&) noexcept = default;
        ~hook_base() noexcept
        {
            shutdown();
        }
        void shutdown() noexcept;
    protected:
        error_type initialize(void* const old_func, void* const new_func) noexcept;
        void* get() const noexcept
        {
            return m_old_func;
        }
    private:
        void* m_new_func = nullptr;
        void* m_old_func = nullptr;
    };

    template<typename func_type>
    class hook;

    template<typename R, typename... A>
    class hook<R(*)(A...)> : public hook_base
    {
    public:
        error_type initialize(R(*old_func)(A...), R(*new_func)(A...)) noexcept
        {
            return hook_base::initialize(old_func, new_func);
        }
        explicit operator bool() const noexcept
        {
            return !!get();
        }
        template<typename... U>
        R operator()(U&&... args) noexcept
        {
            return static_cast<R(*)(A...)>(hook_base::get())(std::forward<U>(args)...);
        }
    };
}