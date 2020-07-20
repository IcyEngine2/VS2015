#pragma once

#include "icy_array_view.hpp"
#include "icy_smart_pointer.hpp"
#include "icy_string.hpp"

namespace icy
{
	template<typename T>
	class array : public array_view<T>
	{
		template<typename U>
		friend class array;
	public:
		using type = array<T>;
		using base = array_view<T>;
		using value_type = typename base::value_type;
		using pointer = typename base::pointer;
		using const_pointer = typename base::const_pointer;
		using reference = typename base::reference;
		using const_reference = typename base::const_reference;
		using size_type = typename base::size_type;
		using difference_type = typename base::difference_type;
		using const_iterator = typename base::const_iterator;
		using iterator = typename base::iterator;
		using const_reverse_iterator = typename base::const_reverse_iterator;
		using reverse_iterator = typename base::reverse_iterator;
	public:
		array() noexcept : array(nullptr, nullptr)
		{

		}
		array(heap* const heap) noexcept : array(heap ? heap_realloc : global_realloc, heap)
		{

		}
		array(const realloc_func realloc, void* const user) noexcept : 
			m_realloc(realloc ? realloc : global_realloc), m_user(user)
		{

		}
		array(const array&) = delete;
		array& operator=(const array&) = delete;
		array(array&& rhs) noexcept : m_realloc(rhs.m_realloc), m_user(rhs.m_user)
		{
			std::swap(array_view<T>::m_ptr, rhs.m_ptr);
			std::swap(array_view<T>::m_size, rhs.m_size);
            std::swap(m_capacity, rhs.m_capacity);
		}
		ICY_DEFAULT_MOVE_ASSIGN(array);
		~array() noexcept
		{
			for (auto k = array_view<T>::m_size; k--;)
				allocator_type::destroy(array_view<T>::m_ptr + k);
			m_realloc(array_view<T>::m_ptr, 0, m_user);
		}
	public:
		size_type capacity() const noexcept
		{
			return m_capacity;
		}
		error_type resize(const size_type size) noexcept
		{
			if (size > array_view<T>::m_size)
			{
				if (const auto error = reserve(size))
					return error;

				while (array_view<T>::m_size != size)
					construct(array_view<T>::m_ptr + (array_view<T>::m_size++));
			}
			else
			{
				pop_back(array_view<T>::m_size - size);
			}
			return error_type();
		}
		error_type reserve(const size_type capacity) noexcept
		{
			return _reserve(capacity, true);
		}
		error_type push_back(value_type&& value) noexcept
		{
			return emplace_back(std::move(value));
		}
		error_type push_back(const const_reference value) noexcept
		{
            value_type tmp;
            ICY_ERROR(copy(value, tmp));
			return emplace_back(std::move(tmp));
		}
		template<typename... arg_types>
		error_type emplace_back(arg_types&& ... args) noexcept
		{
            ICY_ERROR(reserve(array_view<T>::m_size + 1));
            allocator_type::construct(array_view<T>::m_ptr + array_view<T>::m_size, std::forward<arg_types>(args)...);
			++array_view<T>::m_size;
			return error_type();
		}

		template<typename iter_type>
		error_type append(std::move_iterator<iter_type> first, const std::move_iterator<iter_type> last) noexcept
		{
            ICY_ERROR(reserve(array_view<T>::size() + std::distance(first, last)));
			for (; first != last; ++first)
				construct(array_view<T>::m_ptr + (array_view<T>::m_size++), std::move(*first));
			return error_type();
		}
        template<typename iter_type>
        error_type append(iter_type first, const iter_type last) noexcept
        {
            ICY_ERROR(reserve(array_view<T>::size() + std::distance(first, last)));
            for (; first != last; ++first)
            {
                T tmp;
                ICY_ERROR(copy(*first, tmp));
                construct(array_view<T>::m_ptr + (array_view<T>::m_size++), std::move(tmp));
            }
            return error_type();
        }
		error_type append(const std::initializer_list<T> list)
		{
			return append(
				std::make_move_iterator(const_cast<pointer>(list.begin())),
				std::make_move_iterator(const_cast<pointer>(list.end())));
		}
		template<typename rhs_type>
		error_type append(rhs_type&& rhs) noexcept
		{
            return _append(std::is_rvalue_reference<decltype(rhs)>(), rhs);
		}
		template<typename iter_type>
		error_type assign(iter_type first, iter_type last) noexcept
		{
			array tmp;
            ICY_ERROR(tmp.append(first, last));
			*this = std::move(tmp);
			return error_type();
		}
		error_type assign(const std::initializer_list<T> list) noexcept
		{
			array tmp;
            ICY_ERROR(tmp.append(list));
			*this = std::move(tmp);
			return error_type();
		}
		template<typename rhs_type>
		error_type assign(rhs_type&& rhs) noexcept
		{
			array tmp;
            ICY_ERROR(tmp.append(std::forward<rhs_type>(rhs)));
			*this = std::move(tmp);
			return error_type();
		}

