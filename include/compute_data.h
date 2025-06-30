#ifndef COMPUTE_DATA_H
#define COMPUTE_DATA_H
#include <array>
#include <uint128_t.h>
using u128 = unsigned __int128;

constexpr int MAX_S = 18;
constexpr int MAX_U = 18;
constexpr int MAX_N = MAX_S - 1 + MAX_U;

/* 3^k table – operator choices per binary node */
inline constexpr auto Pow3 = [] {
    std::array<u128, MAX_S + 1> a{};
    a[0] = 1;
    for (int i = 1; i <= MAX_S; ++i)
        a[i] = a[i - 1] * 3;
    return a;
}();

/* Bell numbers – |set partitions| of the leaves */
inline constexpr auto Bell = [] {
    std::array<u128, MAX_S + 1> b{}, prev{}, cur{};
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
inline constexpr auto C = [] {
    std::array<std::array<u128, MAX_U + 1>, MAX_S + 1> c{};
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
inline constexpr auto DP_RGS = [] {
    std::array<std::array<u128, MAX_S + 2>, MAX_S + 2> dp{};
    for (int m = 0; m <= MAX_S + 1; ++m)
        dp[0][m] = 1;
    for (int len = 1; len <= MAX_S; ++len)
        for (int max = MAX_S; max >= 0; --max) {
            u128 s = 0;
            for (int v = 0; v <= max + 1; ++v)
                s += dp[len - 1][std::max(max, v)];
            dp[len][max] = s;
        }
    return dp;
}();

/* Wn / prefixN – weight per total size n  = Σ C[s][u]·3^(s-1)·Bell[s] */
inline constexpr auto size_pair = [] {
    std::array<u128, MAX_N + 1> W{}, P{};
    for (int n = 0; n <= MAX_N; ++n) {
        u128 w = 0;
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

inline constexpr auto &Wn = size_pair.first;
inline constexpr auto &prefixN = size_pair.second;

#endif
