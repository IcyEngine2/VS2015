#pragma once

#include <stdexcept>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <system_error>
#include <intrin.h>

#pragma region ICY_MACRO
#define ICY_ERROR(X) if (const auto error_ = (X)) return error_
#define ICY_ANONYMOUS_VARIABLE(str) _CRT_CONCATENATE(str, __COUNTER__)
#define ICY_SCOPE_EXIT_EX auto ICY_ANONYMOUS_VARIABLE(ICY_SCOPE_EXIT_) = icy::detail::scope_guard_exit<void>{} + 
#define ICY_SCOPE_FAIL_EX auto ICY_ANONYMOUS_VARIABLE(ICY_SCOPE_FAIL_) = icy::detail::scope_guard_conditional<void, true>{} + 
#define ICY_SCOPE_SUCCESS_EX auto ICY_ANONYMOUS_VARIABLE(ICY_SCOPE_SUCCESS_) = icy::detail::scope_guard_conditional<void, false>{} + 
#define ICY_ROLLBACK(VAR) ICY_SCOPE_FAIL_EX [&, save = std::move(VAR)]() noexcept { VAR = std::move(save); }
#define ICY_SCOPE_EXIT ICY_SCOPE_EXIT_EX [&]() noexcept
#define ICY_SCOPE_FAIL ICY_SCOPE_FAIL_EX [&]() noexcept
#define ICY_SCOPE_SUCCESS ICY_SCOPE_SUCCESS_EX [&]()
#define ICY_STATIC_NAMESPACE_BEG namespace {
#define ICY_STATIC_NAMESPACE_END }
#define ICY_DECLARE_INJECT_FUNC(X) extern "C" __declspec(dllexport) unsigned __stdcall X(icy::inject_args* args)
#define ICY_ASSERT(EXPR, MESSAGE) \
    { static_assert(std::is_same<decltype(MESSAGE[0]), const char&>::value,\
    "ASSERTION BUG"); assert(MESSAGE && EXPR); }
#define ICY_CATCH	                                \
catch (const std::logic_error&)	                    \
{													\
	return uint32_t(std::errc::invalid_argument);   \
}													\
catch (const std::system_error& ex)					\
{													\
	return ex.code().value();	                    \
}													\
catch (const std::bad_alloc&)						\
{													\
	return uint32_t(std::errc::not_enough_memory);  \
}                                                   \
catch (...)                                         \
{                                                   \
	return UINT32_MAX;                              \
}

#define ICY_DECLARE_GLOBAL(X) __declspec(selectany) decltype(X) X

#define ICY_DEFAULT_COPY_ASSIGN(X) X& operator=(const X& rhs)               \
noexcept(std::is_nothrow_copy_constructible<X>::value)                      \
{                                                                           \
    if (this != std::addressof(rhs))                                        \
    {                                                                       \
        this->~X();                                                         \
        new (this) X{ rhs };                                                \
    }                                                                       \
    return *this;                                                           \
}
#define ICY_DEFAULT_MOVE_ASSIGN(X) X& operator=(X&& rhs) noexcept           \
{                                                                           \
    static_assert(std::is_nothrow_destructible<X>::value,                   \
        "DESTRUCTOR SHOULD NOT THROW");                                     \
    static_assert(std::is_nothrow_move_constructible<X>::value,             \
        "MOVE CONSTRUCTOR SHOULD NOT THROW");                               \
    this->~X();                                                             \
    return *(new (this) X{ std::move(rhs) });                               \
}
#define ICY_DECLARE_CLASS_MEMBER_EXISTS(NAME)                               \
template<typename class_type, typename... arg_types>                        \
struct _CRT_CONCATENATE(_CRT_CONCATENATE(has_member_, NAME), _helper)       \
{                                                                           \
    struct tag { };                                                         \
    template<typename = class_type> static auto test(int) -> decltype(      \
        std::declval<class_type>().NAME(std::declval<arg_types>()...));     \
    template<typename = class_type> static auto test(...) -> tag;           \
    using return_type = decltype(test<>(0));                                \
    static constexpr bool value = !std::is_same<return_type, tag>::value;   \
};                                                                          \
template<typename class_type, typename... arg_types>                        \
using _CRT_CONCATENATE(has_member_, NAME) = std::bool_constant<             \
    _CRT_CONCATENATE(_CRT_CONCATENATE(has_member_, NAME), _helper)          \
    <class_type, arg_types...>::value>
