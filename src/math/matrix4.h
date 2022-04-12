#ifndef MATRIX4_H
#define MATRIX4_H

#include "../hash_code.h"
#include "../util.h"

#include <iostream>
#include <array>

namespace hyperion {

class Vector3;
class Quaternion;

class Matrix4 {
    friend std::ostream &operator<<(std::ostream &os, const Matrix4 &mat);
public:
    static Matrix4 Translation(const Vector3 &);
    static Matrix4 Rotation(const Quaternion &);
    static Matrix4 Rotation(const Vector3 &axis, float radians);
    static Matrix4 Scaling(const Vector3 &);
    static Matrix4 Perspective(float fov, int w, int h, float n, float f);
    static Matrix4 Orthographic(float l, float r, float b, float t, float n, float f);
    static Matrix4 LookAt(const Vector3 &dir, const Vector3 &up);
    static Matrix4 LookAt(const Vector3 &pos, const Vector3 &target, const Vector3 &up);

    std::array<float, 16> values;

    Matrix4();
    explicit Matrix4(float *v);
    Matrix4(const Matrix4 &other);

    float Determinant() const;
    Matrix4 &Transpose();
    Matrix4 &Invert();

    Matrix4 &operator=(const Matrix4 &other);
    Matrix4 operator+(const Matrix4 &other) const;
    Matrix4 &operator+=(const Matrix4 &other);
    Matrix4 operator*(const Matrix4 &other) const;
    Matrix4 &operator*=(const Matrix4 &other);
    Matrix4 operator*(float scalar) const;
    Matrix4 &operator*=(float scalar);
    bool operator==(const Matrix4 &other) const;

    constexpr float operator()(int i, int j) const { return values[i * 4 + j]; }
    constexpr float &operator()(int i, int j) { return values[i * 4 + j]; }
    constexpr float At(int i, int j) const { return operator()(i, j); }
    constexpr float &At(int i, int j) { return operator()(i, j); }
    constexpr float operator[](int index) const { return values[index]; }
    constexpr float &operator[](int index) { return values[index]; }

    static Matrix4 Zeroes();
    static Matrix4 Ones();
    static Matrix4 Identity();

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (float value : values) {
            hc.Add(value);
        }

        return hc;
    }
};

static_assert(sizeof(Matrix4) == sizeof(float) * 16, "sizeof(Matrix4) must be equal to sizeof(float) * 16");

} // namespace hyperion

#endif
