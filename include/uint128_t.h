#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
// thanks
// https://stackoverflow.com/questions/8776073/efficient-multiply-divide-of-two-128-bit-integers-on-x86-no-64-bit
struct uint128_t {
    uint64_t low = 0, high = 0;

    constexpr uint128_t() noexcept = default;
    constexpr uint128_t(uint64_t lo) noexcept : low(lo), high(0) {}
    constexpr uint128_t(uint64_t hi, uint64_t lo) noexcept
        : low(lo), high(hi) {}

    constexpr bool operator==(const uint128_t &o) const noexcept {
        return low == o.low && high == o.high;
    }
    constexpr bool operator!=(const uint128_t &o) const noexcept {
        return !(*this == o);
    }
    constexpr bool operator<(const uint128_t &o) const noexcept {
        return (high < o.high) || (high == o.high && low < o.low);
    }
    constexpr bool operator<=(const uint128_t &o) const noexcept {
        return !(o < *this);
    }
    constexpr bool operator>(const uint128_t &o) const noexcept {
        return o < *this;
    }
    constexpr bool operator>=(const uint128_t &o) const noexcept {
        return !(*this < o);
    }

    constexpr uint128_t operator+(const uint128_t &o) const noexcept {
        uint64_t lo = low + o.low;
        uint64_t hi = high + o.high + (lo < low);
        return {hi, lo};
    }
    constexpr uint128_t operator-(const uint128_t &o) const noexcept {
        uint64_t lo = low - o.low;
        uint64_t hi = high - o.high - (lo > low);
        return {hi, lo};
    }
    constexpr uint128_t &operator+=(const uint128_t &o) noexcept {
        return *this = *this + o;
    }
    constexpr uint128_t &operator-=(const uint128_t &o) noexcept {
        return *this = *this - o;
    }

    constexpr uint128_t operator*(const uint128_t &o) const noexcept {
        uint64_t a0 = low, a1 = high;
        uint64_t b0 = o.low, b1 = o.high;

        uint64_t a0_lo = uint32_t(a0);
        uint64_t a0_hi = a0 >> 32;
        uint64_t b0_lo = uint32_t(b0);
        uint64_t b0_hi = b0 >> 32;

        uint64_t lo_lo = a0_lo * b0_lo;
        uint64_t lo_hi = a0_lo * b0_hi;
        uint64_t hi_lo = a0_hi * b0_lo;
        uint64_t hi_hi = a0_hi * b0_hi;

        uint64_t cross = lo_hi + hi_lo;
        uint64_t carry1 = (cross < lo_hi) ? 1ULL : 0ULL;
        uint64_t cross_lo = uint32_t(cross);
        uint64_t cross_hi = cross >> 32;

        uint64_t low_res = lo_lo + (cross_lo << 32);
        uint64_t carry2 = (low_res < lo_lo) ? 1ULL : 0ULL;

        uint64_t high_res = hi_hi;
        high_res += cross_hi;
        high_res += carry1;
        high_res += carry2;
        high_res += a1 * b0;
        high_res += a0 * b1;

        return {high_res, low_res};
    }

    constexpr uint128_t operator*(uint64_t o) const noexcept {
        uint64_t lo1 = uint32_t(low), hi1 = low >> 32;
        uint64_t lo2 = uint32_t(o), hi2 = o >> 32;

        uint64_t lo_lo = lo1 * lo2;
        uint64_t hi_lo = hi1 * lo2;
        uint64_t lo_hi = lo1 * hi2;
        uint64_t hi_hi = hi1 * hi2;

        uint64_t mid = hi_lo + lo_hi;
        uint64_t carry = (mid < hi_lo) ? (1ULL << 32) : 0;

        uint64_t low_part = lo_lo + (mid << 32);
        if (low_part < lo_lo)
            carry++;

        uint64_t high_part = hi_hi + (mid >> 32) + carry;
        high_part += high * o;

        return {high_part, low_part};
    }

    constexpr uint128_t operator<<(int n) const noexcept {
        if (n == 0)
            return *this;
        if (n >= 128)
            return {0, 0};
        if (n >= 64)
            return {low << (n - 64), 0};
        return {(high << n) | (low >> (64 - n)), low << n};
    }

    constexpr uint128_t operator>>(int n) const noexcept {
        if (n == 0)
            return *this;
        if (n >= 128)
            return {0, 0};
        if (n >= 64)
            return {0, high >> (n - 64)};
        return {high >> n, (high << (64 - n)) | (low >> n)};
    }

    constexpr std::pair<uint128_t, uint128_t> divmod(const uint128_t &v) const {
        if (v.high == 0 && v.low == 0)
            throw std::domain_error("divide by zero");

        uint128_t rem{0, 0};
        uint128_t q{0, 0};

        for (int i = 127; i >= 0; --i) {
            rem = rem << 1;

            bool bit;
            if (i >= 64) {
                bit = (high >> (i - 64)) & 1;
            } else {
                bit = (low >> i) & 1;
            }
            if (bit)
                rem.low |= 1;

            if (rem >= v) {
                rem = rem - v;
                if (i >= 64) {
                    q.high |= uint64_t(1) << (i - 64);
                } else {
                    q.low |= uint64_t(1) << i;
                }
            }
        }
        return {q, rem};
    }

    constexpr uint128_t operator/(const uint128_t &o) const {
        return divmod(o).first;
    }
    constexpr uint128_t operator%(const uint128_t &o) const {
        return divmod(o).second;
    }
    constexpr uint128_t &operator/=(const uint128_t &o) {
        return *this = *this / o;
    }
    constexpr uint128_t &operator%=(const uint128_t &o) {
        return *this = *this % o;
    }

    constexpr uint128_t operator/(uint64_t d) const noexcept {
        __uint128_t full = (__uint128_t(high) << 64) | low;
        __uint128_t q = full / d;
        return uint128_t(uint64_t(q >> 64), uint64_t(q));
    }

    constexpr uint64_t operator%(uint64_t d) const noexcept {
        __uint128_t full = (__uint128_t(high) << 64) | low;
        return uint64_t(full % d);
    }

    constexpr uint128_t &operator++() noexcept { return *this += 1; }
    constexpr uint128_t operator++(int) noexcept {
        uint128_t tmp = *this;
        ++(*this);
        return tmp;
    }

    constexpr uint128_t &operator--() noexcept { return *this -= 1; }

    constexpr uint128_t operator--(int) noexcept {
        uint128_t tmp = *this;
        --*this;
        return tmp;
    }
    constexpr bool operator!() const noexcept { return !low && !high; }
    constexpr explicit operator bool() const noexcept { return !(!*this); }

    std::string to_string() const {
        if (high == 0)
            return std::to_string(low);
        uint128_t tmp = *this;
        std::string out;
        while (tmp) {
            uint64_t r = tmp % 10;
            out += '0' + r;
            tmp = tmp / 10;
        }
        std::reverse(out.begin(), out.end());
        return out;
    }
};

constexpr uint128_t operator*(uint64_t lhs, const uint128_t &rhs) noexcept {
    return rhs * lhs;
}
