#pragma once

#include "icy_array.hpp"

namespace icy
{
    template<typename T>
    class mpsc_queue
    {
    public:
        using allocator_type = allocator_type;
        using type = mpsc_queue<T>;
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        struct pair_type
        {
            void* unused = nullptr;
            T value;
        };
    public:
        mpsc_queue() noexcept = default;
        mpsc_queue(const mpsc_queue&) = delete;
        mpsc_queue& operator=(const mpsc_queue&) = delete;
        mpsc_queue(mpsc_queue&& rhs) noexcept : m_queue(std::move(rhs.m_queue)), m_size(rhs.m_size.load())
        {

        }
        ICY_DEFAULT_MOVE_ASSIGN(mpsc_queue);
        ~mpsc_queue() noexcept
        {
            clear();
        }
    public:
        error_type push(const T& value) noexcept
        {
            T tmp;
            ICY_ERROR(copy(value, tmp));
            return emplace(std::move(tmp));
        }
        error_type push(T&& value) noexcept
        {
            return emplace(std::move(value));
        }
        template<typename... arg_types>
        error_type emplace(arg_types&&... args) noexcept
        {
            auto new_pair = allocator_type::allocate<pair_type>(1);
            if (!new_pair)
                return make_stdlib_error(std::errc::not_enough_memory);
            allocator_type::construct(&new_pair->value, std::forward<arg_types>(args)...);
            m_queue.push(new_pair);
            m_size.fetch_add(1, std::memory_order_release);
            return error_type();
        }
        bool pop(T& value) noexcept
        {
            auto pair = static_cast<pair_type*>(m_queue.pop());
            if (!pair)
                return false;
            m_size.fetch_sub(1, std::memory_order_release);
            value = std::move(pair->value);
            allocator_type::destroy(&pair->value);
            allocator_type::deallocate(pair);
            return true;
        }
        void clear() noexcept
        {
            while (auto pair = static_cast<pair_type*>(m_queue.pop()))
            {
                allocator_type::destroy(&pair->value);
                allocator_type::deallocate(pair);
                m_size.fetch_sub(1, std::memory_order_release);
            }
        }
        bool empty() const noexcept
        {
            return !!m_size.load(std::memory_order_acquire);
        }
        size_type size() const noexcept
        {
            return m_size.load(std::memory_order_acquire);
        }
    private:
        detail::intrusive_mpsc_queue m_queue;
        std::atomic<size_type> m_size = 0;
    };
}