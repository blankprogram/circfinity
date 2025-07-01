#ifndef COMPUTE_DATA_H
#define COMPUTE_DATA_H
#include <array>

#include <boost/multiprecision/cpp_int.hpp>
using bigint = boost::multiprecision::cpp_int;

constexpr int MAX_S = 100;
constexpr int MAX_U = 100;
constexpr int MAX_N = MAX_S - 1 + MAX_U;
constexpr std::size_t kMaxLabels = MAX_S * 2;

/* 3^k table – operator choices per binary node */
inline const auto Pow3 = [] {
    std::array<bigint, MAX_S + 1> a{};
    a[0] = 1;
    for (int i = 1; i <= MAX_S; ++i)
        a[i] = a[i - 1] * 3;
    return a;
}();

/* Bell numbers – |set partitions| of the leaves */
inline const auto Bell = [] {
    std::array<bigint, MAX_S + 1> b{}, prev{}, cur{};
    prev[0] = b[0] = 1;
    for (int n = 1; n <= MAX_S; ++n) {
        cur[0] = prev[n - 1];
        for (int k = 1; k <= n; ++k)
            cur[k] = cur[k - 1] + prev[k - 1];
        b[n] = cur[0];
        prev = cur;
    }
    return b;
}();

/* C[s][u] – number of shapes with s leaves, u unary nodes */
inline const auto C = [] {
    std::array<std::array<bigint, MAX_U + 1>, MAX_S + 1> c{};
    c[1][0] = 1;
    for (int s = 1; s <= MAX_S; ++s)
        for (int u = 1; u <= MAX_U; ++u)
            c[s][u] = c[s][u - 1];
    for (int s = 2; s <= MAX_S; ++s)
        for (int u = 0; u <= MAX_U; ++u)
            for (int ls = 1; ls < s; ++ls) {
                int rs = s - ls;
                for (int u1 = 0; u1 <= u; ++u1)
                    c[s][u] += c[ls][u1] * c[rs][u - u1];
            }
    return c;
}();

/* DP_RGS – Ruskey/Williams table for restricted growth strings */
inline const auto DP_RGS = [] {
    std::array<std::array<bigint, MAX_S + 2>, MAX_S + 2> dp{};
    for (int m = 0; m <= MAX_S + 1; ++m)
        dp[0][m] = 1;
    for (int len = 1; len <= MAX_S; ++len)
        for (int max = MAX_S; max >= 0; --max) {
            bigint s = 0;
            for (int v = 0; v <= max + 1; ++v)
                s += dp[len - 1][std::max(max, v)];
            dp[len][max] = s;
        }
    return dp;
}();

/* Wn / prefixN – weight per total size n  = Σ C[s][u]·3^(s-1)·Bell[s] */
inline const auto size_pair = [] {
    std::array<bigint, MAX_N + 1> W{}, P{};
    for (int n = 0; n <= MAX_N; ++n) {
        bigint w = 0;
        for (int u = 0; u <= n && u <= MAX_U; ++u) {
            int s = n - u + 1, b = n - u;
            if (s < 1 || s > MAX_S)
                continue;
            w += C[s][u] * Pow3[b] * Bell[s];
        }
        W[n] = w;
        P[n] = (n ? P[n - 1] : 0) + w;
    }
    return std::pair{W, P};
}();

inline const auto &Wn = size_pair.first;
inline const auto &prefixN = size_pair.second;

inline const auto Labels = [] {
    constexpr std::size_t MaxLen = 8;
    using buf_t = std::array<char, MaxLen>;

    std::array<std::string, kMaxLabels> tbl{};

    for (std::size_t i = 0; i < kMaxLabels; ++i) {
        buf_t buf{};
        std::size_t pos = MaxLen;
        std::uint64_t id = i;

        do {
            buf[--pos] = static_cast<char>('A' + id % 26);
            id /= 26;
            if (id == 0)
                break;
            --id;
        } while (true);

        tbl[i] = std::string(buf.data() + pos, MaxLen - pos);
    }
    return tbl;
}();
#endif
