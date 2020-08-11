#pragma once

#include "icy_core.hpp"

namespace icy
{
#if _DEBUG
	template<typename T> class const_array_iterator;
    template<typename T> class const_array_iterator
	{
    	static_assert(!std::is_const<T>::value, "REMOVE CONST/VOLATILE QUALIFIERS FROM TYPE");
	public:
		using type = const_array_iterator;
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using pointer = const value_type*;
		using reference = const value_type &;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
	public:
        rel_ops(const_array_iterator);
		const_array_iterator() noexcept : m_ptr{ nullptr }, m_begin{ nullptr }, m_end{ nullptr }
		{

		}
		const_array_iterator(const pointer begin, const pointer end, const pointer ptr) noexcept :
			m_ptr{ ptr }, m_begin{ begin }, m_end{ end }
		{

		}
	public:
		type& operator+=(const difference_type offset) noexcept
		{
			m_ptr += offset;
			ICY_ASSERT(m_ptr >= m_begin && m_ptr <= m_end,
				"ARRAY ITERATOR IS OUT OF RANGE");
			return *this;
		}
		type operator+(const difference_type offset) const noexcept
		{
			type tmp = *this;
			tmp += offset;
			return tmp;
		}
		type& operator++() noexcept
		{
			return *this += 1;
		}
		type operator++(int) noexcept
		{
			const type tmp = *this;
			++* this;
			return tmp;
		}
		type& operator-=(const difference_type offset) noexcept
		{
			return *this += (-offset);
		}
		type operator-(const difference_type offset) const noexcept
		{
			return *this + (-offset);
		}
		type & operator--() noexcept
		{
			return *this += -1;
		}
		type operator--(int) noexcept
		{
			const type tmp = *this;
			--* this;
			return tmp;
		}
	public:
		difference_type operator-(const type & rhs) const noexcept
		{
			return m_ptr - rhs.m_ptr;
		}
	public:
		reference operator*() const noexcept
		{
			ICY_ASSERT(m_ptr < m_end && m_ptr >= m_begin,
				"ARRAY ITERATOR IS OUT OF RANGE");
			return *m_ptr;
		}
		reference operator[](const difference_type offset) const noexcept
		{
			return *(*this + offset);
		}
		pointer operator->() const noexcept
		{
			return &(**this);
		}
	protected:
		pointer m_ptr;
		pointer m_begin;
		pointer m_end;
	};

    template<typename T> inline int compare(const const_array_iterator<T>& lhs, const const_array_iterator<T>& rhs) noexcept
    {
        return static_cast<int>(lhs - rhs);
    }
	template<typename T> class array_iterator : public const_array_iterator<T>
	{
	public:
		using base = const_array_iterator<T>;
		using type = array_iterator<T>;
		using base::value_type;
		using base::difference_type;
		using reference = value_type &;
		using pointer = value_type *;
		using base::const_array_iterator;
		using base::operator-;
	public:
		type& operator+=(const difference_type offset) noexcept
		{
			return static_cast<type&>(base::operator+=(offset));
		}
		type operator+(const difference_type offset) const noexcept
		{
			return static_cast<const type&>(base::operator+(offset));
		}
		type& operator++() noexcept
		{
			return static_cast<type&>(base::operator++());
		}
		type operator++(int) noexcept
		{
			return static_cast<const type&>(base::operator++(0));
		}
		type& operator-=(const difference_type offset) noexcept
		{
			return static_cast<type&>(base::operator-=(offset));
		}
		type operator-(const difference_type offset) const noexcept
		{
			return static_cast<const type&>(base::operator-(offset));
		}
		type& operator--() noexcept
		{
			return static_cast<type&>(base::operator--());
		}
		type operator--(int) noexcept
		{
			return static_cast<const type&>(base::operator--(0));
		}
	public:
		reference operator*() const noexcept
		{
			return const_cast<reference>(base::operator*());
		}
		reference operator[](const difference_type offset) const noexcept
		{
			return const_cast<reference>(base::operator[](offset));
		}
		pointer operator->() const noexcept
		{
			return const_cast<pointer>(base::operator->());
		}
	};
	template<typename array_type>
	const_array_iterator<typename array_type::value_type> make_array_begin(const array_type & array) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) };
	}
	template<typename array_type>
	const_array_iterator<typename array_type::value_type> make_array_end(const array_type & array) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) + std::size(array) };
	}
	template<typename array_type>
	array_iterator<typename array_type::value_type> make_array_begin(array_type & array) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) };
	}
	template<typename array_type>
	array_iterator<typename array_type::value_type> make_array_end(array_type & array) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) + std::size(array) };
	}
	template<typename T, size_t N>
	const_array_iterator<T> make_array_begin(const T(&array)[N]) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) };
	}
	template<typename T, size_t N>
	const_array_iterator<T> make_array_end(const T(&array)[N]) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) + std::size(array) };
	}
	template<typename T, size_t N>
	array_iterator<T> make_array_begin(T(&array)[N]) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) };
	}
	template<typename T, size_t N>
	array_iterator<T> make_array_end(T(&array)[N]) noexcept
	{
		return{ std::data(array), std::data(array) + std::size(array), std::data(array) + std::size(array) };
	}
