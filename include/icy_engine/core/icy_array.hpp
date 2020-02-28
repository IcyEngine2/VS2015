#pragma once

#include "icy_array_view.hpp"
#include "icy_smart_pointer.hpp"

namespace icy
{
    template<typename T>
	class array : public array_view<T>
	{
	public:
		using allocator_type = allocator_type;
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
		array() noexcept = default;
		array(const array&) = delete;
		array& operator=(const array&) = delete;
		array(array&& rhs) noexcept
		{
			std::swap(m_ptr, rhs.m_ptr);
			std::swap(m_size, rhs.m_size);
            std::swap(m_capacity, rhs.m_capacity);
		}
		ICY_DEFAULT_MOVE_ASSIGN(type);
		~array() noexcept
		{
			for (auto k = m_size; k--;)
				allocator_type::destroy(m_ptr + k);
			allocator_type::deallocate(m_ptr);
		}
	public:
		size_type capacity() const noexcept
		{
			return m_capacity;
		}
		error_type resize(const size_type size) noexcept
		{
			if (size > m_size)
			{
				if (const auto error = reserve(size))
					return error;

				while (m_size != size)
					allocator_type::construct(m_ptr + (m_size++), T{});
			}
			else
			{
				pop_back(m_size - size);
			}
			return {};
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
            ICY_ERROR(reserve(m_size + 1));
            allocator_type::construct(m_ptr + m_size, std::forward<arg_types>(args)...);
			++m_size;
			return {};
		}

		template<typename iter_type>
		error_type append(std::move_iterator<iter_type> first, const std::move_iterator<iter_type> last) noexcept
		{
            ICY_ERROR(reserve(size() + std::distance(first, last)));
			for (; first != last; ++first)
				allocator_type::construct(m_ptr + (m_size++), std::move(*first));
			return {};
		}
        template<typename iter_type>
        error_type append(iter_type first, const iter_type last) noexcept
        {
            ICY_ERROR(reserve(size() + std::distance(first, last)));
            for (; first != last; ++first)
            {
                T tmp;
                ICY_ERROR(copy(*first, tmp));
                allocator_type::construct(m_ptr + (m_size++), std::move(tmp));
            }
            return {};
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
			return {};
		}
		error_type assign(const std::initializer_list<T> list) noexcept
		{
			array tmp;
            ICY_ERROR(tmp.append(list));
			*this = std::move(tmp);
			return {};
		}
		template<typename rhs_type>
		error_type assign(rhs_type&& rhs) noexcept
		{
			array tmp;
            ICY_ERROR(tmp.append(std::forward<rhs_type>(rhs)));
			*this = std::move(tmp);
			return {};
		}

		void clear() noexcept
		{
			pop_back(size());
		}
		void pop_back(const size_type count = 1) noexcept
		{
			ICY_ASSERT(count <= size(), "INVALID POP_BACK COUNT");
			for (auto k = 0_z; k < count; ++k)
				allocator_type::destroy(m_ptr + (--m_size));
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
				const auto ptr = allocator_type::allocate<T>(new_capacity);
				if (!ptr)
					return make_stdlib_error(std::errc::not_enough_memory);

				if (clear)
				{
					for (auto k = size(); k; --k)
					{
						allocator_type::construct(ptr + k - 1, std::move(m_ptr[k - 1]));
						allocator_type::destroy(m_ptr + k - 1);
					}
					allocator_type::deallocate(m_ptr);
				}
				m_ptr = ptr;
                m_capacity = new_capacity;
			}
			return {};
		}
    private:
        size_type m_capacity = 0_z;
	};

    template<typename T>
    error_type copy(const const_array_view<T>& src, array<T>& dst) noexcept
    {
        array<T> tmp;
        ICY_ERROR(tmp.assign(src));
        dst = std::move(tmp);
        return {};
    }

    template<typename T>
    error_type copy(const array<T>& src, array<T>& dst) noexcept
    {
        return copy(const_array_view<T>(src), dst);
    }

}