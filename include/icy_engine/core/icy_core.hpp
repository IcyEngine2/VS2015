#pragma once

#include <stdexcept>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <system_error>
#include <intrin.h>

namespace icy
{
    namespace detail
    {
        template<typename X>
        inline X icy_error_func(const X& error) noexcept
        {
            return error;
        }
    }
}

#pragma region ICY_MACRO
#define ICY_ERROR(X) if (const auto error_ = (X)) return icy::detail::icy_error_func(error_)
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
/*#define ICY_DECLARE_CLASS_MEMBER_EXISTS(NAME)                               \
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
 */
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
    namespace detail
    {
        enum class compare_type
        {
            none,
            container,
            pointer,
        };
        template<typename T> int compare_ex(std::integral_constant<compare_type, compare_type::container>, const T& lhs, const T& rhs) noexcept;
        template<typename T> int compare_ex(std::integral_constant<compare_type, compare_type::pointer>, const T& lhs, const T& rhs) noexcept;

        template<typename T, typename _ = void>
        struct is_container : std::false_type {};

        template<typename... Ts>
        struct is_container_helper {};

        template<typename T>
        struct is_container<
            T,
            std::conditional_t<
            false,
            is_container_helper<
            typename T::value_type,
            typename T::size_type,
            typename T::iterator,
            typename T::const_iterator,
            decltype(std::declval<T>().size()),
            decltype(std::declval<T>().begin()),
            decltype(std::declval<T>().end()),
            decltype(std::declval<T>().cbegin()),
            decltype(std::declval<T>().cend())
            >,
            void
            >
        > : public std::true_type{};

        template<typename T>
        int compare_ex(std::integral_constant<compare_type, compare_type::container>, const T& lhs, const T& rhs) noexcept;

        template<typename T>
        inline int compare(std::integral_constant<compare_type, compare_type::pointer>, const T& lhs, const T& rhs) noexcept
        {
            return int(rhs < lhs) - int(lhs < rhs);
        }
        template<typename T>
        inline int compare(std::integral_constant<compare_type, compare_type::pointer>, T& lhs, T& rhs) noexcept
        {
            return int(rhs < lhs) - int(lhs < rhs);
        }
    }

    template<typename T>
    inline int compare(const T& lhs, const T& rhs) noexcept
    {
        return detail::compare_ex(std::integral_constant<detail::compare_type,
            detail::is_container<T>::value ? detail::compare_type::container :
            std::is_pointer<T>::value ? detail::compare_type::pointer : detail::compare_type::none>{}, lhs, rhs);
    }

    template<>
    inline int compare<char const*>(char const* const& lhs, char const* const& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }

    template<typename T>
    inline int detail::compare_ex(std::integral_constant<detail::compare_type, detail::compare_type::container>, const T& lhs, const T& rhs) noexcept
    {
        const auto lhs_size = std::size(lhs);
        const auto rhs_size = std::size(rhs);
        const auto min_size = lhs_size < rhs_size ? lhs_size : rhs_size;
        auto it = std::begin(lhs);
        auto jt = std::begin(rhs);
        for (auto k = 0_z; k < min_size; ++k, ++it, ++jt)
        {
            const auto cmp = icy::compare(*it, *jt);
            if (cmp != 0) return cmp;
        }
        return int(rhs_size < lhs_size) - int(lhs_size < rhs_size);
    }
        
    template<> inline int compare<uint8_t>(const uint8_t& lhs, const uint8_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<uint16_t>(const uint16_t& lhs, const uint16_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<uint32_t>(const uint32_t& lhs, const uint32_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<uint64_t>(const uint64_t& lhs, const uint64_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<char>(const char& lhs, const char& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<int8_t>(const int8_t& lhs, const int8_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<int16_t>(const int16_t& lhs, const int16_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<int32_t>(const int32_t& lhs, const int32_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<int64_t>(const int64_t& lhs, const int64_t& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }

    template<> inline int compare<float>(const float& lhs, const float& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<double>(const double& lhs, const double& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
    template<> inline int compare<bool>(const bool& lhs, const bool& rhs) noexcept
    {
        return int(rhs < lhs) - int(lhs < rhs);
    }
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


        
#define rel_ops(T)                                                                          \
        bool operator<(const T& rhs) const noexcept                                         \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) < 0; \
        }                                                                                   \
        bool operator>(const T& rhs) const noexcept                                         \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) > 0; \
        }                                                                                   \
        bool operator<=(const T& rhs) const noexcept                                        \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) <= 0;\
        }                                                                                   \
        bool operator>=(const T& rhs) const noexcept                                        \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) >= 0;\
        }                                                                                   \
        bool operator==(const T& rhs) const noexcept                                        \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) == 0;\
        }                                                                                   \
        bool operator!=(const T& rhs) const noexcept                                        \
        {                                                                                   \
            return compare(*static_cast<const T*>(this), *static_cast<const T*>(&rhs)) != 0;\
        }
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

    class guid;
    
    template<> inline int compare<guid>(const guid& lhs, const guid& rhs) noexcept;

    class guid
    {
    public:
        rel_ops(guid);
        static guid create() noexcept;
        static error_type create(guid& index) noexcept 
        {
            index = create();
            if (!index) return last_system_error();
            return error_type();
        }
        guid() noexcept
        {
            memset(this, 0, sizeof(*this));
        }
        guid(const uint64_t x0, const uint64_t x1) noexcept : guid()
        {
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(this) + 0x00) = x0;
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(this) + 0x08) = x1;
        }
        explicit operator bool() const noexcept
        {
            const guid null;
            return memcmp(this, &null, sizeof(guid)) != 0;
        }
    private:
        uint32_t Data1;
        uint16_t Data2;
        uint16_t Data3;
        uint8_t Data4[8];
    };

    template<> inline int compare<guid>(const guid& lhs, const guid& rhs) noexcept
    {
        const auto value = memcmp(&lhs, &rhs, sizeof(guid));
        return value > 0 ? 1 : value < 0 ? -1 : 0;
    }

    using clock_type = std::chrono::steady_clock;
    using duration_type = clock_type::duration;

    inline uint32_t ms_timeout(const duration_type timeout) noexcept
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
        ms = ms < 0 ? 0 : ms > 0xFFFF'FFFF ? 0xFFFF'FFFF : ms;
        return uint32_t(ms);
    }
    static const auto max_timeout = std::chrono::milliseconds(0xFFFF'FFFF);

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

 
    error_type win32_parse_cargs(array<string>& args) noexcept;
    error_type computer_name(string& str) noexcept;
    error_type process_name(HINSTANCE__* module, string& str) noexcept;
    uint32_t process_index() noexcept;

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
        T find(const char* func) const noexcept
        {
            return reinterpret_cast<T>(GetProcAddress(m_module, func));
        }
        void* find(const char* func) const noexcept
        {
            return reinterpret_cast<void*>(GetProcAddress(m_module, func));
        }
        HINSTANCE__* handle() const noexcept
        {
            return m_module;
        }
    private:
        const char* m_name = nullptr;
        HINSTANCE__* m_module = nullptr;
    };
    inline library operator""_lib(const char* name, size_t) noexcept
    {
        return library(name);
    }

    template<typename T>
    inline error_type copy(const T& src, T& dst) noexcept
    {
        static_assert(std::is_trivially_destructible<T>::value, "CAN ONLY COPY TRIVIAL TYPES");
        static_assert(std::is_rvalue_reference<decltype(dst)>::value == false, "COPY != MOVE");
        dst = src;
        return error_type();
    }


    struct window_size
    {
        window_size() noexcept : x(0), y(0)
        {

        }
        window_size(const uint32_t x, const uint32_t y) noexcept : x(x), y(y)
        {

        }
        uint32_t x;
        uint32_t y;
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
    error_type win32_message(const string_view text, const string_view header, bool* yesno = nullptr) noexcept;
}
#endif

