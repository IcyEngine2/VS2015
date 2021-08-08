#pragma once

#include <icy_engine/core/icy_array_view.hpp>
#include <icy_engine/core/icy_string_view.hpp>

namespace icy
{
    struct blob
    {
        blob() noexcept = default;
        uint32_t index = 0;
    };

    error_type blob_initialize()noexcept;
    void blob_shutdown() noexcept;

    error_type blob_add(const const_array_view<uint8_t> bytes, const string_view type, blob& object) noexcept;
    const_array_view<uint8_t> blob_data(const blob object) noexcept;
    string_view blob_type(const blob object) noexcept;

}