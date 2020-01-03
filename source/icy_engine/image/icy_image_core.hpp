#pragma once

#include <icy_engine/image/icy_image.hpp>

struct icy::image::data_type
{
    struct write_stream
    {
        virtual error_type append(const uint8_t* const data, const size_t size) noexcept = 0;
    };
    data_type(icy::global_heap_type heap) noexcept : heap(heap)
    {

    }
    virtual ~data_type() noexcept = 0
    {

    }
    virtual icy::error_type load(const icy::const_array_view<uint8_t> bytes) noexcept = 0;
    virtual icy::error_type save(const icy::const_matrix_view<icy::color> colors, write_stream& output) noexcept = 0;
    virtual icy::error_type view(const icy::image_size offset, icy::matrix_view<icy::color> colors) const noexcept = 0;
    icy::global_heap_type heap;
    icy::image_size size;
};

extern icy::image::data_type* make_image_png(icy::global_heap_type heap) noexcept;
extern icy::image::data_type* make_image_jpg(icy::global_heap_type heap) noexcept;