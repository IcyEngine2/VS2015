#pragma once

#include "icy_array.hpp"

namespace icy
{
    template<typename T> class const_matrix_view;
    template<typename T> class const_matrix_col;
    template<typename T> class matrix_view;
    template<typename T> class matrix_col;
    
    template<typename T> class const_matrix_iterator
    {
        friend const_matrix_view<T>;
        static const_matrix_iterator begin(const const_matrix_view<T>&);
        static const_matrix_iterator end(const const_matrix_view<T>&);
    };
    template<typename T> class matrix_iterator : public const_matrix_iterator<T>
    {
        friend matrix_view<T>;
        static matrix_iterator begin(const matrix_view<T>&);
        static matrix_iterator end(const matrix_view<T>&);
    };

    template<typename T> class const_matrix_view
    {
    public:
        using type = const_matrix_view;
        using value_type = remove_cvr<T>;
        using pointer = const value_type*;
        using const_pointer = const value_type*;
        using reference = const value_type&;
        using const_reference = const value_type&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using const_view_type = const_matrix_view<T>;
        using const_row_type = const_array_view<T>;
        using const_col_type = const_matrix_col<T>;
        using view_type = const_matrix_view<T>;
        using row_type = const_array_view<T>;
        using col_type = const_matrix_col<T>;
        using const_iterator = const_matrix_iterator<T>;
        using iterator = const_matrix_iterator<T>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;
    public:
        const_matrix_view() noexcept = default;
        const_matrix_view(const const_pointer data, const size_type pitch,
            const size_type rows, const size_type cols, 
            const size_type row,  const size_type col) noexcept : 
            m_data{ const_cast<pointer>(data) }, m_pitch{ pitch },
            m_rows{ rows }, m_cols{ cols },
            m_row{ row }, m_col{ col }
        {

        }
        template<size_t N, size_t M>
        const_matrix_view(const T(&array)[N][M]) noexcept : m_data{ const_cast<pointer>(&**array) }, m_pitch{ M },
            m_rows{ N }, m_cols{ M }, m_row{ 0 }, m_col{ 0 }
        {

        }
        const_matrix_view(const type& rhs) noexcept : m_data{ rhs.m_data }, m_pitch{ rhs.m_pitch },
            m_rows{ rhs.m_rows }, m_cols{ rhs.m_cols }, m_row{ rhs.m_row }, m_col{ rhs.m_col }
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(const_matrix_view);
    public:
        bool empty() const noexcept
        {
            return !size();
        }
        size_type rows() const noexcept
        {
            return m_rows;
        }
        size_type cols() const noexcept
        {
            return m_cols;
        }
        size_type size() const noexcept
        {
            return m_rows * m_cols;
        }
        size_type pitch() const noexcept
        {
            return m_pitch;
        }
        const_view_type view(const size_type row, const size_type col, const size_type rows, const size_type cols) const noexcept
        {
            return{ m_data, m_pitch, rows, cols, row, col };
        }
        const_view_type cview(const size_type row, const size_type col, const size_type rows, const size_type cols) const noexcept
        {
            return view(row, col, rows, cols);
        }        
        const_col_type col(const size_type index) const noexcept;
        const_col_type ccol(const size_type index) const noexcept
        {
            return col(index);
        }
        const_row_type row(const size_type index) const noexcept
        {
            ICY_ASSERT(index < rows(), "INVALID MATRIX ROW");
            return{ m_data + (m_row + index) * m_pitch + m_col, m_cols };
        }
        const_row_type crow(const size_type index) const noexcept
        {
            return row(index);
        }
        const_reference at(const size_type row, const size_type col) const noexcept
        {
            ICY_ASSERT(row < rows() && col < cols(), "INDEX IS OUT OF RANGE");
            return m_data[(m_row + row) * m_pitch + m_col + col];
        }
        const_row_type operator[](const size_type index) const noexcept
        {
            return row(index);
        }
        const_reference front() const noexcept
        {
            return at(0, 0);
        }
        const_reference back() const noexcept
        {
            return at(m_rows - 1, m_cols - 1);
        }
        const_iterator begin() const noexcept
        {
            return const_iterator::begin(*this);
        }
        const_iterator cbegin() const noexcept
        {
            return begin();
        }
        const_iterator end() const noexcept
        {
            return const_iterator::end(*this);
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
    protected:
        const pointer m_data = nullptr;
        const size_type m_pitch = 0;
        const size_type m_rows = 0;
        const size_type m_cols = 0;
        const size_type m_row = 0;
        const size_type m_col = 0;
    };
    template<typename T> class const_matrix_col : public const_matrix_view<T>
    {
    public:
        using base = const_matrix_view<T>;
        using const_reference = const remove_cvr<T>&;
        using size_type = size_t;
        const_reference operator[](const size_type index) const noexcept
        {
            return at(index);
        }
        const_reference at(const size_type index) const noexcept
        {
            return base::at(index, 0u);
        }
    };
    template<typename T> class matrix_view : public const_matrix_view<T>
    {
    public:
        using type = matrix_view;
        using base = const_matrix_view<T>;
        using value_type = remove_cvr<T>;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using const_view_type = const_matrix_view<T>;
        using const_row_type = const_array_view<T>;
        using const_col_type = const_matrix_col<T>;
        using view_type = matrix_view<T>;
        using row_type = array_view<T>;
        using col_type = matrix_col<T>;
        using const_iterator = const_matrix_iterator<T>;
        using iterator = matrix_iterator<T>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;
    public:
        matrix_view() noexcept = default;
        matrix_view(const pointer data, const size_type pitch,
            const size_type rows, const size_type cols,
            const size_type row, const size_type col) noexcept : 
            base(data, pitch, rows, cols, row, col)
        {

        }
        template<size_t N, size_t M>
        matrix_view(T(&array)[N][M]) noexcept : type{ &**array, M, N, M, 0, 0 }
        {

        }
    public:
        using base::view;
        using base::col;
        using base::row;
        using base::at;
        using base::operator[];
        using base::front;
        using base::back;
        using base::begin;
        using base::end;
        using base::rbegin;
        using base::rend;
    public:
        view_type view(const size_type row, const size_type col, const size_type rows, const size_type cols) const noexcept
        {
            return reinterpret_cast<const view_type&>(base::view(row, col, rows, cols));
        }
        col_type col(const size_type index) noexcept;
        row_type row(const size_type index) noexcept
        {
            const auto row = base::row(index);
            return reinterpret_cast<const row_type&>(row);
        }
        reference at(const size_type row, const size_type col) noexcept
        {
            return const_cast<reference>(base::at(row, col));
        }
        row_type operator[](const size_type index) noexcept
        {
            return row(index);
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
            return iterator::begin(*this);
        }
        iterator end() noexcept
        {
            return iterator::end(*this);
        }
        reverse_iterator rbegin() noexcept
        {
            return std::make_reverse_iterator(end());
        }
        reverse_iterator rend() noexcept
        {
            return std::make_reverse_iterator(begin());
        }
    };
    template<typename T> class matrix_col : public matrix_view<T>
    {
    public:
        using base = matrix_view<T>;
        using reference = remove_cvr<T>&;
        using size_type = typename base::size_type;
    public:
        reference operator[](const size_type index) const noexcept
        {
            return at(index);
        }
        reference at(const size_type index) const noexcept
        {
            return base::at(index, 0);
        }
    };
    template<typename T>
    class matrix : public matrix_view<T>
    {
    public:
        using type = matrix;
        using base = matrix_view<T>; 
        using value_type = remove_cvr<T>;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using const_view_type = const_matrix_view<T>;
        using const_row_type = const_array_view<T>;
        using const_col_type = const_matrix_col<T>;
        using view_type = matrix_view<T>;
        using row_type = array_view<T>;
        using col_type = matrix_col<T>;
        using const_iterator = const_matrix_iterator<T>;
        using iterator = matrix_iterator<T>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;
    public:
        matrix() noexcept = default;
        matrix(const size_type rows, const size_type cols) :
            base([&]
        {
            pointer ptr = nullptr;
            if (rows && cols)
            {
                ptr = allocator_type::allocate<T>(rows * cols);
                auto k = 0_z;
                ICY_SCOPE_FAIL
                {
                    while (k) allocator_type::destroy(ptr + (--k));
                    allocator_type::deallocate(ptr);
                };
                for (; k < rows * cols; ++k)
                    allocator_type::construct(ptr + k);
            }
            return ptr;
        }(), cols, rows, cols, 0, 0)
        {
            
        }
        matrix(const const_matrix_view<T>& rhs) : type(rhs.rows(), rhs.cols())
        {
            for (auto row = 0; row < rhs.rows(); ++row)
            {
                for (auto col = 0; col < rhs.cols(); ++col)
                    base::at(row, col) = rhs.at(row, col);
            }
        }
        matrix(const matrix<T>& rhs) : matrix(static_cast<const_matrix_view<T>>(rhs))
        {

        }
        matrix(matrix<T>&& rhs) noexcept : base(rhs.data(), rhs.m_cols, rhs.m_rows, rhs.m_cols, 0, 0)
        {
            new (&rhs) type;
        }
        ICY_DEFAULT_COPY_ASSIGN(matrix);
        ICY_DEFAULT_MOVE_ASSIGN(matrix);
        ~matrix() noexcept
        {
            for (auto k = base::size(); k--;)
                allocator_type::destroy(base::m_data + k);

            if (base::m_data)
                allocator_type::deallocate(const_cast<pointer>(base::m_data));
        }
    public:
        const_pointer data() const noexcept
        {
            return base::m_data;
        }
        pointer data() noexcept
        {
            return const_cast<pointer>(base::m_data);
        }
    };
    template<typename T>
    error_type copy(const const_matrix_view<T> src, matrix<T>& dst) noexcept
    {
        dst = matrix<T>(src.rows(), src.cols());
        if (dst.size() != src.size())
            return make_stdlib_error(std::errc::not_enough_memory);

        for (auto row = 0u; row < src.rows(); ++row)
        {
            for (auto col = 0u; col < src.cols(); ++col)
                dst.at(row, col) = src.at(row, col);
        }
        return error_type();
    }
}