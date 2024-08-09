/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Vector2.hpp>
#include <math/Vector4.hpp>
#include <math/MathUtil.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    Vec2f,
    HypProperty(NAME("X"), static_cast<float (Vec2f::*)() const>(&Vec2f::GetX), &Vec2f::SetX),
    HypProperty(NAME("Y"), static_cast<float (Vec2f::*)() const>(&Vec2f::GetY), &Vec2f::SetY)
);

HYP_DEFINE_CLASS(
    Vec2i,
    HypProperty(NAME("X"), static_cast<int (Vec2i::*)() const>(&Vec2i::GetX), &Vec2i::SetX),
    HypProperty(NAME("Y"), static_cast<int (Vec2i::*)() const>(&Vec2i::GetY), &Vec2i::SetY)
);

HYP_DEFINE_CLASS(
    Vec2u,
    HypProperty(NAME("X"), static_cast<uint (Vec2u::*)() const>(&Vec2u::GetX), &Vec2u::SetX),
    HypProperty(NAME("Y"), static_cast<uint (Vec2u::*)() const>(&Vec2u::GetY), &Vec2u::SetY)
);

namespace math {
namespace detail {

float Vec2<float>::Min() const
{
    return MathUtil::Min(x, y);
}

float Vec2<float>::Max() const
{
    return MathUtil::Max(x, y);
}

float Vec2<float>::Distance(const Vec2<float> &other) const
{
    return sqrt(DistanceSquared(other));
}

float Vec2<float>::DistanceSquared(const Vec2<float> &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
}

Vec2<float> &Vec2<float>::Normalize()
{
    float len = Length();
    float len_sqr = len * len;
    if (len_sqr == 0 || len_sqr == 1) {
        return *this;
    }

    (*this) *= (1.0f / len);
    return *this;
}

Vec2<float> &Vec2<float>::Lerp(const Vec2<float> &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    return *this;
}

float Vec2<float>::Dot(const Vec2<float> &other) const
{
    return x * other.x + y * other.y;
}

Vec2<float> Vec2<float>::Abs(const Vec2<float> &vec)
{
    return Vector2(abs(vec.x), abs(vec.y));
}

Vec2<float> Vec2<float>::Round(const Vec2<float> &vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vec2<float> Vec2<float>::Clamp(const Vec2<float> &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec2<float> Vec2<float>::Min(const Vec2<float> &a, const Vec2<float> &b)
{
    return Vec2<float>(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
}

Vec2<float> Vec2<float>::Max(const Vec2<float> &a, const Vec2<float> &b)
{
    return Vec2<float>(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
}

template<>
int math::detail::Vec2<int>::Max() const
{
    return MathUtil::Max(x, y);
}

template<>
int math::detail::Vec2<int>::Min() const
{
    return MathUtil::Min(x, y);
}

template<>
uint math::detail::Vec2<uint>::Max() const
{
    return MathUtil::Max(x, y);
}

template<>
uint math::detail::Vec2<uint>::Min() const
{
    return MathUtil::Min(x, y);
}

} // namespace detail
} // namespace math

std::ostream &operator<<(std::ostream &out, const Vec2<float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec2<uint> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << "]";
    return out;
}

} // namespace hyperion
