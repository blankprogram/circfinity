#include "uint128_t.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using u64 = uint64_t;
using u128 = uint128_t;
inline constexpr int MAX_SIZE = 43;

// ─────────────────────────────────────────────
// Precomputed Bell numbers: B[n] = # of set partitions of n
// B[0] = 1
// ─────────────────────────────────────────────

inline constexpr auto Bell = [] {
    std::array<u64, 2 * MAX_SIZE + 2> B{}, prev{}, temp{};
    B[0] = prev[0] = 1;
    for (int n = 1; n <= MAX_SIZE + 1; ++n) {
        temp[0] = prev[n - 1];
        for (int k = 1; k <= n; ++k)
            temp[k] = temp[k - 1] + prev[k - 1];
        B[n] = temp[0];
        std::ranges::copy(temp, prev.begin());
    }
    return B;
}();

// ─────────────────────────────────────────────
// Precomputed powers of 3: 3^n
// ─────────────────────────────────────────────

inline constexpr auto Pow3 = [] {
    std::array<u64, 2 * MAX_SIZE + 2> P{};
    P[0] = 1;
    for (int i = 1; i <= MAX_SIZE; ++i)
        P[i] = P[i - 1] * 3;
    return P;
}();

// ─────────────────────────────────────────────
// Combined weight factors for each # of internal nodes b:
// W[b] = B[b+1] * 3^b
// ─────────────────────────────────────────────

inline constexpr auto WeightFactor = [] {
    std::array<u128, 2 * MAX_SIZE + 2> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        W[b] = u128(Bell[b + 1]) * Pow3[b];
    return W;
}();

// ─────────────────────────────────────────────
// Shape Count DP
// C[s][b] = # of binary trees with s leaves and b internal nodes
// ─────────────────────────────────────────────

// 4) C[s][b] at compile time
inline constexpr auto C = [] {
    std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> tbl{};
    tbl[1][0] = 1;
    for (int s = 2; s <= MAX_SIZE; ++s) {
        // binary splits
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
        // unary-not expansions
        for (int b = 0; b <= s - 1; ++b)
            tbl[s][b] += tbl[s - 1][b];
    }
    return tbl;
}();

