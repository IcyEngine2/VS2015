#pragma once

#include "icy_smart_pointer.hpp"

namespace icy
{
    namespace detail
    {
        template<typename F> class function;
        template<typename R, typename... T>
        class function<R(T...)>
        {
            static constexpr size_t dynamic_flag = SIZE_MAX / 2 + 1;
            class data_base
            {
            public:
				data_base() noexcept = default;
                virtual ~data_base() noexcept = default;
                virtual R operator()(const T&... args) = 0;
                void add_ref() noexcept
                {
                    m_counter.fetch_add(1, std::memory_order_release);
                }
				void release() noexcept
                {
                    if (m_counter.fetch_sub(1, std::memory_order_acq_rel) == 1)
						destroy();
                } 
			protected:
				virtual void destroy() noexcept = 0;
            private:
                std::atomic<uint32_t> m_counter = 1;
            };
            template<typename U>
            class data_type : public data_base
            {
                static_assert(!std::is_reference<U>::value, "");
            public:
                data_type(const U& func) noexcept : call{ func }
                {

                }
                data_type(U&& func) noexcept : call{ std::move(func) }
                {

                }
                data_type(const data_type&) = delete;
                ~data_type() noexcept = default;
                R operator()(const T&... args) noexcept
                {
                    return call(args...);
                }
			private:
				auto destroy() noexcept -> void
				{
					allocator_type::destroy(this);
                    allocator_type::deallocate(this);
				}
            private:
                U call;
            };
        public:
            rel_ops(function);
            using return_type = R;
            using function_type = R(*)(T...);
            struct arg_types
            {
                static constexpr size_t arity = sizeof...(T);
                template<size_t N> using get = std::tuple_element_t<N, std::tuple<T...>>;
            };
            using type = function;
        public:
            function() noexcept = default;
            function(const function& rhs) : m_ptr{ rhs.m_ptr }
            {
                if (is_dynamic())
                    dynamic().add_ref();
            }
            function(function&& rhs) noexcept : m_ptr{ rhs.m_ptr }
            {
                rhs.m_ptr = 0;
            }
            ICY_DEFAULT_COPY_ASSIGN(function);
            ICY_DEFAULT_MOVE_ASSIGN(function);
            ~function() noexcept
            {
                if (is_dynamic())
                    dynamic().release();
            }
            function(std::nullptr_t) noexcept
            {

            }
            function(const function_type ptr) noexcept : m_ptr(reinterpret_cast<size_t>(ptr))
            {

            }
			template<typename U>
			error_type initialize(U&& func) noexcept
			{
				using data_type = type::data_type<remove_cvr<U>>;
				auto ptr = allocator_type::allocate<data_type>(1);
				if (!ptr)
					return make_stdlib_error(std::errc::not_enough_memory);

				ICY_SCOPE_FAIL{ allocator_type::deallocate(ptr); };
                allocator_type::construct(ptr, std::forward<U>(func));
				m_ptr = reinterpret_cast<size_t>(ptr) + dynamic_flag;
                return error_type();
			}
        public:
            explicit operator bool() const noexcept
            {
                return !!m_ptr;
            }
            bool is_dynamic() const noexcept
            {
                return (m_ptr >= dynamic_flag);
            }
            return_type operator()(T... args) const noexcept
            {
                ICY_ASSERT(*this, "CALLING UNINITIALIZED FUNCTION");
                if (is_dynamic())
                    return dynamic()(args...);
                else 
                    return reinterpret_cast<function_type>(m_ptr)(args...);
            }
            auto compare(const function& rhs) noexcept
            {
                return icy::compare(m_ptr, rhs.m_ptr);
            }
            auto value() const noexcept
            {
                return m_ptr;
            }
        private:
            data_base& dynamic() const noexcept
            {
                return *reinterpret_cast<data_base*>(m_ptr - dynamic_flag);
            }
        private:
            size_t m_ptr = 0;
        };
    }
    using detail::function;    
}