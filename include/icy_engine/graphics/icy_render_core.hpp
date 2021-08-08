#pragma once

#include <icy_engine/core/icy_array.hpp>
#include <icy_engine/core/icy_color.hpp>
#include <icy_engine/core/icy_blob.hpp>

namespace icy
{
    union render_vec4
    {
        render_vec4() noexcept : vec{}
        {

        }
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
        float vec[4];
    };
    union render_vec2
    {
        render_vec2() noexcept : vec{}
        {

        }
        struct
        {
            float u;
            float v;
        };
        struct
        {
            float x;
            float y;
        };
        float vec[2];
    };
    struct render_gui_vtx
    {
        render_vec2 pos;
        render_vec2 tex;
        color col;
    };
    struct render_gui_cmd
    {
        render_vec4 clip;
        blob tex;           //  ID
        uint32_t vtx = 0;   //  offset
        uint32_t idx = 0;   //  offset
        uint32_t size = 0;  //  Count
    };
    struct render_gui_list
    {
        array<uint32_t> idx;
        array<render_gui_vtx> vtx;
        array<render_gui_cmd> cmd;
    };
    struct render_gui_frame
    {
        array<render_gui_list> data;
        uint32_t vbuffer = 0;
        uint32_t ibuffer = 0;
        render_vec4 viewport;
    };
    inline error_type copy(const render_gui_list& src, render_gui_list& dst) noexcept
    {
        ICY_ERROR(copy(src.idx, dst.idx));
        ICY_ERROR(copy(src.vtx, dst.vtx));
        ICY_ERROR(copy(src.cmd, dst.cmd));
        return error_type();
    }
    inline error_type copy(const render_gui_frame& src, render_gui_frame& dst) noexcept
    {
        dst.vbuffer = src.vbuffer;
        dst.ibuffer = src.ibuffer;
        dst.viewport = src.viewport;
        ICY_ERROR(copy(src.data, dst.data));
        return error_type();
    }
}