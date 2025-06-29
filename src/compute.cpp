#include "compute.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

char OUT[OUT_BUF_SIZE] = {};

// Recursively build the Boolean expression string from shape index and
// operators
void build_expr(int s, u128 idx, const std::vector<int> &ops,
                const std::vector<int> &rgs, int &leafIdx, int &opIdx,
                int &OL) {
    if (s == 1) {
        OUT[OL++] = char('A' + rgs[leafIdx++]);
        return;
    }

    u128 binCount = shapeCount[s] - shapeCount[s - 1];
    if (idx < binCount) {
        // Binary operator: find left/right subtree split
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
        // Unary NOT node
        std::memcpy(OUT + OL, "NOT(", 4);
        OL += 4;
        build_expr(s - 1, idx - binCount, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
    }
}

// Decode weight offset back to shape index + internal node count.
// This handles the layered DP blocks deterministically.
u128 shape_unrank(int s, u128 woff, int &b_shape, u128 &variantOff) {
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

                    // Find right-side b2 matching this residual offset
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

// map rank N to its unique Boolean expression
std::string unrank(u128 N) {
    assert(N >= 1 && N <= cumShapeWeight[MAX_SIZE]);

    // Find the leaf size layer s such that N lands in its weight interval
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

    // Decode operators (base-3)
    std::vector<int> ops(b_shape);
    for (int i = b_shape - 1; i >= 0; --i) {
        ops[i] = int(opIndex % 3);
        opIndex /= 3;
    }

    // Decode variable labels (restricted growth string)
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

    // Emit expression string into OUT buffer
    int OL = 0, leafIdx = 0, opIdx = 0;
    build_expr(s, shapeIdx, ops, rgs, leafIdx, opIdx, OL);
    return {OUT, OUT + OL};
}

int main() {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    const u128 total = cumShapeWeight[MAX_SIZE];
    const u128 N = 100;

    std::unordered_map<std::string, std::vector<u128>> exprMap;
    std::vector<std::string> exprs;

    for (u128 i = 1; i <= N; ++i) {
        std::string expr = unrank(i);
        exprMap[expr].push_back(i);
        exprs.push_back(expr);
        printf("#%s: %s\n", i.to_string().c_str(), expr.c_str());
    }

    bool has_duplicates = false;
    for (const auto &[expr, ranks] : exprMap) {
        if (ranks.size() <= 1)
            continue;

        has_duplicates = true;
        printf("\nDuplicate expression: \"%s\" (%zu times)\n", expr.c_str(),
               ranks.size());

        for (u128 idx : ranks) {
            int s = 1;
            while (cumShapeWeight[s] < idx)
                ++s;
            u128 layerOff = idx - (cumShapeWeight[s - 1] + 1);

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

            printf("  #%s: s=%d | shapeIdx=%s | b=%d | variantOff=%s\n",
                   idx.to_string().c_str(), s, shapeIdx.to_string().c_str(),
                   b_shape, variantOff.to_string().c_str());

            printf("    ops = ");
            for (int o : ops)
                printf("%d", o);
            printf("\n");

            printf("    rgs = ");
            for (int r : rgs)
                printf("%d", r);
            printf("\n");
        }
    }

    if (!has_duplicates)
        printf("\n✅ No visual duplicates found in first %s expressions.\n",
               N.to_string().c_str());

    // Summary
    u128 firstDeep = cumShapeWeight[MAX_SIZE - 1] + 1;
    std::string exprFirstDeep = unrank(firstDeep);
    std::string exprLastDeep = unrank(total);

    printf("\nFirst expression at deepest layer (%d leaves) (#%s): %s\n",
           MAX_SIZE, firstDeep.to_string().c_str(), exprFirstDeep.c_str());
    printf("Last  expression at deepest layer (#%s): %s\n",
           total.to_string().c_str(), exprLastDeep.c_str());

    // 128-bit usage
    __uint128_t tot128 = ((__uint128_t)total.high << 64) | total.low;
    long double pct = (long double)tot128 / powl(2.0L, 128) * 100.0L;
    printf("\nUsed %.18Lf%% of 128-bit range\n", pct);

    auto end = Clock::now();
    auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    printf("\nElapsed time: %.3f ms (%.0f µs)\n", elapsed_us / 1000.0,
           (double)elapsed_us);

    return 0;
}