#define ICY_DECLARE_GLOBAL_CALL_EXISTS_EX(FUNC, NAMESPACE)                  \
template<typename... arg_types>                                             \
struct _CRT_CONCATENATE(_CRT_CONCATENATE(can_global_call_, FUNC), _helper)  \
{                                                                           \
    struct tag { };                                                         \
    template<typename T = void> static auto test(int) -> decltype(          \
        NAMESPACE FUNC(std::declval<arg_types>()...));                      \
    template<typename T = void> static auto test(...) -> tag;               \
    using return_type = decltype(test<>(0));                                \
    static constexpr bool value = !std::is_same<return_type, tag>::value;   \
};                                                                          \
template<typename... arg_types>                                             \
using _CRT_CONCATENATE(can_global_call_, FUNC) = std::bool_constant<        \
    _CRT_CONCATENATE(_CRT_CONCATENATE(can_global_call_, FUNC), _helper)     \
    <arg_types...>::value>
#define ICY_DECLARE_GLOBAL_CALL_EXISTS(FUNC) ICY_DECLARE_GLOBAL_CALL_EXISTS_EX(FUNC, )

#define ICY_DECLARE_CLASS_SUBTYPE_EXISTS(NAME)                              \
template<typename class_type, typename = std::bool_constant<                \
    std::is_class<class_type>::value>>                                      \
struct _CRT_CONCATENATE(_CRT_CONCATENATE(has_subtype_, NAME), _helper);     \
template<typename class_type>                                               \
struct _CRT_CONCATENATE(_CRT_CONCATENATE(has_subtype_, NAME), _helper)<     \
    class_type, std::true_type>                                             \
{                                                                           \
    struct tag { };                                                         \
    template<typename = class_type> static auto test(int) ->                \
        decltype(std::declval<typename class_type::NAME>());                \
    template<typename = class_type> static auto test(...) -> tag;           \
    using return_type = decltype(test<>(0));                                \
    static constexpr bool value = !std::is_same<return_type, tag>::value;   \
};                                                                          \
template<typename class_type>                                               \
struct _CRT_CONCATENATE(_CRT_CONCATENATE(has_subtype_, NAME), _helper)<     \
    class_type, std::false_type>                                            \
{                                                                           \
    struct tag { };                                                         \
    using return_type = tag;                                                \
    static constexpr bool value = false;                                    \
};                                                                          \
template<typename class_type>                                               \
using _CRT_CONCATENATE(has_subtype_, NAME) = std::bool_constant<            \
    _CRT_CONCATENATE(_CRT_CONCATENATE(has_subtype_, NAME), _helper)         \
    <class_type>::value>

#define ICY_FIND_FUNC(LIB, X) LIB.find<decltype(&X)>(#X)

#pragma endregion ICY_MACRO

struct HINSTANCE__;
using GetProcAddressReturnType = long long(__stdcall*)();
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) HINSTANCE__ * __stdcall LoadLibraryA(const char*);
extern "C" __declspec(dllimport) int __stdcall FreeLibrary(HINSTANCE__*);
extern "C" __declspec(dllimport) GetProcAddressReturnType __stdcall GetProcAddress(HINSTANCE__*, const char*);

namespace std
{
    template<typename T>
    constexpr size_t capacity(const T& container) noexcept
    {
        return container.capacity();
    }
    template<typename T, size_t N>
    constexpr size_t capacity(const T(&)[N]) noexcept
    {
        return N;
    }
    template<typename T, size_t N>
    constexpr size_t capacity(const std::array<T, N>&) noexcept
    {
        return N;
    }
}

