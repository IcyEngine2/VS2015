#pragma once

#include <icy_engine/core/icy_core.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_matrix.hpp>
#include <icy_engine/graphics/icy_render_math.hpp>

struct IDXGISwapChain;
using dxgi_present = long(__stdcall*)(IDXGISwapChain*, unsigned, unsigned);
struct HWND__;

//  by default: 17152
icy::error_type dxgi_offset(size_t& offset) noexcept;
icy::error_type dxgi_window(IDXGISwapChain& chain, icy::string& wname, icy::window_size& wsize, HWND__*& hwnd);
icy::error_type dxgi_copy(IDXGISwapChain& chain, 
    const icy::const_array_view<icy::render_d2d_rectangle_u> src, icy::array<icy::matrix<icy::color>>& dst) noexcept;