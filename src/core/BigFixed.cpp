#include "Mandelbrotter/BigFixed.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mandelbrotter
{

namespace
{

using Limb  = std::uint32_t;
using Limbs = std::vector<Limb>;

constexpr int  kLimbBits     = BigFixed::kLimbBits;
constexpr Limb kSignBit      = 0x8000'0000U;
constexpr Limb kDecimalBase  = 10;
constexpr int  kMantissaBits = std::numeric_limits<double>::digits;
/// Exponents beyond this in decimal input are rejected rather than expanded into digit strings.
constexpr int kMaxDecimalExponent = 100'000;
/// An upper bound on the bits a decimal digit contributes (log2(10) < 4).
constexpr std::size_t kBitsPerDecimalDigit = 4;

int limbsFor(int fractionBits) noexcept
{
    const int bits =
        std::clamp(fractionBits, BigFixed::kMinFractionBits, BigFixed::kMaxFractionBits);
    return (bits + kLimbBits - 1) / kLimbBits;
}

bool negative(const Limbs& limbs) noexcept
{
    return (limbs.back() & kSignBit) != 0;
}

/// Two's complement negation in place.
void negate(Limbs& limbs) noexcept
{
    std::uint64_t carry = 1;
    for (Limb& limb : limbs)
    {
        const std::uint64_t sum = static_cast<std::uint64_t>(~limb) + carry;
        limb                    = static_cast<Limb>(sum);
        carry                   = sum >> kLimbBits;
    }
}

/// The unsigned magnitude of a two's complement value.
Limbs magnitude(Limbs limbs)
{
    if (negative(limbs))
    {
        negate(limbs);
    }
    return limbs;
}

/// a += b for equally sized operands, wrapping.
void addInto(Limbs& a, const Limbs& b) noexcept
{
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const std::uint64_t sum = static_cast<std::uint64_t>(a[i]) + b[i] + carry;
        a[i]                    = static_cast<Limb>(sum);
        carry                   = sum >> kLimbBits;
    }
}

/// a -= b for equally sized operands, wrapping.
void subtractFrom(Limbs& a, const Limbs& b) noexcept
{
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const std::uint64_t difference = static_cast<std::uint64_t>(a[i]) - b[i] - borrow;
        a[i]                           = static_cast<Limb>(difference);
        borrow                         = (difference >> kLimbBits) & 1U;
    }
}

/// Multiplies an unsigned magnitude by a small factor in place; returns the limb carried out.
Limb multiplySmall(Limbs& limbs, Limb factor) noexcept
{
    std::uint64_t carry = 0;
    for (Limb& limb : limbs)
    {
        const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
        limb                        = static_cast<Limb>(product);
        carry                       = product >> kLimbBits;
    }
    return static_cast<Limb>(carry);
}

/// Adds a small value to an unsigned magnitude in place, wrapping.
void addSmall(Limbs& limbs, Limb value) noexcept
{
    std::uint64_t carry = value;
    for (Limb& limb : limbs)
    {
        if (carry == 0)
        {
            return;
        }
        const std::uint64_t sum = static_cast<std::uint64_t>(limb) + carry;
        limb                    = static_cast<Limb>(sum);
        carry                   = sum >> kLimbBits;
    }
}

/// Divides an unsigned magnitude by a small divisor in place, truncating; returns the remainder.
Limb divideSmall(Limbs& limbs, Limb divisor) noexcept
{
    std::uint64_t remainder = 0;
    for (Limb& limb : std::views::reverse(limbs))
    {
        const std::uint64_t current = (remainder << kLimbBits) | limb;
        limb                        = static_cast<Limb>(current / divisor);
        remainder                   = current % divisor;
    }
    return static_cast<Limb>(remainder);
}

bool allZero(const Limbs& limbs) noexcept
{
    return std::ranges::all_of(limbs, [](Limb limb) { return limb == 0; });
}

