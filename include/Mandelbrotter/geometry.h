#pragma once

namespace mandelbrotter
{

/// A point in the complex plane. Deliberately minimal: the iteration kernels work on raw doubles.
struct Complex
{
    double re{};
    double im{};

    [[nodiscard]] constexpr double normSquared() const noexcept { return re * re + im * im; }

    bool operator==(const Complex&) const = default;

    friend constexpr Complex operator+(Complex a, Complex b) noexcept
    {
        return {a.re + b.re, a.im + b.im};
    }
    friend constexpr Complex operator-(Complex a, Complex b) noexcept
    {
        return {a.re - b.re, a.im - b.im};
    }
    friend constexpr Complex operator*(Complex a, Complex b) noexcept
    {
        return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
    }
    friend constexpr Complex operator*(Complex a, double s) noexcept
    {
        return {a.re * s, a.im * s};
    }
    friend constexpr Complex operator/(Complex a, double s) noexcept
    {
        return {a.re / s, a.im / s};
    }
};

struct PixelPoint
{
    int x{};
    int y{};

    bool operator==(const PixelPoint&) const = default;
};

struct PixelSize
{
    int width{};
    int height{};

    [[nodiscard]] constexpr bool empty() const noexcept { return width <= 0 || height <= 0; }

    bool operator==(const PixelSize&) const = default;
};

struct PixelRect
{
    int x{};
    int y{};
    int width{};
    int height{};

    [[nodiscard]] constexpr bool empty() const noexcept { return width <= 0 || height <= 0; }
    [[nodiscard]] constexpr int  right() const noexcept { return x + width; }    // exclusive
    [[nodiscard]] constexpr int  bottom() const noexcept { return y + height; }  // exclusive

    bool operator==(const PixelRect&) const = default;
};

}  // namespace mandelbrotter
