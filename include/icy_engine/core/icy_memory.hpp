#pragma once

#include "icy_atomic.hpp"
#include <intrin.h>

#pragma warning(push)
#pragma warning(disable:4324) // structure was padded due to alignment
#pragma warning(disable:4201) // nameless struct/union

#ifndef SYSTEM_CACHE_ALIGNMENT_SIZE
#define SYSTEM_CACHE_ALIGNMENT_SIZE 64
#endif

namespace icy
{
	inline constexpr size_t operator""_kb(const uint64_t arg) noexcept
	{
		return size_t(arg << 10);
	}
	inline constexpr size_t operator""_mb(const uint64_t arg) noexcept
	{
		return size_t(arg << 20);
	}
	inline constexpr size_t operator""_gb(const uint64_t arg) noexcept
	{
		return size_t(arg << 30);
	}
	inline constexpr size_t operator""_tb(const uint64_t arg) noexcept
	{
		return size_t(arg << 40);
	}
    size_t align_size(const size_t size) noexcept;

    using realloc_func = void* (*)(const void* const old_ptr, const size_t new_size, void* user);
    using memsize_func = size_t(*)(const void* const ptr, void* user);
    struct global_heap_type
    {
        realloc_func realloc = nullptr;
        memsize_func memsize = nullptr;
        void* user = nullptr;
    };
    namespace detail { extern global_heap_type global_heap; }

    inline void* realloc(const void* const old_ptr, const size_t new_size) noexcept
    {
        return detail::global_heap.realloc ? 
            detail::global_heap.realloc(old_ptr, new_size, detail::global_heap.user) : nullptr;
    }
    inline size_t memsize(const void* const ptr) noexcept
    {
        return detail::global_heap.memsize ?
            detail::global_heap.memsize(ptr, detail::global_heap.user) : 0_z;
    }

    struct heap_init
    {
        static heap_init global(const uint64_t capacity) noexcept
        {
            heap_init init(capacity);
            init.global_heap = true;
            init.multithread = true;
            init.disable_sys = false;
            return init;
        }
        heap_init(const uint64_t capacity) noexcept : capacity(capacity), max_size(0), bit_pattern(1),
            debug_trace(0), record_size(0), disable_sys(1), global_heap(0), multithread(0)
        {

        }
        uint64_t capacity;
        uint64_t max_size       :   0x30;
        uint64_t bit_pattern    :   0x01;   //  alloc/free bit patterns
        uint64_t debug_trace    :   0x01;   //	record call stack trace on allocations
        uint64_t record_size    :   0x01;   //  record each allocation size by category
        uint64_t disable_sys    :   0x01;   //  no system calls (big allocations)
        uint64_t global_heap    :   0x01;   //  set as default heap for "realloc" calls
        uint64_t multithread    :   0x01;   //  enable multithread access
    };
    class heap
    {
    public:
        heap() noexcept = default;
        heap(heap&& rhs) noexcept : m_init(std::move(rhs.m_init)), m_ptr(rhs.m_ptr)
        {
            rhs.m_ptr = nullptr;
        }
        ~heap() noexcept
        {
            shutdown();
        }
        heap_init info() const noexcept
        {
            return m_init;
        }
        void shutdown() noexcept;
        error_type initialize(const heap_init init) noexcept;
        void* realloc(const void* const old_ptr, const size_t new_size) noexcept;
        size_t memsize(const void* const ptr) noexcept;
    private:
        heap_init m_init = 0;
        void* m_ptr = nullptr;
    };
	struct heap_report
	{
        heap_report(const heap& heap) noexcept;
		size_t memory_reserved  =   0;
		size_t memory_passive   =   0;
		size_t memory_active    =   0;
		size_t threads_reserved =   0;
		size_t threads_active   =   0;
	};
    struct heap_debug
    {
        error_type initialize(const heap& heap) noexcept;
        virtual error_type operator()(const size_t index, const void* ptr, const char* func_name) noexcept;
    };
	
    class allocator_type
    {
    public:
        template<typename class_type>
        _DECLSPEC_ALLOCATOR static class_type* allocate(const size_t count) noexcept
        {
            return static_cast<class_type*>(icy::realloc(nullptr, sizeof(class_type) * count));
        }
        static void deallocate(void* const ptr) noexcept
        {
            icy::realloc(ptr, 0);
        }
        template<typename class_type, typename... arg_types>
        static void construct(class_type* const ptr, arg_types&& ... args) noexcept
        {
            static_assert(std::is_nothrow_constructible<class_type, arg_types...>::value, "INVALID TYPE");
            new (ptr) class_type{ std::forward<arg_types>(args)... };
        }
        template<typename class_type>
        static void destroy(class_type* const ptr) noexcept
        {
            if (ptr) ptr->~class_type();
        }
        template<typename class_type>
        static size_t size_of(const class_type* const ptr) noexcept
        {
            return icy::memsize(ptr) / sizeof(class_type);
        }
    };
}

#pragma warning(pop)