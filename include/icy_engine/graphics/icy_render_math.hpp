#pragma warning(push)
#pragma warning(disable:4201)

#include <cmath>

namespace icy
{
    struct render_d2d_vector
    {
        render_d2d_vector() noexcept : x(0), y(0)
        {

        }
        render_d2d_vector(const float x, const float y) noexcept : x(x), y(y)
        {

        }
        float x;
        float y;
    };
    struct render_d3d_vector
    {
        render_d3d_vector() noexcept : x(0), y(0), z(0)
        {

        }
        render_d3d_vector(const float x, const float y, const float z) noexcept : x(x), y(y), z(z)
        {

        }
        float x;
        float y;
        float z;
    };
    struct render_d4d_vector
    {
        render_d4d_vector() noexcept : x(0), y(0), z(0), w(0)
        {

        }
        render_d4d_vector(const float x, const float y, const float z, const float w) noexcept : x(x), y(y), z(z), w(w)
        {

        }
        float x;
        float y;
        float z;
        float w;
    };
    struct render_d2d_matrix
    {
        render_d2d_matrix() noexcept : array{ 1, 0, 0, 1, 0, 0 }
        {

        }
        render_d2d_matrix(const float x, const float y) noexcept : array{ 1, 0, 0, 1, x, y }
        {

        }
        render_d2d_matrix(const render_d2d_vector vec) noexcept : array{ 1, 0, 0, 1, vec.x, vec.y }
        {

        }
        static render_d2d_matrix rotate(const float angle) noexcept
        {
            const auto s = sin(angle);
            const auto c = cos(angle);
            render_d2d_matrix value;
            value._11 = c;
            value._22 = c;
            value._12 = +s;
            value._21 = -s;
            return value;
        }
        static render_d2d_matrix scale(const float sx, const float sy) noexcept
        {
            render_d2d_matrix value;
            value._11 = sx;
            value._22 = sy;
            return value;
        }
        render_d2d_matrix operator*(const render_d2d_matrix& rhs) const noexcept
        {
            const auto _33 = 1.0F;
            auto lhs = *this;
            lhs._11 = _11 * rhs._11 + _12 * rhs._21;
            lhs._12 = _11 * rhs._12 + _12 * rhs._22;
            lhs._21 = _21 * rhs._11 + _22 * rhs._21;
            lhs._22 = _21 * rhs._12 + _22 * rhs._22;
            lhs._31 = _31 * rhs._11 + _32 * rhs._21 + _33 * rhs._31;
            lhs._32 = _31 * rhs._12 + _32 * rhs._22 + _33 * rhs._32;
            return lhs;
        }
        render_d2d_matrix& operator*=(const render_d2d_matrix& rhs) noexcept
        {
            return *this = *this * rhs;
        }
        render_d2d_vector operator*(const render_d2d_vector& rhs) const noexcept
        {
            const auto rhs_z = 1.0F;
            return
            {
                _11 * rhs.x + _21 * rhs.y + _31 * rhs_z,
                _12 * rhs.x + _22 * rhs.y + _32 * rhs_z,
            };
        }
        union
        {
            struct
            {
                float _11;
                float _12;
                float _21;
                float _22;
                float _31;
                float _32;
            };
            float array[6];
        };
    };
    struct render_d2d_rectangle_u;
    struct render_d2d_rectangle
    {
        render_d2d_rectangle() noexcept : x(0), y(0), w(0), h(0)
        {

        }
        render_d2d_rectangle(const float x, const float y, const float w, const float h) noexcept : x(x), y(y), w(w), h(h)
        {

        }
        render_d2d_rectangle(const render_d2d_rectangle_u&) noexcept;
        float x;
        float y;
        float w;
        float h;
    };
    struct render_d2d_rectangle_u
    {
        render_d2d_rectangle_u() noexcept : x(0), y(0), w(0), h(0)
        {

        }
        render_d2d_rectangle_u(const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h) noexcept : x(x), y(y), w(w), h(h)
        {

        }
        uint32_t x;
        uint32_t y;
        uint32_t w;
        uint32_t h;
    };
    inline render_d2d_rectangle::render_d2d_rectangle(const render_d2d_rectangle_u& rhs) noexcept :
        x(float(rhs.x)), y(float(rhs.y)), w(float(rhs.w)), h(float(rhs.h))
    {

    }
    struct render_d2d_line
    {
        render_d2d_line() noexcept = default;
        render_d2d_line(const render_d2d_vector v0, const render_d2d_vector v1) noexcept : v0(v0), v1(v1)
        {

        }
        render_d2d_vector v0;
        render_d2d_vector v1;
    };
    struct render_d2d_triangle
    {
        render_d2d_triangle() noexcept : vec{}
        {

        }
        render_d2d_triangle(const render_d2d_vector v0, const render_d2d_vector v1, const render_d2d_vector v2) noexcept : v0(v0), v1(v1), v2(v2)
        {

        }
        union
        {
            struct
            {
                render_d2d_vector v0;
                render_d2d_vector v1;
                render_d2d_vector v2;
            };
            render_d2d_vector vec[3];
        };
    };
    struct render_box
    {
        render_d3d_vector center;
        render_d3d_vector axis[2];
        render_d3d_vector dimensions;
    };
    struct render_d2d_vertex
    {
        render_d3d_vector coord;	//	x,y (screen), z(layer 0..1)
        unsigned color;			    //	bgra
    };
}

#pragma warning(pop)
