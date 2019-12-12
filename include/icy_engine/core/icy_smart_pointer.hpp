#pragma once

#include "icy_memory.hpp"

#pragma push_macro("small")
#undef small

namespace icy
{
	template<typename T> class unique_ptr;
    template<typename T> class shared_ptr;
    template<typename T, typename... arg_types> unique_ptr<T> make_unique(arg_types&&... args) noexcept;

    template<typename T>
    class unique_ptr : public detail::rel_ops<unique_ptr<T>>
    {
        template<typename U> friend class unique_ptr;
        template<typename U, typename... arg_types> friend unique_ptr<U> make_unique(arg_types&&...) noexcept;
        explicit unique_ptr(T* const ptr) noexcept : m_ptr{ ptr }
        {

        }
    public:
        unique_ptr() noexcept = default;
        unique_ptr(unique_ptr&& rhs) noexcept : m_ptr{ rhs.m_ptr }
        {
            new (&rhs) unique_ptr;
        }
		template<typename U>
		unique_ptr(unique_ptr<U>&& rhs) noexcept : m_ptr{ rhs.m_ptr }
		{
			new (&rhs) unique_ptr<U>;
		}
        ICY_DEFAULT_MOVE_ASSIGN(unique_ptr);
        ~unique_ptr() noexcept
        {
            if (m_ptr)
            {
                allocator_type::destroy(m_ptr);
                allocator_type::deallocate(m_ptr);
            }
        }
        const T* operator->() const noexcept
        {
            return m_ptr;
        }
        const T& operator*() const noexcept
        {
            return *m_ptr;
        }
        T* operator->() noexcept
        {
            return m_ptr;
        }
        T& operator*() noexcept
        {
            return *m_ptr;
        }
        explicit operator bool() const noexcept
        {
            return !!m_ptr;
        }
        T* get() noexcept
        {
            return m_ptr;
        }
        const T* get() const noexcept
        {
            return m_ptr;
        }
        T* release() noexcept
        {
            const auto ptr = m_ptr;
            m_ptr = nullptr;
            return ptr;
        }
        int compare(const unique_ptr<T>& rhs) const noexcept
        {
            return icy::compare(m_ptr, rhs.m_ptr);
        }
    private:
        T* m_ptr = nullptr;
    };

    namespace detail
    {
        struct shared_ptr_buffer
        {
            enum state
            {
                none        =   0x00,
                no_weak     =   0x01,
                no_strong   =   0x02,
            };
            static void to_counters(const uint64_t value, uint32_t& weak, uint32_t& strong) noexcept
            {
                weak    = (value >> 0x20) & UINT32_MAX;
                strong  = (value >> 0x00) & UINT32_MAX;
            }
            static uint64_t make_value(const uint32_t weak, const uint32_t strong) noexcept
            {
                return strong | (static_cast<uint64_t>(weak) << 0x20);
            }
            void initialize() noexcept
            {
                ref = make_value(1, 1);
                auto weak = 0u;
                auto strong = 0u;
                to_counters(ref, weak, strong);
                weak = 0;
            }
            void add_weak_ref() noexcept
            {
                auto value = ref.load(std::memory_order_acquire);
                while (true)
                {
                    auto weak = 0u;
                    auto strong = 0u;
                    to_counters(value, weak, strong);
                    if (ref.compare_exchange_weak(value, make_value(weak + 1, strong), std::memory_order_acq_rel))
                        return;
                }
            }
            void add_strong_ref(const uint32_t count = 1) noexcept
            {
                auto value = ref.load(std::memory_order_acquire);
                while (true)
                {
                    auto weak = 0u;
                    auto strong = 0u;
                    to_counters(value, weak, strong);
                    if (ref.compare_exchange_weak(value, make_value(weak + count, strong + count), std::memory_order_acq_rel))
                        return;
                }
            }
            //  true -> can deallocate
            bool del_weak_ref() noexcept
            {
                auto value = ref.load(std::memory_order_acquire);
                while (true)
                {
                    auto weak = 0u;
                    auto strong = 0u;
                    to_counters(value, weak, strong);
                    if (ref.compare_exchange_weak(value, make_value(weak - 1, strong), std::memory_order_acq_rel))
                        return weak == 1 && strong == 0;
                }
            }
            uint32_t del_strong_ref(const uint32_t count = 1) noexcept
            {
                auto value = ref.load(std::memory_order_acquire);
                while (true)
                {
                    auto weak = 0u;
                    auto strong = 0u;
                    to_counters(value, weak, strong);
                    if (ref.compare_exchange_weak(value, make_value(weak - count, strong - count), std::memory_order_acq_rel))
                    {
                        auto result = 0u;
                        if (strong == count) result |= state::no_strong;
                        if (strong == count && weak == count) result |= state::no_weak;
                        return result;
                    }
                }
            }
            bool try_acquire() noexcept
            {
                auto value = ref.load(std::memory_order_acquire);
                while (true)
                {
                    auto weak = 0u;
                    auto strong = 0u;
                    to_counters(value, weak, strong);
                    if (strong == 0)
                        return false;

                    if (ref.compare_exchange_weak(value, make_value(weak + 1, strong + 1), std::memory_order_acq_rel))
                        return true;
                }
            }
            std::atomic<uint64_t> ref;  //  32 bytes (weak/shared)
            char buffer[1];
        };
    }