#else
	template<typename T> using const_array_iterator = const T*;
	template<typename T> using array_iterator = T *;
	template<typename array_type>
	const_array_iterator<typename array_type::value_type> make_array_begin(const array_type & array) noexcept
	{
		return{ std::data(array) };
	}
	template<typename array_type>
	const_array_iterator<typename array_type::value_type> make_array_end(const array_type & array) noexcept
	{
		return{ std::data(array) + std::size(array) };
	}
	template<typename array_type>
	array_iterator<typename array_type::value_type> make_array_begin(array_type & array) noexcept
	{
		return{ std::data(array) };
	}
	template<typename array_type>
	array_iterator<typename array_type::value_type> make_array_end(array_type & array) noexcept
	{
		return{ std::data(array) + std::size(array) };
	}
	template<typename T, size_t N>
	const_array_iterator<T> make_array_begin(const T(&array)[N]) noexcept
	{
		return{ std::data(array) };
	}
	template<typename T, size_t N>
	const_array_iterator<T> make_array_end(const T(&array)[N]) noexcept
	{
		return{ std::data(array) + std::size(array) };
	}
	template<typename T, size_t N>
	array_iterator<T> make_array_begin(T(&array)[N]) noexcept
	{
		return{ std::data(array) };
	}
	template<typename T, size_t N>
	array_iterator<T> make_array_end(T(&array)[N]) noexcept
	{
		return{ std::data(array) + std::size(array) };
	}
