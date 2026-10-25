#pragma once

#include <cmath>
#include <cstring>
#include <array>

namespace kjd {

constexpr float PI = 3.14159265358979323846f;

inline float radians(float degrees) {
    return degrees * (PI / 180.0f);
}

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 normalized() const {
        float len = length();
        if (len > 1e-6f) {
            return *this / len;
        }
        return {0.0f, 0.0f, 0.0f};
    }

    static float dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

// 4x4 matrix in column-major order (OpenGL standard)
// Index mapping:
//  [0]  [4]  [8]  [12]
//  [1]  [5]  [9]  [13]
//  [2]  [6]  [10] [14]
//  [3]  [7]  [11] [15]
struct Mat4 {
    float m[16]{};

    Mat4() {
        std::memset(m, 0, sizeof(m));
    }

    static Mat4 identity() {
        Mat4 res;
        res.m[0] = 1.0f;
        res.m[5] = 1.0f;
        res.m[10] = 1.0f;
        res.m[15] = 1.0f;
        return res;
    }

    static Mat4 perspective(float fovRad, float aspect, float zNear, float zFar) {
        Mat4 res;
        float tanHalfFov = std::tan(fovRad / 2.0f);

        res.m[0]  = 1.0f / (aspect * tanHalfFov);
        res.m[5]  = 1.0f / tanHalfFov;
        res.m[10] = -(zFar + zNear) / (zFar - zNear);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return res;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalized();
        Vec3 s = Vec3::cross(f, up).normalized();
        Vec3 u = Vec3::cross(s, f);

        Mat4 res = Mat4::identity();
        res.m[0] = s.x;
        res.m[4] = s.y;
        res.m[8] = s.z;

        res.m[1] = u.x;
        res.m[5] = u.y;
        res.m[9] = u.z;

        res.m[2] = -f.x;
        res.m[6] = -f.y;
        res.m[10] = -f.z;

        res.m[12] = -Vec3::dot(s, eye);
        res.m[13] = -Vec3::dot(u, eye);
        res.m[14] =  Vec3::dot(f, eye);
        return res;
    }

    static Mat4 translate(const Vec3& v) {
        Mat4 res = Mat4::identity();
        res.m[12] = v.x;
        res.m[13] = v.y;
        res.m[14] = v.z;
        return res;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 res;
        res.m[0]  = s.x;
        res.m[5]  = s.y;
        res.m[10] = s.z;
        res.m[15] = 1.0f;
        return res;
    }

    static Mat4 rotation(float rad, const Vec3& axis) {
        Vec3 a = axis.normalized();
        float c = std::cos(rad);
        float s = std::sin(rad);
        float nc = 1.0f - c;

        Mat4 res;
        res.m[0] = c + a.x * a.x * nc;
        res.m[1] = a.y * a.x * nc + a.z * s;
        res.m[2] = a.z * a.x * nc - a.y * s;
        res.m[3] = 0.0f;

        res.m[4] = a.x * a.y * nc - a.z * s;
        res.m[5] = c + a.y * a.y * nc;
        res.m[6] = a.z * a.y * nc + a.x * s;
        res.m[7] = 0.0f;

        res.m[8]  = a.x * a.z * nc + a.y * s;
        res.m[9]  = a.y * a.z * nc - a.x * s;
        res.m[10] = c + a.z * a.z * nc;
        res.m[11] = 0.0f;

        res.m[12] = 0.0f;
        res.m[13] = 0.0f;
        res.m[14] = 0.0f;
        res.m[15] = 1.0f;
        return res;
    }

    Mat4 operator*(const Mat4& rhs) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += m[k * 4 + r] * rhs.m[c * 4 + k];
                }
                res.m[c * 4 + r] = sum;
            }
        }
        return res;
    }

    const float* data() const {
        return m;
    }
};

} // namespace kjd