/// The same value with `fractionLimbs` limbs after the point: zero-extended, or truncated at the
/// bottom (toward -infinity).
Limbs withFractionLimbs(const Limbs& limbs, std::size_t fractionLimbs)
{
    const std::size_t total = fractionLimbs + BigFixed::kIntegerLimbs;
    Limbs             out(total, 0);
    if (total >= limbs.size())
    {
        std::ranges::copy(limbs, out.begin() + static_cast<std::ptrdiff_t>(total - limbs.size()));
    }
    else
    {
        std::ranges::copy(limbs.end() - static_cast<std::ptrdiff_t>(total), limbs.end(),
                          out.begin());
    }
    return out;
}

/// Ors a 64-bit value, shifted left by `bitPosition`, into the limbs. Bits above the top are lost.
void orShifted(Limbs& limbs, std::uint64_t value, int bitPosition) noexcept
{
    const auto          index  = static_cast<std::size_t>(bitPosition / kLimbBits);
    const int           offset = bitPosition % kLimbBits;
    const std::uint64_t low    = value << offset;
    const std::uint64_t high   = offset == 0 ? 0 : value >> (2 * kLimbBits - offset);
    const std::array    parts{static_cast<Limb>(low), static_cast<Limb>(low >> kLimbBits),
                              static_cast<Limb>(high)};
    for (std::size_t k = 0; k < parts.size(); ++k)
    {
        if (index + k < limbs.size())
        {
            limbs[index + k] |= parts.at(k);
        }
    }
}

bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

Limb digitValue(char c) noexcept
{
    return static_cast<Limb>(c - '0');
}

/// A decimal string taken apart: the value is 0.<digits> * 10^pointPosition, i.e. the point sits
/// after `pointPosition` digits (possibly before the first or after the last).
struct DecimalParts
{
    bool           negative{false};
    std::string    digits;
    std::ptrdiff_t pointPosition{0};
};

/// Parses the exponent after the 'e'; the sign is optional.
std::optional<int> parseExponent(std::string_view text) noexcept
{
    bool negativeExponent = false;
    if (!text.empty() && (text.front() == '+' || text.front() == '-'))
    {
        negativeExponent = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty() || !std::ranges::all_of(text, isDigit))
    {
        return std::nullopt;
    }
    int exponent = 0;
    for (const char c : text)
    {
        exponent = exponent * static_cast<int>(kDecimalBase) + static_cast<int>(digitValue(c));
        if (exponent > kMaxDecimalExponent)
        {
            return std::nullopt;
        }
    }
    return negativeExponent ? -exponent : exponent;
}

std::optional<DecimalParts> parseDecimal(std::string_view text)
{
    DecimalParts parts;
    if (!text.empty() && (text.front() == '+' || text.front() == '-'))
    {
        parts.negative = text.front() == '-';
        text.remove_prefix(1);
    }
    const auto             exponentAt  = text.find_first_of("eE");
    const std::string_view mantissa    = text.substr(0, exponentAt);
    const auto             point       = mantissa.find('.');
    const auto             integerPart = mantissa.substr(0, point);
    const auto             fractionPart =
        point == std::string_view::npos ? std::string_view{} : mantissa.substr(point + 1);
    if ((integerPart.empty() && fractionPart.empty()) ||
        !std::ranges::all_of(integerPart, isDigit) || !std::ranges::all_of(fractionPart, isDigit))
    {
        return std::nullopt;
    }
    parts.digits.reserve(integerPart.size() + fractionPart.size());
    parts.digits.append(integerPart);
    parts.digits.append(fractionPart);
    parts.pointPosition = static_cast<std::ptrdiff_t>(integerPart.size());
    if (exponentAt != std::string_view::npos)
    {
        const auto exponent = parseExponent(text.substr(exponentAt + 1));
        if (!exponent)
        {
            return std::nullopt;
        }
        parts.pointPosition += *exponent;
    }
    return parts;
}