namespace icy
{
    template<typename T> using remove_cvr = std::remove_cv_t<std::remove_reference_t<T>>;
    namespace detail
    {
        template<typename F>
        class scope_guard_exit
        {
        public:
            explicit scope_guard_exit(F&& func) noexcept : m_func{ std::move(func) }
            {

            }
            scope_guard_exit(scope_guard_exit&& rhs) noexcept : m_func{ std::move(rhs.m_func) }
            {

            }
            ~scope_guard_exit() noexcept
            {
                m_func();
            }
        private:
            scope_guard_exit& operator=(const scope_guard_exit&) = delete;
        private:
            F m_func;
        };
        template<> class scope_guard_exit<void>
        {
        public:
            template<typename F> scope_guard_exit<std::decay_t<F>> operator+(F&& fn) noexcept
            {
                return scope_guard_exit<std::decay_t<F>>{ std::move(fn) };
            }
        };
        class uncaught_exceptions_counter
        {
        public:
            uncaught_exceptions_counter() noexcept : m_count{ size_t(std::uncaught_exceptions()) }
            {

            }
            uncaught_exceptions_counter(const uncaught_exceptions_counter& rhs) noexcept : m_count{ rhs.m_count }
            {

            }
            uncaught_exceptions_counter& operator=(const uncaught_exceptions_counter&) = delete;
            bool operator!=(uncaught_exceptions_counter rhs) const noexcept
            {
                return m_count != rhs.m_count;
            }
            bool operator==(uncaught_exceptions_counter rhs) const noexcept
            {
                return m_count == rhs.m_count;
            }
        private:
            const size_t m_count;
        };
        template<typename F, bool execute_on_exception>
        class scope_guard_conditional
        {
        public:
            explicit scope_guard_conditional(F&& func) noexcept : m_func{ std::move(func) }
            {

            }
            scope_guard_conditional(scope_guard_conditional&& rhs) noexcept :
                m_func{ std::move(rhs.m_func) }, m_counter{ rhs.m_counter }
            {

            }
            ~scope_guard_conditional() noexcept(execute_on_exception)
            {
                const bool exception_exists = uncaught_exceptions_counter{} != m_counter;
                if (execute_on_exception == exception_exists)
                    m_func();
            }
        private:
            scope_guard_conditional& operator=(const scope_guard_conditional&) = delete;
        private:
            const uncaught_exceptions_counter m_counter;
            F m_func;
        };
        template<bool execute_on_exception> class scope_guard_conditional<void, execute_on_exception>
        {
        public:
            template<typename F> scope_guard_conditional<std::decay_t<F>, execute_on_exception> operator+(F&& fn) noexcept
            {
                return scope_guard_conditional<std::decay_t<F>, execute_on_exception>{ std::move(fn) };
            }
        };

        template<typename class_type, typename index_type>
        struct has_subscript_operator_helper
        {
            struct tag { };
            template<typename = void> static auto test(int) -> decltype(
                std::declval<class_type>()[std::declval<index_type>()]);
            template<typename = void> static auto test(...)->tag;
            using return_type = decltype(test<>(0));
            static constexpr bool value = !std::is_same<return_type, tag>::value;
        };
        template<typename class_type, typename index_type> using has_subscript_operator =
            std::bool_constant<has_subscript_operator_helper<class_type, index_type>::value>;

        template<typename lhs_type, typename rhs_type>
        struct is_explicitly_convertible_helper
        {
            struct tag { };
            template<typename = void> static auto test(int) -> decltype(
                static_cast<lhs_type>(std::declval<rhs_type>()));
            template<typename = void> static auto test(...)->tag;
            using return_type = decltype(test<>(0));
            static constexpr bool value = !std::is_same<return_type, tag>::value;
        };
        template<typename lhs_type, typename rhs_type> using is_explicitly_convertible =
            std::bool_constant<is_explicitly_convertible_helper<lhs_type, rhs_type>::value>;

        ICY_DECLARE_GLOBAL_CALL_EXISTS_EX(begin, std::);
        ICY_DECLARE_GLOBAL_CALL_EXISTS_EX(data, std::);

        template<typename T, typename = can_global_call_begin<T>>
        struct is_container;
        template<typename T>
        struct is_container<T, std::true_type> : std::true_type
        {
            using iterator_type = typename can_global_call_begin_helper<T>::return_type;
            using value_type = remove_cvr<decltype(*std::declval<iterator_type>())>;
        };
        template<typename T>
        struct is_container<T, std::false_type> : std::false_type
        {
            using iterator_type = void*;
            using value_type = void;
        };
        template<typename T> using is_array = is_container<T, can_global_call_data<T>>;
        template<typename T> struct is_std_array : public std::false_type
        {
            using value_type = void;
            static constexpr size_t size = 0;
        };
        template<typename T, size_t N> struct is_std_array<T[N]> : public std::true_type
        {
            using value_type = T;
            static constexpr size_t size = N;
        };
        template<typename T, size_t N> struct is_std_array<std::array<T, N>> : public std::true_type
        {
            using value_type = T;
            static constexpr size_t size = N;
        };

