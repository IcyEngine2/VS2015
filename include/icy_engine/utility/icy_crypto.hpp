#pragma once

#include <icy_engine/core/icy_string_view.hpp>

namespace icy
{
    static constexpr struct{ } crypto_random;
    class crypto_nonce
    {
    public:
        crypto_nonce() noexcept = default;
        crypto_nonce(decltype(crypto_random)) noexcept;
        const uint8_t* data() const noexcept
        {
            return m_array;
        }
        size_t size() const noexcept
        {
            return sizeof(m_array);
        }
    private:
        uint8_t m_array[24];
    };
    class crypto_salt
    {
    public:
        crypto_salt() noexcept = default;
        crypto_salt(const string_view login) noexcept;
        crypto_salt(decltype(crypto_random)) noexcept;
        const uint8_t* data() const noexcept
        {
            return m_array;
        }
        size_t size() const noexcept
        {
            return sizeof(m_array);
        }
    private:
        uint8_t m_array[16];
    };
    class crypto_key
    {
    public:
        static void pair(crypto_key& public_key, crypto_key& private_key) noexcept;
        crypto_key() noexcept : m_array{}
        {

        }
        crypto_key(decltype(crypto_random)) noexcept;
        crypto_key(const string_view login, const string_view password) noexcept;
        ICY_DEFAULT_COPY_ASSIGN(crypto_key);
        const uint8_t* data() const noexcept
        {
            return m_array;
        }
        uint8_t* data()  noexcept
        {
            return m_array;
        }
        size_t size() const noexcept
        {
            return sizeof(m_array);
        }
    private:
        uint8_t m_array[32];
    };
    class crypto_login : public detail::rel_ops<crypto_login>
    {
    public:
        crypto_login() noexcept = default;
        crypto_login(const string_view login) noexcept;
        int compare(const crypto_login& rhs) const noexcept
        {
            return memcmp(m_array, rhs.m_array, sizeof(*this));
        }
        const uint8_t* data() const noexcept
        {
            return m_array;
        }
        size_t size() const noexcept
        {
            return sizeof(m_array);
        }
    private:
        uint8_t m_array[64];
    };
    class crypto_mac
    {
    public:
        const uint8_t* data() const noexcept
        {
            return m_array;
        }
        uint8_t* data() noexcept
        {
            return m_array;
        }
        size_t size() const noexcept
        {
            return sizeof(m_array);
        }
    private:
        uint8_t m_array[16];
    };
    
    static constexpr auto crypto_msg_size = sizeof(crypto_nonce) + sizeof(crypto_mac);
    error_type crypto_msg_encode(const crypto_key& key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept;
    error_type crypto_msg_encode(const crypto_key& public_key, const crypto_key& private_key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept;
    error_type crypto_msg_decode(const crypto_key& key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept;
    error_type crypto_msg_decode(const crypto_key& public_key, const crypto_key& private_key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept;

    class crypto_msg_base
    {
    protected:
        crypto_msg_base() noexcept : m_nonce{}
        {

        }
        crypto_msg_base(const crypto_key& key, const size_t size, const void* const input, void* const output) noexcept;
        crypto_msg_base(const crypto_key& public_key, const crypto_key& private_key, const size_t size, const void* input, void* const output) noexcept;
        error_type decode(const crypto_key& key, const size_t size, const void* const input, void* const output) const noexcept;
        error_type decode(const crypto_key& public_key, const crypto_key& private_key, const size_t size, const void* const input, void* const output) const noexcept;
    private:
        const crypto_nonce m_nonce;
        crypto_mac m_mac;
    };
    template<typename T>
    class crypto_msg : public crypto_msg_base
    {
    public:
        static_assert(std::is_trivially_destructible<T>::value, "INVALID CRYPTO MSG TYPE (MUST BE TRIVIAL)");
        using value_type = T;
    public:
        crypto_msg() noexcept
        {

        }
        ICY_DEFAULT_COPY_ASSIGN(crypto_msg);
        //  private encode
        crypto_msg(const crypto_key& key, const T& input) noexcept : crypto_msg_base(key, sizeof(T), &input, m_value)
        {

        }
        //  public encode
        crypto_msg(const crypto_key& public_key, const crypto_key& private_key, const T& input) noexcept : 
            crypto_msg_base(public_key, private_key, sizeof(T), &input, m_value)
        {

        }
        //  private decode
        error_type decode(const crypto_key& key, T& value) const noexcept
        {
            return crypto_msg_base::decode(key, sizeof(T), m_value, &value);
        }
        //  public decode
        error_type decode(const crypto_key& public_key, const crypto_key& private_key, T& value) const noexcept
        {
            return crypto_msg_base::decode(public_key, private_key, sizeof(T), m_value, &value);
        }
    private:
        uint8_t m_value[sizeof(T)];
    };
}