/// Rounds a digit string with one extra digit at the end, half up, carrying into the integer
/// digits when needed.
void roundLastDigit(std::string& integerText, std::string& fractionText)
{
    const bool roundUp = fractionText.back() >= '5';
    fractionText.pop_back();
    if (!roundUp)
    {
        return;
    }
    for (std::string* text : {&fractionText, &integerText})
    {
        for (char& digit : std::views::reverse(*text))
        {
            if (digit != '9')
            {
                ++digit;
                return;
            }
            digit = '0';
        }
    }
    integerText.insert(integerText.begin(), '1');
}

}  // namespace

BigFixed::BigFixed(int fractionBits)
  : m_limbs(static_cast<std::size_t>(limbsFor(fractionBits) + kIntegerLimbs), 0)
{
}

BigFixed::BigFixed(std::vector<Limb> limbs) noexcept : m_limbs(std::move(limbs)) { }

int BigFixed::fractionLimbs() const noexcept
{
    return static_cast<int>(m_limbs.size()) - kIntegerLimbs;
}

int BigFixed::fractionBits() const noexcept
{
    return fractionLimbs() * kLimbBits;
}

BigFixed BigFixed::fromDouble(double value, int fractionBits)
{
    BigFixed result(fractionBits);
    if (!std::isfinite(value) || value == 0.0)
    {
        return result;
    }
    int          exponent = 0;
    const double mantissa = std::frexp(std::abs(value), &exponent);  // |value| = m * 2^exponent
    auto significand = static_cast<std::uint64_t>(std::ldexp(mantissa, kMantissaBits));  // exact
    // |value| * 2^fractionBits = significand * 2^shift
    const int shift = exponent - kMantissaBits + result.fractionBits();
    if (shift < 0)
    {
        significand = -shift >= 2 * kLimbBits ? 0 : significand >> -shift;
        orShifted(result.m_limbs, significand, 0);
    }
    else
    {
        orShifted(result.m_limbs, significand, shift);
    }
    if (value < 0.0)
    {
        negate(result.m_limbs);
    }
    return result;
}

std::optional<BigFixed> BigFixed::fromDecimal(std::string_view text, int fractionBits)
{
    auto parts = parseDecimal(text);
    if (!parts)
    {
        return std::nullopt;
    }
    // Pad so that the point falls inside the digit string, then read all the digits as one
    // integer M: the value is M / 10^fractionDigits.
    std::string& digits = parts->digits;
    if (parts->pointPosition < 0)
    {
        digits.insert(0, static_cast<std::size_t>(-parts->pointPosition), '0');
        parts->pointPosition = 0;
    }
    const auto pointPosition = static_cast<std::size_t>(parts->pointPosition);
    if (pointPosition > digits.size())
    {
        digits.append(pointPosition - digits.size(), '0');
    }
    const std::size_t fractionDigits = digits.size() - pointPosition;

    const auto        fractionLimbs = static_cast<std::size_t>(limbsFor(fractionBits));
    const std::size_t digitLimbs =
        digits.size() * kBitsPerDecimalDigit / static_cast<std::size_t>(kLimbBits) + 1;
    Limbs integer(digitLimbs + kIntegerLimbs, 0);
    for (const char c : digits)
    {
        multiplySmall(integer, kDecimalBase);
        addSmall(integer, digitValue(c));
    }
    // M * 2^fractionBits, then floor-divided by 10 once per fraction digit: nested integer
    // divisions compose exactly, so this is the correctly truncated value.
    Limbs work(fractionLimbs, 0);
    work.insert(work.end(), integer.begin(), integer.end());
    for (std::size_t i = 0; i < fractionDigits; ++i)
    {
        divideSmall(work, kDecimalBase);
    }
    const std::size_t kept = fractionLimbs + kIntegerLimbs;
    const bool overflow    = (work[kept - 1] & kSignBit) != 0 ||
                             !std::ranges::all_of(work.begin() + static_cast<std::ptrdiff_t>(kept),
                                                  work.end(), [](Limb limb) { return limb == 0; });
    if (overflow)
    {
        return std::nullopt;
    }
    work.resize(kept);
    if (parts->negative)
    {
        negate(work);
    }
    return BigFixed(std::move(work));
}