        template<typename... Fs>
        struct overload_set
        {

        };
        template<typename F0, typename... Fs>
        struct overload_set<F0, Fs...> : F0, overload_set<Fs...>
        {
            overload_set(F0 f0, Fs... fs) :
                F0{ std::move(f0) }, overload_set<Fs...>{ std::move(fs)... }
            {

            }
            using F0::operator();
            using overload_set<Fs...>::operator();
        };
        template<typename F>
        struct overload_set<F> : F
        {
            overload_set(F f) : F{ std::move(f) } {};
            using F::operator();
        };
        template<typename... Fs>
        overload_set<std::decay_t<Fs>...> overload(Fs&&...fs)
        {
            return{ std::forward<Fs>(fs)... };
        }

        inline constexpr size_t constexpr_log2(const size_t n, const size_t p = 0) noexcept
        {
            return (n <= 1) ? p : constexpr_log2(n / 2, p + 1);
        }
        inline auto log2(const uint32_t x) noexcept
        {
            unsigned long index;
            return _BitScanReverse(&index, x) * index;
        }
#if _WIN64
        inline auto log2(const uint64_t x) noexcept
        {
            unsigned long index;
            return _BitScanReverse64(&index, x) * index;
        }
#endif
        inline auto distance(const void* const first, const void* const last) noexcept
        {
            return size_t(static_cast<const char*>(last) - static_cast<const char*>(first));
        }

        ICY_DECLARE_CLASS_MEMBER_EXISTS(compare);
        template<typename lhs_type, typename rhs_type>
        int compare(const lhs_type& lhs, const rhs_type& rhs) noexcept;
        struct compare_helper
        {
            enum type
            {
                trivial,
                member,
                container,
                invalid,
            };
            using tag_trivial = std::integral_constant<type, trivial>;
            using tag_member = std::integral_constant<type, member>;
            using tag_container = std::integral_constant<type, container>;
            using tag_invalid = std::integral_constant<type, invalid>;

            template<typename lhs_type, typename rhs_type>
            static constexpr type get() noexcept
            {
                return has_member_compare<lhs_type, rhs_type>::value ? member :
                    is_container<lhs_type>::value && is_container<rhs_type>::value ? container :
                    std::is_scalar<lhs_type>::value && std::is_scalar<rhs_type>::value ? trivial : invalid;
            }

