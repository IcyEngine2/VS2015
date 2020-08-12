#pragma once

#include <icy_engine/core/icy_string.hpp>
#include <icy_engine/core/icy_array.hpp>

namespace icy
{
    error_type process_path(const uint32_t index, string& str) noexcept;
    error_type process_launch(const string_view path, uint32_t& process, uint32_t& thread) noexcept;
    error_type process_threads(const uint32_t index, array<uint32_t>& threads) noexcept;
}