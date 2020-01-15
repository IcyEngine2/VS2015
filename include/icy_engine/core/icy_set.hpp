#pragma once

#include "icy_array.hpp"

namespace icy
{
    template<typename T>
    class set : public array<T>
    {
    public:
        using type = set;
        using base = array<T>;
        using typename base::size_type;
        using typename base::difference_type;
        using typename base::value_type;
        using typename base::const_pointer;
        using typename base::pointer;
        using typename base::const_reference;
        using typename base::reference;
        using typename base::const_iterator;
        using typename base::iterator;
        using typename base::const_reverse_iterator;
        using typename base::reverse_iterator;
    public:
        const T* data() const noexcept
        {
            return base::data();
        }
        using base::cbegin;
        using base::cend;
        using base::crbegin;
        using base::crend;
        const_iterator begin() const noexcept
        {
            return base::begin();
        }
        const_iterator end() const noexcept
        {
            return base::end();
        }
        const_reverse_iterator rbegin() const noexcept
        {
            return base::rbegin();
        }
        const_reverse_iterator rend() const noexcept
        {
            return base::rend();
        }
        const_reference front() const noexcept
        {
            return base::front();
        }
        const_reference back() const noexcept
        {
            return base::back();
        }
        const_iterator find(const T& key) const noexcept
        {
            return icy::binary_search(begin(), end(), key, pred);
        }
        const_iterator lower_bound(const T& key) const noexcept
        {
            return std::lower_bound(begin(), end(), key, pred);
        }
        const_iterator upper_bound(const T& key) const noexcept
        {
            return std::upper_bound(begin(), end(), key, pred);
        }
        const_pointer try_find(const T& key) const noexcept
        {
            const auto it = find(key);
            if (it != cend())
                return &it->value;
            return nullptr;
        }
        const_iterator erase(const const_iterator it) noexcept
        {
            if (it != end())
            {
                auto offset = std::distance(base::begin(), it);
                for (auto k = offset + 1; k < size(); ++k)
                    base::data()[k - 1] = std::move(base::data[k]);
                pop_back();
            }
            return it;
        }
        error_type erase(const T& key, iterator* it = nullptr) noexcept
        {
            auto search = find(key);
            if (search != end())
            {
                search = erase(search);
                if (it) *it = search;
                return {};
            }
            return make_stdlib_error(std::errc::invalid_argument);
        }
        error_type insert(value_type&& key, const_iterator* it = nullptr) noexcept
        {
            const auto search = lower_bound(key);
            if (search != end())
            {
                if (icy::compare(key, *search) == 0)
                    return make_stdlib_error(std::errc::invalid_argument);
                
                const auto offset = std::distance(begin(), search);
                const auto length = size();
                ICY_ERROR(base::emplace_back());
                for (auto k = length; k != offset; --k)
                    base::data()[k] = std::move(base::data()[k - 1]);
                base::data()[offset] = std::move(key);
            }
            else
            {
                ICY_ERROR(base::push_back(std::move(key)));
            }
            return {};
        }
        error_type insert(const value_type& key, const_iterator* it = nullptr) noexcept
        {
            return insert(value_type(key), it);
        }
        template<typename U>
        error_type assign(U&& rhs) noexcept
        {
            ICY_ERROR(base::assign(std::forward<U>(rhs)));
            std::sort(base::begin(), base::end(), pred);
            return {};
        }
        template<typename iter_type>
        error_type assign(const iter_type first, const iter_type last) noexcept
        {
            ICY_ERROR(base::assign(first, last));
            std::sort(base::begin(), base::end(), pred);
            return {};
        }
    private:
        static bool pred(const value_type& lhs, const value_type& rhs) noexcept
        {
            return compare(lhs, rhs) < 0;
        }
        using base::push_back;
        using base::emplace_back;
    };

}