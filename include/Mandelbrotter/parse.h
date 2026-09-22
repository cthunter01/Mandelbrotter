#pragma once

#include <charconv>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace mandelbrotter
{

/// Parses the whole of `text` as a number (no leading/trailing junk, no whitespace). Floating-point
/// results must be finite.
template <typename T>
[[nodiscard]] std::optional<T> parseNumber(std::string_view text) noexcept
{
    T           value{};
    const char* first    = std::to_address(text.begin());
    const char* last     = std::to_address(text.end());
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last)
    {
        return std::nullopt;
    }
    if constexpr (std::is_floating_point_v<T>)
    {
        if (!std::isfinite(value))
        {
            return std::nullopt;
        }
    }
    return value;
}

/// Trims ASCII spaces from both ends.
[[nodiscard]] constexpr std::string_view trimSpaces(std::string_view text) noexcept
{
    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    return text;
}

}  // namespace mandelbrotter