double BigFixed::toDouble() const
{
    const Limbs mag = magnitude(m_limbs);
    const auto  top = std::ranges::find_if(mag.rbegin(), mag.rend(), [](Limb l) { return l != 0; });
    if (top == mag.rend())
    {
        return 0.0;
    }
    // The top three non-zero limbs carry 96 bits: more than a double can hold.
    const auto        highIndex = static_cast<std::size_t>(std::distance(top, mag.rend())) - 1;
    const std::size_t lowIndex  = highIndex >= 2 ? highIndex - 2 : 0;
    double            result    = 0.0;
    for (std::size_t i = highIndex + 1; i > lowIndex; --i)
    {
        const std::size_t k = i - 1;
        result += std::ldexp(static_cast<double>(mag[k]),
                             (static_cast<int>(k) - fractionLimbs()) * kLimbBits);
    }
    return isNegative() ? -result : result;
}

std::string BigFixed::toDecimal(int fractionDigits) const
{
    const Limbs mag = magnitude(m_limbs);
    const auto  f   = static_cast<std::ptrdiff_t>(fractionLimbs());
    Limbs       integer(mag.begin() + f, mag.end());
    Limbs       fraction(mag.begin(), mag.begin() + f);

    std::string integerText;
    while (!allZero(integer))
    {
        integerText.push_back(static_cast<char>('0' + divideSmall(integer, kDecimalBase)));
    }
    if (integerText.empty())
    {
        integerText = "0";
    }
    std::ranges::reverse(integerText);

    // One digit more than asked for, for the rounding.
    std::string fractionText;
    for (int i = 0; i <= std::max(fractionDigits, 0); ++i)
    {
        fractionText.push_back(static_cast<char>('0' + multiplySmall(fraction, kDecimalBase)));
    }
    roundLastDigit(integerText, fractionText);

    const bool printsZero =
        integerText == "0" && fractionText.find_first_not_of('0') == std::string::npos;
    std::string out;
    if (isNegative() && !printsZero)
    {
        out.push_back('-');
    }
    out += integerText;
    if (fractionDigits > 0)
    {
        out.push_back('.');
        out += fractionText;
    }
    return out;
}

BigFixed BigFixed::withFractionBits(int fractionBits) const
{
    return BigFixed(withFractionLimbs(m_limbs, static_cast<std::size_t>(limbsFor(fractionBits))));
}

bool BigFixed::isZero() const noexcept
{
    return allZero(m_limbs);
}

bool BigFixed::isNegative() const noexcept
{
    return negative(m_limbs);
}

BigFixed BigFixed::abs() const
{
    return isNegative() ? -*this : *this;
}

BigFixed BigFixed::operator-() const
{
    BigFixed result = *this;
    negate(result.m_limbs);
    return result;
}

BigFixed& BigFixed::operator+=(const BigFixed& other)
{
    if (other.m_limbs.size() > m_limbs.size())
    {
        m_limbs = withFractionLimbs(m_limbs, static_cast<std::size_t>(other.fractionLimbs()));
    }
    if (other.m_limbs.size() == m_limbs.size())
    {
        addInto(m_limbs, other.m_limbs);
    }
    else
    {
        addInto(m_limbs,
                withFractionLimbs(other.m_limbs, static_cast<std::size_t>(fractionLimbs())));
    }
    return *this;
}

BigFixed& BigFixed::operator-=(const BigFixed& other)
{
    if (other.m_limbs.size() > m_limbs.size())
    {
        m_limbs = withFractionLimbs(m_limbs, static_cast<std::size_t>(other.fractionLimbs()));
    }
    if (other.m_limbs.size() == m_limbs.size())
    {
        subtractFrom(m_limbs, other.m_limbs);
    }
    else
    {
        subtractFrom(m_limbs,
                     withFractionLimbs(other.m_limbs, static_cast<std::size_t>(fractionLimbs())));
    }
    return *this;
}