#endif

	template<typename T> class const_array_view
	{
	public:
		using type = const_array_view;
		using value_type = remove_cvr<T>;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type &;
		using const_reference = const value_type &;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using const_iterator = const_array_iterator<value_type>;
		using iterator = array_iterator<value_type>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using reverse_iterator = std::reverse_iterator<iterator>;
	public:
        rel_ops(const_array_view);
		constexpr const_array_view() noexcept : m_ptr{ }, m_size{ }
		{

		}
		constexpr const_array_view(const const_pointer ptr, const size_type size) noexcept :
			m_ptr{ const_cast<pointer>(ptr) }, m_size{ size }
		{

		}
		const_array_view(const const_pointer begin, const const_pointer end) noexcept :
			m_ptr{ const_cast<pointer>(begin) }, m_size{ size_type(end - begin) }
		{
			ICY_ASSERT(begin <= end, "INVALID ITERATOR RANGE (BEGIN > END)");
		}
		template<typename container_type, typename = std::enable_if_t<!std::is_same<container_type, std::initializer_list<T>>::value>>
		const_array_view(const container_type& container) noexcept :
			const_array_view(std::data(container), std::size(container))
		{
			static_assert(!std::is_same<container_type, std::initializer_list<T>>::value, "INVALID CONTAINER");
		}
		const_array_view(const std::initializer_list<T> list) noexcept :
			m_ptr{ const_cast<T*>(list.begin()) }, m_size{ list.size() }
		{

		}
	public:
		constexpr bool empty() const noexcept
		{
			return !m_size;
		}
		constexpr size_type size() const noexcept
		{
			return m_size;
		}
		constexpr const_pointer data() const noexcept
		{
			return m_ptr;
		}
        const_reference at(const size_type index) const noexcept
		{
			ICY_ASSERT(index < m_size, "INDEX IS OUT OF RANGE");
			return m_ptr[index];
		}
        const_reference operator[](const size_type index) const noexcept
        {
			return at(index);
		}
		const_reference front() const noexcept
		{
			return at(0);
		}
        const_reference back() const noexcept
		{
			return at(m_size - 1);
		}
		const_iterator begin() const noexcept
		{
			return make_array_begin(*this);
		}
        const_iterator cbegin() const noexcept
		{
			return begin();
		}
        const_iterator end() const noexcept
		{
			return make_array_end(*this);
		}
        const_iterator cend() const noexcept
		{
			return end();
		}
        const_reverse_iterator rbegin() const noexcept
		{
			return std::make_reverse_iterator(end());
		}
        const_reverse_iterator crbegin() const noexcept
		{
			return rbegin();
		}
        reverse_iterator rend() const noexcept
		{
			return std::make_reverse_iterator(begin());
		}
        reverse_iterator crend() const noexcept
		{
			return rend();
		}
	protected:
		pointer m_ptr;
		size_type m_size;
	};

	template<typename T> class array_view : public const_array_view<T>
	{
	public:
		using type = array_view<T>;
		using base = const_array_view<T>;
		using value_type = typename base::value_type;
		using pointer = typename base::pointer;
		using const_pointer = typename base::const_pointer;
		using reference = typename  base::reference;
		using const_reference =typename  base::const_reference;
		using size_type =typename  base::size_type;
		using difference_type =typename  base::difference_type;
		using const_iterator =typename  base::const_iterator;
		using iterator = typename base::iterator;
		using const_reverse_iterator = typename base::const_reverse_iterator;
		using reverse_iterator = typename base::reverse_iterator;
	public:
		array_view() noexcept = default;
		array_view(const pointer ptr, const size_type size) noexcept : base{ ptr, size }
		{
			
		}
		array_view(const pointer begin, const pointer end) noexcept : base{ begin, end }
		{

		}
		template<typename container_type, typename = std::enable_if_t<!std::is_const<container_type>::value>>
		array_view(container_type& container) noexcept : array_view{ std::data(container), std::size(container) }
		{

		}
		array_view(const std::initializer_list<T> list) noexcept : array_view{ list.begin(), list.end() }
		{

		}
	public:
		using base::data;
		using base::at;
		using base::operator[];
		using base::front;
		using base::back;
		using base::begin;
		using base::end;
		using base::rbegin;
		using base::rend;
		using base::size;
        pointer data() noexcept
		{
			return const_cast<pointer>(base::data());
		}
        reference at(const size_type index) noexcept
		{
			return const_cast<reference>(base::at(index));
		}
        reference operator[](const size_type index) noexcept
		{
			return const_cast<reference>(base::operator[](index));
		}
        reference front() noexcept
		{
			return const_cast<reference>(base::front());
		}
        reference back() noexcept
		{
			return const_cast<reference>(base::back());
		}
		iterator begin() noexcept
		{
			return make_array_begin(*this);
		}
        iterator end() noexcept
		{
			return make_array_end(*this);
		}
		reverse_iterator rbegin() noexcept
		{
			return std::make_reverse_iterator(end());
		}
        reverse_iterator rend() noexcept
		{
			return std::make_reverse_iterator(begin());
		}
	public:
		template<typename iter_type>
		type& assign(iter_type first, const iter_type last) noexcept
		{
			const auto dst = size_type(std::distance(first, last));
			ICY_ASSERT(dst == size(), "TRYING TO ASSIGN INVALID ITERATOR RANGE (SIZE MISMATCH)");
            for (auto k = 0_z; k < size(); ++k, ++first)
            {
                copy;
                data()[k] = T{ *first };
            }
            return *this;
		}
		type& assign(std::initializer_list<T> list) noexcept
		{
			return assign(std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()));
		}
		template<typename rhs_type>
		type& assign(rhs_type&& rhs) noexcept
		{
			assign(
				std::make_move_iterator(std::begin(rhs)),
				std::make_move_iterator(std::end(rhs)));
			clear(rhs);
			return *this;
		}
		template<typename rhs_type>
		type& operator=(rhs_type && rhs) noexcept
		{
			return assign(std::forward<rhs_type>(rhs));
		}
		type& operator=(std::initializer_list<T> list) noexcept
		{
			return assign(list);
		}
	};

	template<typename iterator_type, typename T>
	iterator_type binary_search(const iterator_type first, const iterator_type last, const T& value) noexcept
	{
		auto it = std::lower_bound(first, last, value);
		if (it != last && *it == value)
			return it;
		return last;
	}
    template<typename iterator_type, typename T, typename pred_type>
    iterator_type binary_search(const iterator_type first, const iterator_type last, const T& value, const pred_type& pred) noexcept
    {
        auto it = std::lower_bound(first, last, value, pred);
        if (it != last && *it == value)
            return it;
        return last;
    }
}