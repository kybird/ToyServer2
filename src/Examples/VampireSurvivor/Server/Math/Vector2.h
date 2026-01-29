#pragma once
#include <cmath>

namespace SimpleGame {

struct Vector2
{
    float x = 0.0f;
    float y = 0.0f;

    static constexpr float kEpsilon = 1e-6f;

    Vector2() = default;
    Vector2(float x, float y) : x(x), y(y)
    {
    }

    // Basic operators
    Vector2 operator+(const Vector2 &other) const
    {
        return {x + other.x, y + other.y};
    }
    Vector2 operator-(const Vector2 &other) const
    {
        return {x - other.x, y - other.y};
    }
    Vector2 operator*(float scalar) const
    {
        return {x * scalar, y * scalar};
    }
    Vector2 operator/(float scalar) const
    {
        return {x / scalar, y / scalar};
    }

    Vector2 &operator+=(const Vector2 &other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2 &operator-=(const Vector2 &other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vector2 &operator*=(float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    bool operator==(const Vector2 &other) const
    {
        return std::abs(x - other.x) < kEpsilon && std::abs(y - other.y) < kEpsilon;
    }

    bool operator!=(const Vector2 &other) const
    {
        return !(*this == other);
    }

    // Utilities
    bool IsZero() const
    {
        return std::abs(x) < kEpsilon && std::abs(y) < kEpsilon;
    }

    float MagnitudeSq() const
    {
        return x * x + y * y;
    }
    float Magnitude() const
    {
        return std::sqrt(MagnitudeSq());
    }

    Vector2 Normalized() const
    {
        float mag = Magnitude();
        if (mag > kEpsilon)
        {
            return *this / mag;
        }
        return Zero();
    }

    void Normalize()
    {
        float mag = Magnitude();
        if (mag > kEpsilon)
        {
            x /= mag;
            y /= mag;
        }
        else
        {
            x = 0;
            y = 0;
        }
    }

    static float Distance(const Vector2 &a, const Vector2 &b)
    {
        return (a - b).Magnitude();
    }

    static float DistanceSq(const Vector2 &a, const Vector2 &b)
    {
        return (a - b).MagnitudeSq();
    }

    float Dot(const Vector2 &other) const
    {
        return x * other.x + y * other.y;
    }

    static float Dot(const Vector2 &a, const Vector2 &b)
    {
        return a.x * b.x + a.y * b.y;
    }

    static Vector2 Zero()
    {
        return {0.0f, 0.0f};
    }
};

} // namespace SimpleGame