    template<typename T>
    class weak_ptr
    {
        template<typename U> friend class shared_ptr;
        template<typename U> friend class weak_ptr;
    public:
        weak_ptr() noexcept = default;
        weak_ptr(const weak_ptr& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            if (m_ptr)
                m_ptr->add_weak_ref();
        }
        weak_ptr(weak_ptr&& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            rhs.m_ptr = nullptr;
        }
        template<typename U>
        weak_ptr(const weak_ptr<U>& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            static_assert(std::is_base_of<T, U>::value || std::is_same<T, U>::value, "INVALID WEAK_PTR ASSIGNMENT");
            if (m_ptr)
                m_ptr->add_weak_ref();
        }
        template<typename U>
        weak_ptr(weak_ptr<U>&& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            static_assert(std::is_base_of<T, U>::value || std::is_same<T, U>::value, "INVALID WEAK_PTR ASSIGNMENT");
            rhs.m_ptr = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(weak_ptr);
        ICY_DEFAULT_COPY_ASSIGN(weak_ptr);
        ~weak_ptr() noexcept
        {
            if (m_ptr)
            {
                if (m_ptr->del_weak_ref())
                    allocator_type::deallocate(m_ptr);
            }
        }
        template<typename U>        
        weak_ptr(const shared_ptr<U>& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            static_assert(std::is_base_of<T, U>::value || std::is_same<T, U>::value, "INVALID WEAK_PTR ASSIGNMENT");
            if (m_ptr)
                m_ptr->add_weak_ref();
        }
        explicit operator bool() const noexcept
        {
            return !!m_ptr;
        }
    private:
        detail::shared_ptr_buffer* m_ptr = nullptr;
    };

