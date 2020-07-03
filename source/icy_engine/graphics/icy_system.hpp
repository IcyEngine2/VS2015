#pragma once

#include <icy_engine/core/icy_memory.hpp>

namespace icy
{
    template<typename T>
    void any_system_shutdown(T*& data) noexcept
    {
        if (data)
        {
            allocator_type::destroy(data);
            allocator_type::deallocate(data);
        }
        data = nullptr;
    }
    template<typename T>
    void any_system_shutdown_ref(T*& data) noexcept
    {
        if (data && data->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            allocator_type::destroy(data);
            allocator_type::deallocate(data);
        }
        data = nullptr;
    }
    template<typename T, typename... arg_types>
    error_type any_system_initialize(T*& data, arg_types&&... args) noexcept
    {
        data = allocator_type::allocate<T>(1);
        if (!data)
            return make_stdlib_error(std::errc::not_enough_memory);

        allocator_type::construct(data);
        if (const auto error = data->initialize(std::forward<arg_types>(args)...))
        {
            allocator_type::destroy(data);
            allocator_type::deallocate(data);
            return error;
        }
        return error_type();
    }
    template<typename T>
    class any_system_acqrel_pointer
    {
    public:
        any_system_acqrel_pointer() noexcept = default;
        any_system_acqrel_pointer(const any_system_acqrel_pointer&) noexcept = delete;
        ~any_system_acqrel_pointer() noexcept
        {
            if (!m_ptr)
                return;

            static constexpr auto shift_ptr = 0x00;
            static constexpr auto shift_ref = 0x30;
            static constexpr auto mask_ptr = (1ui64 << shift_ref) - 1;
            static constexpr auto mask_ref = UINT64_MAX - mask_ptr;

            auto& instance = get_instance();
            auto old_instance = instance.load(std::memory_order_acquire);
            while (true)
            {
                const auto ptr = (old_instance & mask_ptr) >> shift_ptr;
                const auto ref = (old_instance & mask_ref) >> shift_ref;
                if (ref == 0 || reinterpret_cast<T*>(ptr) != m_ptr)
                {
                    ICY_ASSERT(false, "CORRUPTED MEMORY");
                }
                if (ref == 1)
                {
                    if (instance.compare_exchange_strong(old_instance, 0, std::memory_order_acq_rel))
                    {
                        any_system_shutdown(m_ptr);
                        break;
                    }
                }
                else // if (ref > 1)
                {
                    const auto new_instance = (ptr << shift_ptr) | ((ref - 1) << shift_ref);
                    if (instance.compare_exchange_strong(old_instance, new_instance, std::memory_order_acq_rel))
                    {
                        break;
                    }
                }
            }
        }
        template<typename... arg_types>
        error_type initialize(arg_types&& ... args) noexcept
        {
            static constexpr auto shift_ptr = 0x00;
            static constexpr auto shift_ref = 0x30;
            static constexpr auto mask_ptr = (1ui64 << shift_ref) - 1;
            static constexpr auto mask_ref = UINT64_MAX - mask_ptr;

            auto& instance = get_instance();
            auto old_instance = instance.load(std::memory_order_acquire);
            while (true)
            {
                if (old_instance)
                {
                    const auto ptr = (old_instance & mask_ptr) >> shift_ptr;
                    const auto ref = (old_instance & mask_ref) >> shift_ref;
                    const auto new_instance = (ptr << shift_ptr) | ((ref + 1) << shift_ref);
                    if (instance.compare_exchange_weak(old_instance, new_instance, std::memory_order_acq_rel))
                    {
                        m_ptr = reinterpret_cast<T*>(ptr);
                        break;
                    }
                }
                else
                {
                    T* new_system = nullptr;
                    ICY_ERROR(any_system_initialize(new_system, std::forward<arg_types>(args)...));
                    auto new_ptr = reinterpret_cast<uint64_t>(new_system);
                    if ((new_ptr & mask_ptr) != new_ptr)
                    {
                        any_system_shutdown(new_system);
                        return make_stdlib_error(std::errc::bad_address);
                    }
                    const auto new_instance = (new_ptr << shift_ptr) | (1ui64 << shift_ref);
                    if (instance.compare_exchange_strong(old_instance, new_instance, std::memory_order_acq_rel))
                    {
                        m_ptr = new_system;
                        break;
                    }
                    else
                    {
                        any_system_shutdown(new_system);
                    }
                }
            }
            return error_type();
        }
        T* operator->() const noexcept
        {
            return m_ptr;
        }
    private:
        static std::atomic<uint64_t>& get_instance() noexcept
        {
#if _DEBUG
            struct instance_type
            {
                ~instance_type() noexcept
                {
                    ICY_ASSERT(value == 0, "INSTANCE REF COUNT MUST BE 0");
                }
                std::atomic<uint64_t> value;
            };
            static instance_type value;
            return value.value;
#else
            static std::atomic<uint64_t> value;
            return value;
#endif
        }
        T* m_ptr = nullptr;
    };
}