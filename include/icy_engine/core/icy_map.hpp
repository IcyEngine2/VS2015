#pragma once

#include "icy_array.hpp"
#include "icy_string.hpp"

namespace icy
{
	template<typename K, typename T>
	class map
	{
        friend error_type copy(const map<K, T>& src, map<K, T>& dst) noexcept
        {
            array<K> keys;
            array<T> vals;
            ICY_ERROR(copy(src.m_keys, keys));
            ICY_ERROR(copy(src.m_vals, vals));
            dst.m_keys = std::move(keys);
            dst.m_vals = std::move(vals);
            return error_type();
        }
	public:
		using type = map;
		using key_type = K;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using value_type = T;
        class const_pair_type;
        class pair_type;
		using const_pointer = const_pair_type;
		using pointer = pair_type;
		using const_reference = const_pair_type;
		using reference = pair_type;
        class const_iterator;
        class iterator;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using reverse_iterator = std::reverse_iterator<iterator>;
	public:
		map() noexcept = default;
        const_array_view<K> keys() const noexcept
		{
			return m_keys;
		}
        const_array_view<T> vals() const noexcept
		{
			return m_vals;
		}
        array_view<T> vals() noexcept
		{
			return m_vals;
		}
		bool empty() const noexcept
		{
			return m_keys.empty();
		}
		size_type size() const noexcept
		{
			return m_keys.size();
		}
        size_type capacity() const noexcept
		{
			return m_keys.capacity();
		}
		const T* data() const noexcept
		{
			return m_vals.data();
		}
		T* data() noexcept
		{
			return m_vals.data();
		}
        const_iterator begin() const noexcept
		{
			return{ *this, 0 };
		}
        const_iterator cbegin() const noexcept
		{
			return begin();
		}
        const_iterator end() const noexcept
		{
			return const_iterator{ *this, size() };
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
        const_reverse_iterator rend() const noexcept
		{
			return std::make_reverse_iterator(begin());
		}
        const_reverse_iterator crend() const noexcept
		{
			return rend();
		}
        iterator begin() noexcept
		{
			return{ *this, 0 };
		}
        iterator end() noexcept
		{
			return{ *this, size() };
		}
        reverse_iterator rbegin() noexcept
		{
			return std::make_reverse_iterator(end());
		}
        reverse_iterator rend() noexcept
		{
			return std::make_reverse_iterator(begin());
		}
		const_reference front() const noexcept
		{
			return *begin();
		}
        const_reference back() const noexcept
		{
			return *rbegin();
		}
        reference front() noexcept
		{
			return *begin();
		}
        reference back() noexcept
		{
			return *rbegin();
		}
		iterator find(const key_type& key) noexcept
		{
			const auto it = lower_bound(key);
			if (it != end() && icy::compare(it->key, key) == 0)
				return it;
			return end();
		}
		const_iterator find(const key_type& key) const noexcept
		{
			return const_cast<type*>(this)->find(key);
		}
        const_iterator lower_bound(const key_type& key) const noexcept
		{
			const auto it = std::lower_bound(m_keys.begin(), m_keys.end(), key);
			return { *this, size_type(std::distance(m_keys.begin(), it)) };
		}
        const_iterator upper_bound(const key_type& key) const noexcept
		{
			const auto it = std::upper_bound(m_keys.begin(), m_keys.end(), key);
			return { *this, size_type(std::distance(m_keys.begin(), it)) };
		}
        std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept
		{
			const auto pair = std::equal_range(m_keys.begin(), m_keys.end(), key);
			return std::make_pair(
				const_iterator{ *this, size_type(std::distance(m_keys.begin(), pair.first)) },
				const_iterator{ *this, size_type(std::distance(m_keys.begin(), pair.second)) });
		}
        iterator lower_bound(const key_type& key) noexcept
		{
			const auto it = std::lower_bound(m_keys.begin(), m_keys.end(), key);
			return { *this, size_type(std::distance(m_keys.begin(), it)) };
		}
        iterator upper_bound(const key_type& key) noexcept 
		{
			const auto it = std::upper_bound(m_keys.begin(), m_keys.end(), key);
			return { *this, size_type(std::distance(m_keys.begin(), it)) };
		}
        std::pair<iterator, iterator> equal_range(const key_type& key) noexcept
		{
			const auto pair = std::equal_range(m_keys.begin(), m_keys.end(), key);
			return std::make_pair(
				iterator{ *this, size_type(std::distance(m_keys.begin(), pair.first)) },
				iterator{ *this, size_type(std::distance(m_keys.begin(), pair.second)) });
		}
		const_iterator operator[](const key_type& key) const noexcept
		{
			return find(key);
		}
        iterator operator[](const key_type& key) noexcept
		{
			return find(key);
		}
        template<typename U>
        iterator find(const U& key) noexcept
        {
            const auto it = lower_bound(key);
            if (it != end() && icy::compare(it->key, key) == 0)
                return it;
            return end();
        }
        template<typename U>
        const_iterator find(const U& key) const noexcept
        {
            return const_cast<type*>(this)->find(key);
        }
        template<typename U>
        const_iterator lower_bound(const U& key) const noexcept
        {
            const auto it = std::lower_bound(m_keys.begin(), m_keys.end(), key);
            return { *this, size_type(std::distance(m_keys.begin(), it)) };
        }
        template<typename U>
        const_iterator upper_bound(const U& key) const noexcept
        {
            const auto it = std::upper_bound(m_keys.begin(), m_keys.end(), key);
            return { *this, size_type(std::distance(m_keys.begin(), it)) };
        }
        template<typename U>
        std::pair<const_iterator, const_iterator> equal_range(const U& key) const noexcept
        {
            const auto pair = std::equal_range(m_keys.begin(), m_keys.end(), key);
            return std::make_pair(
                const_iterator{ *this, size_type(std::distance(m_keys.begin(), pair.first)) },
                const_iterator{ *this, size_type(std::distance(m_keys.begin(), pair.second)) });
        }
        template<typename U>
        iterator lower_bound(const U& key) noexcept
        {
            const auto it = std::lower_bound(m_keys.begin(), m_keys.end(), key);
            return { *this, size_type(std::distance(m_keys.begin(), it)) };
        }
        template<typename U>
        iterator upper_bound(const U& key) noexcept
        {
            const auto it = std::upper_bound(m_keys.begin(), m_keys.end(), key);
            return { *this, size_type(std::distance(m_keys.begin(), it)) };
        }
        template<typename U>
        std::pair<iterator, iterator> equal_range(const U& key) noexcept
        {
            const auto pair = std::equal_range(m_keys.begin(), m_keys.end(), key);
            return std::make_pair(
                iterator{ *this, size_type(std::distance(m_keys.begin(), pair.first)) },
                iterator{ *this, size_type(std::distance(m_keys.begin(), pair.second)) });
        }
        template<typename U>
        const_iterator operator[](const U& key) const noexcept
        {
            return find(key);
        }
        template<typename U>
        iterator operator[](const U& key) noexcept
        {
            return find(key);
        }
        template<typename U>
        const value_type* try_find(const U& key) const noexcept
        {
            const auto it = find(key);
            if (it != cend())
                return &it->value;
            return nullptr;
        }
	public:
		error_type reserve(const size_type capacity) noexcept
		{
			const auto errorV = m_vals.reserve(capacity);
			const auto errorK = m_keys.reserve(capacity);
			if (errorV) return errorV;
			if (errorK) return errorK;
			return error_type();
		}
		iterator erase(const const_iterator first, const const_iterator last) noexcept
		{
			const auto sz = size();
			const auto p0 = size_type(std::distance(cbegin(), first));
			const auto p1 = size_type(std::distance(cbegin(), last));
			for (auto k = 0u; k < sz - p1; ++k)
				m_keys[p0 + k] = std::move(m_keys[p1 + k]);
			for (auto k = 0u; k < sz - p1; ++k)
				m_vals[p0 + k] = std::move(m_vals[p1 + k]);
			m_vals.pop_back(p1 - p0);
			m_keys.pop_back(p1 - p0);
			return begin() + p0;
		}
        iterator erase(const_iterator it) noexcept
		{
			const auto first = it;
			const auto last = ++it;
			return erase(first, last);
		}
		void clear() noexcept
		{
			m_vals.clear();
			m_keys.clear();
		}
        error_type insert(key_type&& new_key, value_type&& new_val, iterator* it = nullptr) noexcept;
        error_type insert(const key_type& new_key, value_type&& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<key_type>::value, "KEY MUST BE TRIVIAL");
            auto key = new_key;
            return insert(std::move(key), std::move(new_val), it);
        }
        error_type insert(key_type&& new_key, const value_type& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<value_type>::value, "VALUE MUST BE TRIVIAL");
            auto val = new_val;
            return insert(std::move(new_key), std::move(val), it);
        }
        error_type insert(const key_type& new_key, const value_type& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<key_type>::value, "KEY MUST BE TRIVIAL");
            static_assert(std::is_trivially_destructible<value_type>::value, "VALUE MUST BE TRIVIAL");
            auto key = new_key;
            auto val = new_val;
            return insert(std::move(key), std::move(val), it);
        }
		error_type find_or_insert(key_type&& key, value_type&& val, iterator* it = nullptr) noexcept
		{
			auto jt = find(key);
			if (jt == end())
                return insert(std::move(key), std::move(val), it);
			if (it)
				*it = jt;
            jt->value = std::move(val);
            return error_type();
		}
        error_type find_or_insert(const key_type& new_key, value_type&& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<key_type>::value, "KEY MUST BE TRIVIAL");
            key_type tmp_key = new_key;
            return find_or_insert(std::move(tmp_key), std::move(new_val), it);
        }
        error_type find_or_insert(key_type&& new_key, const value_type& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<value_type>::value, "VALUE MUST BE TRIVIAL");
            value_type tmp_val = new_val;
            return find_or_insert(std::move(new_key), std::move(tmp_val), it);
        }
        error_type find_or_insert(const key_type& new_key, const value_type& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<key_type>::value, "KEY MUST BE TRIVIAL");
            static_assert(std::is_trivially_destructible<value_type>::value, "VALUE MUST BE TRIVIAL");
            key_type tmp_key = new_key;
            value_type tmp_val = new_val;
            return find_or_insert(std::move(tmp_key), std::move(tmp_val), it);
        }
	private:
		array<T> m_vals;
		array<K> m_keys;
	};

    
    template<typename K, typename T>
    class map<K, T>::const_pair_type
    {
    public:
        rel_ops(const_pair_type);
        const_pair_type(const value_type& value, const key_type& key) noexcept :
            value{ value }, key{ key }
        {

        }
        const const_pair_type* operator->() const noexcept
        {
            return this;
        }
    public:
        const value_type& value;
        const key_type& key;
    };
    template<typename K, typename T>
    class map<K, T>::pair_type : public map<K, T>::const_pair_type
    {
    public:
        pair_type(value_type& value, const key_type& key) noexcept :
            const_pair_type{ value, key }, value{ value }
        {

        }
        pair_type* operator->() noexcept
        {
            return this;
        }
    public:
        value_type& value;
    };

    template<typename K, typename T>
    int compare(const typename map<K, T>::const_pair_type& lhs, const typename map<K, T>::const_pair_type& rhs) noexcept
    {
        return compare(lhs.key, rhs.key);
    }


    template<typename K, typename T>
    class map<K, T>::const_iterator
    {
        friend int compare(const typename map<K, T>::const_iterator& lhs, const typename map<K, T>::const_iterator& rhs) noexcept
        {
            return compare(lhs.index(), rhs.index());
        }
    public:
        using type = const_iterator;
        using iterator_category = std::random_access_iterator_tag;
        using key_type = typename map::key_type;
        using size_type = typename map::size_type;
        using difference_type = typename map::difference_type;
        using value_type = typename map::value_type;
        using pointer = typename map::const_pointer;
        using reference = typename map::const_reference;
    public:
        rel_ops(const_iterator);
        const_iterator(const map& m, const size_type index) noexcept :
            m_map{ const_cast<map&>(m) }, m_index{ index }
        {

        }
        const_iterator(const const_iterator& rhs) noexcept : m_map{ rhs.m_map }, m_index{ rhs.m_index }
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(const_iterator);
    public:
        type& operator+=(const difference_type offset) noexcept
        {
            m_index += offset;
            ICY_ASSERT(m_index <= m_map.size(), "MAP ITERATOR IS OUT OF RANGE");
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
        type& operator--() noexcept
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
        difference_type operator-(const type& rhs) const noexcept
        {
            return difference_type(m_index - rhs.m_index);
        }
    public:
        reference operator*() const noexcept
        {
            return{ m_map.m_vals[m_index], m_map.m_keys[m_index] };
        }
        reference operator[](const difference_type offset) const noexcept
        {
            return *(*this + offset);
        }
        pointer operator->() const noexcept
        {
            return pointer(**this);
        }
        size_type index() const noexcept
        {
            return m_index;
        }
    protected:
        map& m_map;
        size_type m_index;
    };

    template<typename K, typename T>
    class map<K, T>::iterator : public map<K, T>::const_iterator
    {
    public:
        using base = typename map<K, T>::const_iterator;
        using type = iterator;
        using reference = typename map::reference;
        using pointer = typename map::pointer;
        using base::const_iterator;
        using base::operator-;
    public:
        iterator() noexcept = delete;
        type& operator+=(const difference_type offset) noexcept
        {
            return static_cast<iterator&>(base::operator+=(offset));
        }
        type operator+(const difference_type offset) const noexcept
        {
            return static_cast<const iterator&>(base::operator+(offset));
        }
        type& operator++() noexcept
        {
            return static_cast<iterator&>(base::operator++());
        }
        type operator++(int) noexcept
        {
            return static_cast<const iterator&>(base::operator++(0));
        }
        type& operator-=(const difference_type offset) noexcept
        {
            return static_cast<iterator&>(base::operator-=(offset));
        }
        type operator-(const difference_type offset) const noexcept
        {
            return static_cast<const iterator&>(base::operator-(offset));
        }
        type& operator--() noexcept
        {
            return static_cast<iterator&>(base::operator--());
        }
        type& operator--(int) noexcept
        {
            return static_cast<type&>(base::operator--(0));
        }
    public:
        reference operator*() const noexcept
        {
            return reference{ base::m_map.m_vals[base::m_index], base::m_map.m_keys[base::m_index] };
        }
        reference operator[](const difference_type offset) const noexcept
        {
            return *(*this + offset);
        }
        pointer operator->() const noexcept
        {
            return pointer(**this);
        }
    };

    template<typename K, typename T>
    error_type map<K, T>::insert(typename map<K, T>::key_type&& new_key, typename map<K, T>::value_type&& new_val, typename map<K, T>::iterator* it) noexcept
    {
        auto jt = lower_bound(new_key);
        if (jt.index() != size())
        {
            if (icy::compare(jt->key, new_key) == 0)
                return make_stdlib_error(std::errc::invalid_argument);
        }

        const auto index = size_type(std::distance(begin(), jt));
        const auto size = m_keys.size();

        if (size < m_keys.capacity())
        {
            const auto errorK = m_keys.emplace_back();
            const auto errorV = m_vals.emplace_back();
            if (errorK || errorV)
            {
                m_keys.resize(size);
                m_vals.resize(size);
                if (errorK) return errorK;
                if (errorV) return errorV;
            }
            for (auto k = size; k > index; --k)
                m_keys[k] = std::move(m_keys[k - 1]);
            for (auto k = size; k > index; --k)
                m_vals[k] = std::move(m_vals[k - 1]);

            m_keys[index] = std::move(new_key);
            m_vals[index] = std::move(new_val);
        }
        else // new arrays
        {
            decltype(m_keys) new_keys;
            decltype(m_vals) new_vals;
            ICY_ERROR(new_keys.reserve(size + 1));
            ICY_ERROR(new_vals.reserve(size + 1));

            const auto key_bot = std::make_move_iterator(m_keys.begin());
            const auto key_mid = std::make_move_iterator(m_keys.begin() + ptrdiff_t(index));
            const auto key_top = std::make_move_iterator(m_keys.end());

            const auto val_bot = std::make_move_iterator(m_vals.begin());
            const auto val_mid = std::make_move_iterator(m_vals.begin() + ptrdiff_t(index));
            const auto val_top = std::make_move_iterator(m_vals.end());

            ICY_ERROR(new_keys.append(key_bot, key_mid));
            ICY_ERROR(new_keys.push_back(std::move(new_key)));
            ICY_ERROR(new_keys.append(key_mid, key_top));

            ICY_ERROR(new_vals.append(val_bot, val_mid));
            ICY_ERROR(new_vals.push_back(std::move(new_val)));
            ICY_ERROR(new_vals.append(val_mid, val_top));

            m_keys = std::move(new_keys);
            m_vals = std::move(new_vals);
        }
        if (it)
            *it = iterator{ *this, index };
        return error_type();
    }

    template<typename T>
    using dictionary = map<string, T>;
    /*
    class dictionary : public map<string, T>
    {
    public:
        using type = dictionary;
        using base = map<string, T>;
        using typename base::key_type;
        using typename base::size_type;
        using typename base::difference_type;
        using typename base::value_type;
        using typename base::const_pair_type;
        using typename base::pair_type;
        using typename base::const_pointer;
        using typename base::pointer;
        using typename base::const_reference;
        using typename base::reference;
        using typename base::const_iterator;
        using typename base::iterator;
        using typename base::const_reverse_iterator;
        using typename base::reverse_iterator;
    public:
        template<typename U>
        error_type insert(const U& new_key, T&& new_val, iterator* it = nullptr) noexcept
        {
            string key;
            ICY_ERROR(to_string(new_key, key));
            return base::insert(std::move(key), std::move(new_val), it);
        }
        template<typename U>
        error_type insert(const U& new_key, const T& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<T>::value, "VALUE TYPE MUST BE TRIVIAL");
            return insert(new_key, T(new_val), it);
        }
        template<typename U>
        error_type find_or_insert(const U& new_key, T&& new_val, iterator* it = nullptr) noexcept
        {
            string key;
            ICY_ERROR(to_string(new_key, key));
            return base::find_or_insert(std::move(key), std::move(new_val), it);
        }
        template<typename U>
        error_type find_or_insert(const U& new_key, const T& new_val, iterator* it = nullptr) noexcept
        {
            static_assert(std::is_trivially_destructible<T>::value, "VALUE TYPE MUST BE TRIVIAL");
            return find_or_insert(new_key, T(new_val), it);
        }
    };

    template<typename T>
    error_type copy(const dictionary<T>& src, dictionary<T>& dst)
    {
        return copy(static_cast<const map<string, T>&>(src), )
    }*/
	//error_type emplace(map<string, string>& map, const string_view key, const string_view val) noexcept;
}