    template<typename T>
    class shared_ptr : public detail::rel_ops<shared_ptr<T>>
    {
        template<typename U> friend class shared_ptr;
        template<typename U> friend class weak_ptr;
        template<typename T>
        friend shared_ptr<T> make_shared_from_this(T* ptr) noexcept;
    public:
        shared_ptr() noexcept = default;
        shared_ptr(std::nullptr_t) noexcept
        {

        }
        shared_ptr(const shared_ptr& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            if (m_ptr)
                m_ptr->add_strong_ref();
        }
        shared_ptr(shared_ptr&& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            rhs.m_ptr = nullptr;
        }
        template<typename U>
        shared_ptr(const shared_ptr<U>& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            static_assert(std::is_base_of<T, U>::value || std::is_same<T, U>::value, "INVALID SHARED_PTR ASSIGNMENT");
            if (m_ptr)
                m_ptr->add_strong_ref();
        }
        template<typename U>
        shared_ptr(shared_ptr<U>&& rhs) noexcept : m_ptr(rhs.m_ptr)
        {
            static_assert(std::is_base_of<T, U>::value || std::is_same<T, U>::value, "INVALID WEAK_PTR ASSIGNMENT");
            rhs.m_ptr = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(shared_ptr);
        ICY_DEFAULT_COPY_ASSIGN(shared_ptr);
        ~shared_ptr() noexcept
        {
            if (m_ptr)
            {
                auto state = m_ptr->del_strong_ref();
                if (state & detail::shared_ptr_buffer::no_strong)
                {
                    m_ptr->add_strong_ref(INT32_MAX);
                    allocator_type::destroy(reinterpret_cast<T*>(m_ptr->buffer));
                    state = m_ptr->del_strong_ref(INT32_MAX);
                }
                if (state & detail::shared_ptr_buffer::no_weak)
                    allocator_type::deallocate(m_ptr);
            }
        }
        shared_ptr(const weak_ptr<T>& rhs) noexcept
        {
            if (rhs.m_ptr && rhs.m_ptr->try_acquire())
                m_ptr = rhs.m_ptr;
        }
        const T* operator->() const noexcept
        {
            return get();
        }
        const T& operator*() const noexcept
        {
            return *get();
        }
        T* operator->() noexcept
        {
            return get();
        }
        T& operator*() noexcept
        {
            return *get();
        }
        explicit operator bool() const noexcept
        {
            return !!m_ptr;
        }
        int compare(const shared_ptr<T>& rhs) const noexcept
        {
            return icy::compare(m_ptr, rhs.m_ptr);
        }
        T* get() noexcept
        {
            return m_ptr ? reinterpret_cast<T*>(m_ptr->buffer) : nullptr;
        }
        const T* get() const noexcept
        {
            return m_ptr ? reinterpret_cast<const T*>(m_ptr->buffer) : nullptr;
        }
        template<typename T, typename... arg_types>
        static error_type make_shared(shared_ptr<T>& ptr, arg_types&& ... args) noexcept
        {
            //static_assert(std::is_nothrow_constructible<T, arg_types...>::value, "INVALID CONSTRUCTOR (MUST NOT THROW)");
            auto new_ptr = static_cast<detail::shared_ptr_buffer*>(icy::realloc(nullptr, sizeof(uint64_t) + sizeof(T)));
            if (!new_ptr)
                return make_stdlib_error(std::errc::not_enough_memory);

            new_ptr->initialize();
            ICY_SCOPE_FAIL{ icy::realloc(new_ptr, 0); };
            new (reinterpret_cast<T*>(new_ptr->buffer)) T{ std::forward<arg_types>(args)... };
            ptr = {};
            ptr.m_ptr = new_ptr;
            return {};
        }
    private:
        detail::shared_ptr_buffer* m_ptr = nullptr;
    };

    template<typename T, typename... arg_types>
	unique_ptr<T> make_unique(arg_types&&... args) noexcept
    {
        auto ptr = allocator_type::allocate<T>(1);
        if (ptr)
            new (ptr) T{ std::forward<arg_types>(args)... };
        return unique_ptr<T>(ptr);
    }

    template<typename T>
    shared_ptr<T> make_shared_from_this(T* ptr) noexcept
    {
        if (!ptr)
            return {};

        shared_ptr<T> value;
        const auto offset = offsetof(detail::shared_ptr_buffer, buffer);
        union
        {
            T* as_type;
            char* as_char;
            detail::shared_ptr_buffer* as_buffer;
        };
        as_type = ptr;
        as_char -= offset;
        value.m_ptr = as_buffer;
        value.m_ptr->add_strong_ref();
        return value;
    }

    template<typename T, typename... arg_types>
    error_type make_shared(shared_ptr<T>& ptr, arg_types&& ... args) noexcept
    {
        //static_assert(std::is_nothrow_constructible<T, arg_types...>::value, "INVALID CONSTRUCTOR (MUST NOT THROW)");
        return shared_ptr<T>::make_shared(ptr, std::forward<arg_types>(args)...);
    }
}
#pragma pop_macro("small")