// 5) shapeCount[s], shapeWeight[s], cumShapeWeight[s] at compile time
inline constexpr auto shapeTables = [] {
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
inline constexpr auto &shapeCount = std::get<0>(shapeTables);
inline constexpr auto &shapeWeight = std::get<1>(shapeTables);
inline constexpr auto &cumShapeWeight = std::get<2>(shapeTables);

// 6) blockWeight[s][ls] and rowWeightSum[s][ls][b1] at compile time
inline constexpr auto blockAndRows = [] {
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

inline constexpr auto &blockWeight = std::get<0>(blockAndRows);
inline constexpr auto &rowWeightSum = std::get<1>(blockAndRows);

// 7) DP_RGS[len][k] at compile time
inline constexpr auto DP_RGS = [] {
    std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> dp{};
    for (int k = 0; k <= MAX_SIZE; ++k)
        dp[0][k] = 1;
    for (int len = 1; len <= MAX_SIZE; ++len) {
        for (int k = 0; k <= MAX_SIZE; ++k) {
            u64 sum = 0;
            // v up to k+1, but cap at MAX_SIZE
            for (int v = 0; v <= k + 1 && v <= MAX_SIZE; ++v) {
                int idx = (v > k ? v : k);
                sum += dp[len - 1][idx];
            }
            dp[len][k] = sum;
        }
    }
    return dp;
}();

// ─────────────────────────────────────────────
// Expression Builder: builds a string from shape + operators + labels
// ─────────────────────────────────────────────

inline constexpr size_t OUT_BUF_SIZE = 1 << 16;
inline char OUT[OUT_BUF_SIZE];

/**
 * @brief Recursively emits the expression syntax
 *   s: current tree size
 *   idx: shape index
 *   ops: operator sequence
 *   rgs: variable labels
 *   OL: output length so far
 */
inline void build_expr(int s, u128 idx, const std::vector<int> &ops,
                       const std::vector<int> &rgs, int &leafIdx, int &opIdx,
                       int &OL) {
    if (s == 1) {
        OUT[OL++] = char('A' + rgs[leafIdx++]);
        return;
    }
    u128 binCount = shapeCount[s] - shapeCount[s - 1];
    if (idx < binCount) {
        // Binary node
        u128 acc = 0;
        int ls = 1;
        for (int l = 1; l <= s - 2; ++l) {
            u128 bs = u128(shapeCount[l]) * shapeCount[s - 1 - l];
            if (idx < acc + bs) {
                ls = l;
                idx -= acc;
                break;
            }
            acc += bs;
        }
        int rs = s - 1 - ls;
        u128 i = idx / shapeCount[rs];
        u128 j = idx % shapeCount[rs];
        int op = ops[opIdx++];
        std::string_view opStr = op == 0 ? "AND" : op == 1 ? "OR" : "XOR";
        std::memcpy(OUT + OL, opStr.data(), opStr.size());
        OL += opStr.size();
        OUT[OL++] = '(';
        build_expr(ls, i, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ',';
        build_expr(rs, j, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
    } else {
        // Unary NOT
        std::memcpy(OUT + OL, "NOT(", 4);
        OL += 4;
        build_expr(s - 1, idx - binCount, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
    }
}

// ─────────────────────────────────────────────
// Shape Unranking: decode shape index + weight offset
// into concrete subtree splits and internal node counts.
// Complex but deterministic.
// ─────────────────────────────────────────────

inline u128 shape_unrank(int s, u128 woff, int &b_shape, u128 &variantOff) {
    if (s == 1) {
        b_shape = 0;
        variantOff = woff;
        return 0;
    }
    u128 binTotal = u128(shapeCount[s]) - shapeCount[s - 1];
    u128 acc = 0;
    for (int ls = 1; ls <= s - 2; ++ls) {
        u128 bw = blockWeight[s][ls];
        if (woff < acc + bw) {
            u128 offB = woff - acc;
            int rs = s - 1 - ls;
            u128 rowAcc = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                if (!C[ls][b1])
                    continue;
                u128 rowW = rowWeightSum[s][ls][b1];
                u128 totW = u128(C[ls][b1]) * rowW;
                if (offB < rowAcc + totW) {
                    u128 offG = offB - rowAcc;
                    u128 i = offG / rowW;
                    u128 offR = offG % rowW;
                    u128 colAcc = 0;
                    for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                        if (!C[rs][b2])
                            continue;
                        u128 cellW = WeightFactor[b1 + b2 + 1];
                        u128 totC = u128(C[rs][b2]) * cellW;
                        if (offR < colAcc + totC) {
                            u128 off2 = offR - colAcc;
                            u128 j = off2 / cellW;
                            variantOff = off2 % cellW;
                            b_shape = b1 + b2 + 1;
                            u128 base = 0;
                            for (int x = 1; x < ls; ++x)
                                base +=
                                    u128(shapeCount[x]) * shapeCount[s - 1 - x];
                            return base + i * shapeCount[rs] + j;
                        }
                        colAcc += totC;
                    }
                }
                rowAcc += totW;
            }
        }
        acc += bw;
    }
    return binTotal + shape_unrank(s - 1, woff - acc, b_shape, variantOff);
}

// ─────────────────────────────────────────────
// Top-level Unranking: maps 1 ≤ N ≤ total to unique expression string.
// Uses shape, op pattern, and variable RGS.
// ─────────────────────────────────────────────

inline std::string unrank(u128 N) {
    assert(N >= 1 && N <= cumShapeWeight[MAX_SIZE]);
    int s = 1;
    while (cumShapeWeight[s] < N)
        ++s;

    u128 layerOff = N - (cumShapeWeight[s - 1] + 1);
    int b_shape{};
    u128 variantOff{};
    u128 shapeIdx = shape_unrank(s, layerOff, b_shape, variantOff);
    u128 nVar = Bell[b_shape + 1];
    u128 opIndex = variantOff / nVar;
    u128 varIndex = variantOff % nVar;

    std::vector<int> ops(b_shape);
    for (int i = b_shape - 1; i >= 0; --i) {
        ops[i] = int(opIndex % 3);
        opIndex /= 3;
    }

    std::vector<int> rgs(b_shape + 1);
    rgs[0] = 0;
    int maxSeen = 0;
    u128 rem = varIndex;
    for (int pos = 1; pos <= b_shape; ++pos) {
        int tail = b_shape - pos;
        for (int v = 0; v <= maxSeen + 1; ++v) {
            int nk = std::max(v, maxSeen);
            if (rem < DP_RGS[tail][nk]) {
                rgs[pos] = v;
                maxSeen = nk;
                break;
            }
            rem -= DP_RGS[tail][nk];
        }
    }

    int OL = 0, leafIdx = 0, opIdx = 0;
    build_expr(s, shapeIdx, ops, rgs, leafIdx, opIdx, OL);
    return {OUT, OUT + OL};
}
int main() {
    using Clock = std::chrono::high_resolution_clock;

    auto start = Clock::now();

    const u128 total = cumShapeWeight[MAX_SIZE];

    for (u128 i = 1; i <= 100; ++i)
        printf("#%s: %s\n", i.to_string().c_str(), unrank(i).c_str());
    printf("\n#%s: %s\n", total.to_string().c_str(), unrank(total).c_str());

    __uint128_t tot128 = ((__uint128_t)total.high << 64) | total.low;
    long double pct =
        (long double)tot128 / powl((long double)2.0, 128) * 100.0L;
    printf("Used %.18Lf%% of 128-bit range\n", pct);

    auto end = Clock::now();
    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    auto elapsed_ms = elapsed_us / 1000.0;

    printf("Elapsed time: %.3f ms (%.0f µs)\n", elapsed_ms, (double)elapsed_us);
    return 0;
}
