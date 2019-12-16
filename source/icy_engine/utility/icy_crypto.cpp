#include <icy_engine/utility/icy_crypto.hpp>

#define SODIUM_STATIC 1

#pragma warning(push)
#pragma warning(disable:4324)   //  structure was padded due to alignment specifier
#include "../../libs/sodium/include/crypto_secretbox.h"
#include "../../libs/sodium/include/crypto_hash.h"
#include "../../libs/sodium/include/crypto_pwhash.h"
#include "../../libs/sodium/include/crypto_generichash.h"
#include "../../libs/sodium/include/randombytes.h"
#include "../../libs/sodium/include/crypto_box.h"
#pragma warning(pop)

#if _DEBUG
#pragma comment(lib, "libsodiumd")
#else
#pragma comment(lib, "libsodium")
#endif

using namespace icy;

static_assert(true
    && sizeof(crypto_nonce) == crypto_secretbox_NONCEBYTES
    && sizeof(crypto_salt) == crypto_pwhash_SALTBYTES
    && sizeof(crypto_salt) == crypto_generichash_BYTES_MIN
    && sizeof(crypto_key) == crypto_secretbox_KEYBYTES
    && sizeof(crypto_login) == crypto_hash_BYTES
    && sizeof(crypto_mac) == crypto_secretbox_MACBYTES
    , "INVALID CRYPTO SIZE");

crypto_nonce::crypto_nonce(decltype(crypto_random)) noexcept
{
    randombytes(m_array, sizeof(m_array));
}
crypto_salt::crypto_salt(const string_view login) noexcept
{
    static_assert(crypto_generichash_BYTES_MIN == crypto_pwhash_SALTBYTES, "");
    crypto_generichash(m_array, sizeof(m_array), reinterpret_cast<const uint8_t*>(
        login.bytes().data()), login.bytes().size(), nullptr, 0);
}
crypto_salt::crypto_salt(decltype(crypto_random)) noexcept
{
    randombytes(m_array, sizeof(m_array));
}
void crypto_key::pair(crypto_key& public_key, crypto_key& private_key) noexcept
{
    static_assert(sizeof(crypto_key::m_array) == crypto_box_PUBLICKEYBYTES, "");
    static_assert(sizeof(crypto_key::m_array) == crypto_box_SECRETKEYBYTES, "");
    crypto_box_keypair(public_key.m_array, private_key.m_array);
}
crypto_key::crypto_key(decltype(crypto_random)) noexcept
{
    randombytes(m_array, sizeof(m_array));
}
crypto_key::crypto_key(const string_view login, const string_view password) noexcept
{
    crypto_pwhash
    (
        m_array, sizeof(m_array),
        password.bytes().data(),
        password.bytes().size(),
        crypto_salt{ login }.data(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT
    );
}
crypto_login::crypto_login(const string_view login) noexcept
{
    crypto_hash(m_array, reinterpret_cast<const uint8_t*>(login.bytes().data()), login.bytes().size());
}

error_type icy::crypto_msg_encode(const crypto_key& key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept
{
    if (output.size() != input.size() + crypto_msg_size)
        return make_stdlib_error(std::errc::message_size);
    union
    {
        crypto_nonce* as_nonce;
        uint8_t* as_bytes;
    };
    as_bytes = output.data();
    *as_nonce = crypto_nonce{ crypto_random };

    const auto size = output.size() - sizeof(crypto_nonce);
    const auto nonce = as_nonce++;
    const auto bytes = as_bytes;
    crypto_secretbox_easy(bytes, input.data(), input.size(), nonce->data(), key.data());
    return {};
}
error_type icy::crypto_msg_encode(const crypto_key& public_key, const crypto_key& private_key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept
{
    if (output.size() != input.size() + crypto_msg_size)
        return make_stdlib_error(std::errc::message_size);
    union
    {
        crypto_nonce* as_nonce;
        uint8_t* as_bytes;
    };
    as_bytes = output.data();
    *as_nonce = crypto_nonce{ crypto_random };

    const auto size = output.size() - sizeof(crypto_nonce);
    const auto nonce = as_nonce++;
    const auto bytes = as_bytes;
    crypto_box_easy(bytes, input.data(), input.size(), nonce->data(), public_key.data(), private_key.data());
    return {};
}
error_type icy::crypto_msg_decode(const crypto_key& key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept
{
    if (output.size() + crypto_msg_size != input.size())
        return make_stdlib_error(std::errc::message_size);

    union
    {
        const crypto_nonce* as_nonce;
        const uint8_t* as_bytes;
    };
    as_bytes = input.data();
    const auto size = input.size() - sizeof(crypto_nonce);
    const auto nonce = as_nonce++;
    const auto bytes = as_bytes;
    const auto error = crypto_secretbox_open_easy(output.data(), bytes, size, nonce->data(), key.data());
    if (error)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}
error_type icy::crypto_msg_decode(const crypto_key& public_key, const crypto_key& private_key, const const_array_view<uint8_t> input, array_view<uint8_t> output) noexcept
{
    if (output.size() + crypto_msg_size != input.size())
        return make_stdlib_error(std::errc::message_size);

    union
    {
        const crypto_nonce* as_nonce;
        const uint8_t* as_bytes;
    };
    as_bytes = input.data();
    const auto size = input.size() - sizeof(crypto_nonce);
    const auto nonce = as_nonce++;
    const auto bytes = as_bytes;
    const auto error = crypto_box_open_easy(output.data(), bytes, size, nonce->data(), public_key.data(), private_key.data());
    if (error)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}

crypto_msg_base::crypto_msg_base(const crypto_key& key, const size_t size, const void* const input, void* const output) noexcept : m_nonce(crypto_random)
{
    crypto_secretbox_detached
    (
        static_cast<uint8_t*>(output),
        m_mac.data(),
        static_cast<const uint8_t*>(input),
        size,
        m_nonce.data(),
        key.data()
    );
}
crypto_msg_base::crypto_msg_base(const crypto_key& public_key, const crypto_key& private_key, const size_t size, const void* input, void* const output) noexcept : m_nonce(crypto_random)
{
    crypto_box_detached
    (
        static_cast<uint8_t*>(output),
        m_mac.data(),
        static_cast<const uint8_t*>(input),
        size,
        m_nonce.data(),
        public_key.data(),
        private_key.data()
    );
}
error_type crypto_msg_base::decode(const crypto_key& key, const size_t size, const void* const input, void* const output) const noexcept
{
    const auto error = crypto_secretbox_open_detached
    (
        static_cast<uint8_t*>(output),
        static_cast<const uint8_t*>(input),
        m_mac.data(),
        size,
        m_nonce.data(),
        key.data()
    );
    if (error)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}
error_type crypto_msg_base::decode(const crypto_key& public_key, const crypto_key& private_key, const size_t size, const void* const input, void* const output) const noexcept
{
    const auto error = crypto_box_open_detached
    (
        static_cast<uint8_t*>(output),
        static_cast<const uint8_t*>(input),
        m_mac.data(),
        size,
        m_nonce.data(),
        public_key.data(),
        private_key.data()
    );
    if (error)
        return make_stdlib_error(std::errc::illegal_byte_sequence);
    return {};
}