BigFixed& BigFixed::operator*=(const BigFixed& other)
{
    const auto  f = static_cast<std::size_t>(std::max(fractionLimbs(), other.fractionLimbs()));
    const bool  negativeResult = isNegative() != other.isNegative();
    const Limbs a              = magnitude(withFractionLimbs(m_limbs, f));
    const Limbs b              = magnitude(withFractionLimbs(other.m_limbs, f));
    const std::size_t n        = a.size();

    // Schoolbook product of the magnitudes, 2n limbs.
    Limbs product(2 * n, 0);
    for (std::size_t i = 0; i < n; ++i)
    {
        if (a[i] == 0)
        {
            continue;
        }
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < n; ++j)
        {
            const std::uint64_t term =
                static_cast<std::uint64_t>(a[i]) * b[j] + product[i + j] + carry;
            product[i + j] = static_cast<Limb>(term);
            carry          = term >> kLimbBits;
        }
        product[i + n] = static_cast<Limb>(carry);
    }
    // The product has 2f fraction limbs; dropping the lowest f truncates toward zero.
    const auto offset = static_cast<std::ptrdiff_t>(f);
    Limbs      result(product.begin() + offset,
                      product.begin() + offset + static_cast<std::ptrdiff_t>(n));
    if (negativeResult)
    {
        negate(result);
    }
    m_limbs = std::move(result);
    return *this;
}

bool operator==(const BigFixed& a, const BigFixed& b) noexcept
{
    // The shorter operand's missing low limbs are zero, so the longer one's extra low limbs must
    // be zero and the limbs they share must match.
    const std::size_t common = std::min(a.m_limbs.size(), b.m_limbs.size());
    const auto        extraA = static_cast<std::ptrdiff_t>(a.m_limbs.size() - common);
    const auto        extraB = static_cast<std::ptrdiff_t>(b.m_limbs.size() - common);
    return std::ranges::equal(a.m_limbs.begin() + extraA, a.m_limbs.end(),
                              b.m_limbs.begin() + extraB, b.m_limbs.end()) &&
           std::ranges::all_of(a.m_limbs.begin(), a.m_limbs.begin() + extraA,
                               [](BigFixed::Limb limb) { return limb == 0; }) &&
           std::ranges::all_of(b.m_limbs.begin(), b.m_limbs.begin() + extraB,
                               [](BigFixed::Limb limb) { return limb == 0; });
}

std::strong_ordering operator<=>(const BigFixed& a, const BigFixed& b) noexcept
{
    if (a.isNegative() != b.isNegative())
    {
        return a.isNegative() ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    // Same sign: two's complement patterns order like unsigned integers, top limb first.
    const std::size_t common = std::min(a.m_limbs.size(), b.m_limbs.size());
    const std::size_t extraA = a.m_limbs.size() - common;
    const std::size_t extraB = b.m_limbs.size() - common;
    for (std::size_t k = common; k-- > 0;)
    {
        const BigFixed::Limb limbA = a.m_limbs[extraA + k];
        const BigFixed::Limb limbB = b.m_limbs[extraB + k];
        if (limbA != limbB)
        {
            return limbA < limbB ? std::strong_ordering::less : std::strong_ordering::greater;
        }
    }
    // Shared limbs equal: any non-zero extra low limbs make that operand the larger one.
    const auto nonZeroBelow = [](const std::vector<BigFixed::Limb>& limbs, std::size_t count) {
        return std::ranges::any_of(limbs.begin(),
                                   limbs.begin() + static_cast<std::ptrdiff_t>(count),
                                   [](BigFixed::Limb limb) { return limb != 0; });
    };
    if (nonZeroBelow(a.m_limbs, extraA))
    {
        return std::strong_ordering::greater;
    }
    if (nonZeroBelow(b.m_limbs, extraB))
    {
        return std::strong_ordering::less;
    }
    return std::strong_ordering::equal;
}

}  // namespace mandelbrotter