		void clear() noexcept
		{
			pop_back(array_view<T>::size());
		}
		void pop_back(const size_type count = 1) noexcept
		{
			ICY_ASSERT(count <= array_view<T>::size(), "INVALID POP_BACK COUNT");
			for (auto k = 0_z; k < count; ++k)
				allocator_type::destroy(array_view<T>::m_ptr + (--array_view<T>::m_size));
		}
	private:
        template<typename rhs_type>
        error_type _append(std::true_type, rhs_type&& rhs) noexcept
        {
            return append(std::make_move_iterator(std::begin(rhs)), std::make_move_iterator(std::end(rhs)));          
        }
        template<typename rhs_type>
        error_type _append(std::false_type, rhs_type&& rhs) noexcept
        {
            return append(std::begin(rhs), std::end(rhs));
        }
		error_type _reserve(const size_type capacity, const bool clear) noexcept
		{
			if (capacity > this->capacity())
			{
                const auto new_capacity = icy::align_size(capacity * sizeof(T)) / sizeof(T);
				const auto ptr = static_cast<T*>(m_realloc(nullptr, new_capacity * sizeof(T), m_user));
				if (!ptr)
					return make_stdlib_error(std::errc::not_enough_memory);

				if (clear)
				{
					for (auto k = array_view<T>::size(); k; --k)
					{
						construct(ptr + k - 1, std::move(array_view<T>::m_ptr[k - 1]));
						allocator_type::destroy(array_view<T>::m_ptr + k - 1);
					}
					m_realloc(array_view<T>::m_ptr, 0, m_user);
				}
				array_view<T>::m_ptr = ptr;
                m_capacity = new_capacity;
			}
			return error_type();
		}
		void construct(string* lhs, string&& rhs) noexcept
		{
			ICY_ASSERT(rhs.realloc() == m_realloc && rhs.user() == m_user, "INVALID CUSTOM HEAP CONTAINER");
			allocator_type::construct(lhs, std::move(rhs));
		}
		template<typename U>
		void construct(array<U>* lhs, array<U>&& rhs) noexcept
		{
			ICY_ASSERT(rhs.m_realloc == m_realloc && rhs.m_user == m_user, "INVALID CUSTOM HEAP CONTAINER");
			allocator_type::construct(lhs, std::move(rhs));
		}
		template<typename U>
		void construct(U* lhs, U&& rhs) noexcept
		{
			allocator_type::construct(lhs, std::move(rhs));
		}
		void construct(string* lhs) noexcept
		{
			allocator_type::construct(lhs, string(m_realloc, m_user));
		}
		template<typename U>
		void construct(array<U>* lhs) noexcept
		{
			allocator_type::construct(lhs, array<U>(m_realloc, m_user));
		}
		template<typename U>
		void construct(U* lhs) noexcept
		{
			allocator_type::construct(lhs);
		}
    private:
        size_type m_capacity = 0_z;
		realloc_func m_realloc;
		void* m_user;
	};

	template<typename T>
    error_type copy(const const_array_view<T>& src, array<T>& dst) noexcept
    {
        array<T> tmp;
        ICY_ERROR(tmp.assign(src));
        dst = std::move(tmp);
        return error_type();
    }

    template<typename T>
    error_type copy(const array<T>& src, array<T>& dst) noexcept
    {
        return copy(const_array_view<T>(src), dst);
    }

}