            template<typename lhs_type, typename rhs_type>
            constexpr int operator()(const lhs_type& lhs, const rhs_type& rhs, tag_trivial) const noexcept
            {
                return int(rhs < lhs) - int(lhs < rhs);
            }
            template<typename lhs_type, typename rhs_type>
            constexpr int operator()(const lhs_type& lhs, const rhs_type& rhs, tag_member) const noexcept
            {
                return lhs.compare(rhs);
            }
            template<typename lhs_type, typename rhs_type>
            int operator()(const lhs_type& lhs, const rhs_type& rhs, tag_invalid) const noexcept
            {
                static_assert(false, "TYPES CAN'T BE COMPARED!");
                return 0;
            }
            template<typename lhs_type, typename rhs_type>
            int operator()(const lhs_type& lhs, const rhs_type& rhs, tag_container) const noexcept
            {
                const auto lhs_size = std::size(lhs);
                const auto rhs_size = std::size(rhs);
                const auto min_size = lhs_size < rhs_size ? lhs_size : rhs_size;
                auto it = std::begin(lhs);
                auto jt = std::begin(rhs);
                for (auto k = 0_z; k < min_size; ++k, ++it, ++jt)
                {
                    const auto cmp = compare(*it, *jt);
                    if (cmp != 0) return cmp;
                }
                return compare(lhs_size, rhs_size);
            }
        };
        template<typename lhs_type, typename rhs_type>
        int compare(const lhs_type& lhs, const rhs_type& rhs) noexcept
        {
            return compare_helper{}(lhs, rhs, std::integral_constant<compare_helper::type,
                compare_helper::get<lhs_type, rhs_type>()>{});
        }
        template<typename T>
        class rel_ops
        {
        public:
            bool operator<(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) < 0;
            }
            bool operator>(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) > 0;
            }
            bool operator<=(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) <= 0;
            }
            bool operator>=(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) >= 0;
            }
            bool operator==(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) == 0;
            }
            bool operator!=(const rel_ops& rhs) const noexcept
            {
                return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) != 0;
            }
        };
    }

    struct error_source;
    struct error_type;
    class string_view;
    class string;
    template<typename T> class array_view;
    template<typename T> class array;
    enum class key : uint32_t;
    class color;
    class cvar;
    class mutex;
    
    struct error_source
    {
        unsigned long long hash = 0;
        inline bool operator==(const error_source& rhs) const noexcept
        {
            return hash == rhs.hash;
        }
    };
    struct error_type
    {
        error_type() noexcept : code(0)
        {

        }
        error_type(const unsigned code, const error_source source) noexcept : code(code), source(source)
        {

        }
        error_type(const error_type& rhs) noexcept = default;
        error_type& operator=(const error_type& rhs) noexcept = default;
        explicit operator bool() const noexcept
        {
            return code && source.hash;
        }
        bool operator==(const error_type& rhs) const noexcept
        {
            return code == rhs.code && source == rhs.source;
        }
        bool operator!=(const error_type& rhs) const noexcept
        {
            return !(*this == rhs);
        }
        unsigned code;
        error_source source;
    };

    error_source register_error_source(const string_view name, error_type(*func)(const unsigned code, const string_view locale, string& str));
    extern const error_source error_source_system;
    extern const error_source error_source_stdlib;

    inline constexpr unsigned long make_system_error_code(const unsigned long system_error) noexcept
    {
        return system_error ? ((system_error & 0x0000FFFF) | (7 << 16) | 0x80000000) : 0;
    }
    inline error_type make_system_error(const unsigned code) noexcept
    {
        return error_type(code, error_source_system);
    }
    inline error_type last_system_error() noexcept
    {
        return error_type(make_system_error_code(GetLastError()), error_source_system);
    }
    error_type last_stdlib_error() noexcept;
    inline error_type make_stdlib_error(const std::errc code) noexcept
    {
        return error_type(unsigned(code), error_source_stdlib);
    }
    error_type make_unexpected_error() noexcept;

    error_type to_string(const error_source source, string& str) noexcept;
    error_type to_string(const error_type error, string& str) noexcept;
    error_type to_string(const error_type error, string& str, const string_view locale) noexcept;

    struct guid : public detail::rel_ops<guid>
    {
        guid() noexcept : bytes{}
        {

        }
        guid(const uint64_t x0, const uint64_t x1) noexcept
        {
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(this) + 0x00) = x0;
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(this) + 0x08) = x1;
        }
        int compare(const guid& rhs) const noexcept
        {
            return memcmp(this, &rhs, sizeof(guid));
        }
        unsigned char bytes[16];
    };
    error_type create_guid(guid& guid) noexcept;

    using clock_type = std::chrono::steady_clock;
    using duration_type = clock_type::duration;

    inline uint32_t ms_timeout(const duration_type timeout) noexcept
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
        ms = ms < 0 ? 0 : ms > 0xFFFF'FFFF ? 0xFFFF'FFFF : ms;
        return uint32_t(ms);
    }
    static const auto max_timeout = std::chrono::milliseconds(0xFFFF'FFFF);

    using detail::has_subscript_operator;
    using detail::is_explicitly_convertible;
    using detail::is_container;
    using detail::is_array;
    inline constexpr size_t operator""_z(const uint64_t arg) noexcept
    {
        return size_t(arg);
    }

    template<typename T, typename A>
    constexpr T align_up(const T value, const A alignment) noexcept
    {
        return T(value + alignment - 1) & ~static_cast<T>(alignment - 1);
    }
    template<typename T, typename A>
    constexpr T* align_up(T* value, const A alignment) noexcept
    {
        return reinterpret_cast<T*>(align_up(reinterpret_cast<size_t>(value), alignment));
    }
    template<typename T, typename A>
    constexpr T align_down(const T value, const A alignment) noexcept
    {
        return T(value) & ~static_cast<T>(alignment - 1);
    }
    template<typename T, typename A>
    constexpr T* align_down(T* value, const A alignment) noexcept
    {
        return reinterpret_cast<T*>(align_down(reinterpret_cast<size_t>(value), alignment));
    }
    template<typename T, typename A>
    constexpr T round_up(const T value, const A round) noexcept
    {
        return T(((value + round - 1) / round) * round);
    }
    template<typename value_type, typename min_type, typename max_type>
    constexpr value_type clamp(const value_type& value, const min_type& min, const max_type& max) noexcept
    {
        return value < min ? min : value > max ? max : value;
    }
    template<typename value_type>
    constexpr value_type sqr(const value_type& value) noexcept
    {
        return value * value;
    }
    using detail::compare;

    struct window_size
    {
        window_size() noexcept : w(0), h(0)
        {

        }
        window_size(const uint32_t w, const uint32_t h) noexcept : w(w), h(h)
        {

        }
        window_size(const window_size& rhs) noexcept = default;
        ICY_DEFAULT_COPY_ASSIGN(window_size);
        const uint32_t w;
        const uint32_t h;
    };
    class mutex
    {
        friend cvar;
    public:
        mutex() noexcept = default;
        ~mutex() noexcept;
        mutex(const mutex&) = delete;
        error_type initialize() noexcept;
        bool try_lock() noexcept;
        void lock() noexcept;
        void unlock() noexcept;
        explicit operator bool() const noexcept;
    private:
        char m_buffer[40] = {};
    };
    class cvar
    {
    public:
        void wake() noexcept;
        error_type wait(mutex& mutex, const duration_type timeout = max_timeout) noexcept;
    private:
        void* m_ptr = nullptr;
    };

    error_type console_exit() noexcept;
    error_type console_write(const string_view str) noexcept;
    error_type console_write(const string_view str, const color foreground) noexcept;
    error_type console_read_line(string& str, bool& exit) noexcept;
    error_type console_read_key(key& key) noexcept;
    error_type win32_parse_cargs(array<string>& args) noexcept;
    error_type computer_name(string& str) noexcept;
    error_type process_name(HINSTANCE__* module, string& str) noexcept;

    class library
    {
    public:
        explicit library(const char* const name) noexcept : m_name(name)
        {

        }
        library(library&& rhs) noexcept : m_name(rhs.m_name), m_module(rhs.m_module)
        {
            rhs.m_module = nullptr;
        }
        ICY_DEFAULT_MOVE_ASSIGN(library);
        library(const library&) = delete;
        ~library() noexcept
        {
            shutdown();
        }
        error_type initialize() noexcept;
        void shutdown() noexcept;
        template<typename T>
        T find(const char* func) noexcept
        {
            return reinterpret_cast<T>(GetProcAddress(m_module, func));
        }
        void* find(const char* func) noexcept
        {
            return reinterpret_cast<void*>(GetProcAddress(m_module, func));
        }
    private:
        const char* m_name = nullptr;
        HINSTANCE__* m_module = nullptr;
    };
    inline library operator""_lib(const char* name, size_t) noexcept
    {
        return library(name);
    }

    struct inject_args
    {
        error_type error;
        size_t len = 0;
        char str[1];
    };

    struct rectangle
    {
        rectangle() noexcept : x(0), y(0), w(0), h(0)
        {

        }
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    };
}

#if !defined _CONSOLE
struct HINSTANCE__;
#if _USRDLL
enum class dll_main : uint32_t
{
    process_attach = 1,
    process_detach = 0,
    thread_attach = 2,
    thread_detach = 3,
};
#define main() __stdcall DllMain(HINSTANCE__*, dll_main source, void*)
#else
#define main() __stdcall wWinMain(HINSTANCE__*, HINSTANCE__*, wchar_t*, int)
#endif
namespace icy
{
    HINSTANCE__* win32_instance() noexcept;
    error_type win32_message(const string_view text, const string_view header) noexcept;
}
#endif

