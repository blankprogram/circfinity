#pragma once

#include "uint128_t.h"
#include <algorithm>
#include <array>

using u64 = uint64_t;
using u128 = uint128_t;

inline constexpr int MAX_SIZE = 40;

// Bell numbers: B[n] = number of set partitions of n elements.
// Used to count valid variable labelings (RGS).

inline constexpr auto Bell = [] {
    std::array<u128, 2 * MAX_SIZE + 2> B{}, prev{}, temp{};
    B[0] = prev[0] = 1;
    for (int n = 1; n <= MAX_SIZE + 1; ++n) {
        temp[0] = prev[n - 1];
        for (int k = 1; k <= n; ++k) {
            temp[k] = temp[k - 1] + prev[k - 1];
        }
        B[n] = temp[0];
        std::ranges::copy(temp, prev.begin());
    }
    return B;
}();

// Powers of 3: 3^b is the number of operator patterns for b internal nodes.
inline constexpr auto Pow3 = [] {
    std::array<u64, 2 * MAX_SIZE + 2> P{};
    P[0] = 1;
    for (int i = 1; i <= MAX_SIZE; ++i)
        P[i] = P[i - 1] * 3;
    return P;
}();

// Weight factor for each possible b: B[b+1] * 3^b.
// Total possible variable labelings and operator patterns.
inline constexpr auto WeightFactor = [] {
    std::array<u128, 2 * MAX_SIZE + 2> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        W[b] = u128(Bell[b + 1]) * Pow3[b];
    return W;
}();

// Shape count DP table: C[s][b] = number of shapes with s leaves and b internal
// nodes.
inline const auto C = [] {
    std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> tbl{};
    tbl[1][0] = 1;
    for (int s = 2; s <= MAX_SIZE; ++s) {
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (int b1 = 0; b1 <= ls; ++b1) {
                if (tbl[ls][b1] == 0)
                    continue;
                for (int b2 = 0; b2 <= rs; ++b2) {
                    if (tbl[rs][b2] == 0)
                        continue;
                    tbl[s][b1 + b2 + 1] += tbl[ls][b1] * tbl[rs][b2];
                }
            }
        }
        for (int b = 0; b <= s - 1; ++b)
            tbl[s][b] += tbl[s - 1][b];
    }
    return tbl;
}();

// shapeTables = shape count, weight, and cumulative weight for each s.
inline const auto shapeTables = [] {
    std::array<u64, MAX_SIZE + 1> sc{};
    std::array<u128, MAX_SIZE + 1> sw{}, cum{};
    u128 running = 0;
    for (int s = 1; s <= MAX_SIZE; ++s) {
        u64 cnt = 0;
        u128 wgt = 0;
        for (int b = 0; b <= MAX_SIZE; ++b) {
            cnt += C[s][b];
            wgt += u128(C[s][b]) * WeightFactor[b];
        }
        sc[s] = cnt;
        sw[s] = wgt;
        running += wgt;
        cum[s] = running;
    }
    return std::tuple{sc, sw, cum};
}();

inline const auto &shapeCount = std::get<0>(shapeTables);
inline const auto &shapeWeight = std::get<1>(shapeTables);
inline const auto &cumShapeWeight = std::get<2>(shapeTables);

// blockAndRows = helper tables for shape unranking.
// blockWeight[s][ls] = weight of a block with left size ls.
// rowWeightSum[s][ls][b1] = weight of a row given b1.
inline const auto blockAndRows = [] {
    std::array<std::array<u128, MAX_SIZE + 1>, MAX_SIZE + 1> blk{};
    std::array<std::array<std::array<u128, MAX_SIZE + 1>, MAX_SIZE + 1>,
               MAX_SIZE + 1>
        row{};
    for (int s = 2; s <= MAX_SIZE; ++s) {
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            u128 blockSum = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                if (C[ls][b1] == 0)
                    continue;
                u128 rowSum = 0;
                for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                    if (C[rs][b2] == 0)
                        continue;
                    rowSum += u128(C[rs][b2]) * WeightFactor[b1 + b2 + 1];
                }
                row[s][ls][b1] = rowSum;
                blockSum += u128(C[ls][b1]) * rowSum;
            }
            blk[s][ls] = blockSum;
        }
    }
    return std::tuple{blk, row};
}();

inline const auto &blockWeight = std::get<0>(blockAndRows);
inline const auto &rowWeightSum = std::get<1>(blockAndRows);

// DP_RGS: counts of restricted growth strings for variable labeling.
inline const auto DP_RGS = [] {
    std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> dp{};
    for (int k = 0; k <= MAX_SIZE; ++k)
        dp[0][k] = 1;
    for (int len = 1; len <= MAX_SIZE; ++len) {
        for (int k = 0; k <= MAX_SIZE; ++k) {
            u64 sum = 0;
            for (int v = 0; v <= k + 1 && v <= MAX_SIZE; ++v) {
                int idx = std::max(v, k);
                sum += dp[len - 1][idx];
            }
            dp[len][k] = sum;
        }
    }
    return dp;
}();
