#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ─────────────────────────────────────────────
// Deterministic Random-Access Boolean Expression Generator
// Modern C++23 version, optimised and well-commented
// ─────────────────────────────────────────────

using u64 = uint64_t;
constexpr int MAX_SIZE = 25;

// ─────────────────────────────────────────────
// Compute Bell numbers at compile time.
// Each Bell number counts how many ways you can label leaves (via RGS).
// ─────────────────────────────────────────────

consteval auto make_bell() {
    std::array<u64, MAX_SIZE + 2> B{}, p{}, r{};
    B[0] = p[0] = 1;
    for (int n = 1; n <= MAX_SIZE + 1; ++n) {
        r[0] = p[n - 1];
        for (int k = 1; k <= n; ++k)
            r[k] = r[k - 1] + p[k - 1];
        B[n] = r[0];
        std::copy(r.begin(), r.end(), p.begin());
    }
    return B;
}
constexpr auto Bell = make_bell();

// ─────────────────────────────────────────────
// Precompute powers of 3 at compile time.
// There are 3 choices for each binary operator: AND, OR, XOR.
// ─────────────────────────────────────────────

consteval auto make_pow3() {
    std::array<u64, MAX_SIZE + 1> P{};
    P[0] = 1;
    for (int i = 1; i <= MAX_SIZE; ++i)
        P[i] = P[i - 1] * 3;
    return P;
}
constexpr auto Pow3 = make_pow3();

// ─────────────────────────────────────────────
// WeightFactor for a given number of binary operators.
// It equals Bell[b+1] * 3^b.
// This counts all ways to label leaves and choose operators for a shape.
// ─────────────────────────────────────────────

constexpr auto WeightFactor = [] {
    std::array<u64, MAX_SIZE + 1> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        W[b] = Bell[b + 1] * Pow3[b];
    return W;
}();

// ─────────────────────────────────────────────
// Dynamic programming table C[s][b].
// C[s][b] = number of possible shapes of total size s with exactly b binary
// ops. Shape means the tree structure ignoring actual variable labels and
// operators. ─────────────────────────────────────────────

std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> C;

void compute_C() {
    C[1].fill(0);
    // A single leaf has size 1 and 0 binary ops.
    C[1][0] = 1;
    for (int s = 2; s <= MAX_SIZE; ++s) {
        C[s].fill(0);
        // Binary-root shapes: split left/right, each subtree must be valid.
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (int b1 = 0; b1 <= ls; ++b1) {
                u64 c1 = C[ls][b1];
                if (!c1)
                    continue;
                for (int b2 = 0; b2 <= rs; ++b2) {
                    u64 c2 = C[rs][b2];
                    if (!c2)
                        continue;
                    // +1 for root binary op
                    C[s][b1 + b2 + 1] += c1 * c2;
                }
            }
        }
        // Unary-root shapes: wrap a subtree with a NOT
        for (int b = 0; b <= s - 1; ++b)
            C[s][b] += C[s - 1][b];
    }
}

// ─────────────────────────────────────────────
// Precompute weights needed for random-access unranking.
// These are used to navigate efficiently when choosing shape splits.
// ─────────────────────────────────────────────

std::array<u64, MAX_SIZE + 1> shapeCount, shapeWeight, cumShapeWeight;
std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> blockWeight;
std::array<std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1>,
           MAX_SIZE + 1>
    rowWeightSum;

void build_tables() {
    compute_C();
    cumShapeWeight[0] = 0;
    for (int s = 1; s <= MAX_SIZE; ++s) {
        u64 sc = 0, sw = 0;
        for (int b = 0; b <= MAX_SIZE; ++b) {
            sc += C[s][b];
            sw += C[s][b] * WeightFactor[b];
        }
        shapeCount[s] = sc;
        shapeWeight[s] = sw;
        cumShapeWeight[s] = cumShapeWeight[s - 1] + sw;
    }

    // For binary-root shapes: precompute the weight for each block and row
    for (int s = 2; s <= MAX_SIZE; ++s) {
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            u64 bw = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                u64 cntL = C[ls][b1];
                if (!cntL)
                    continue;
                u64 rowsum = 0;
                for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                    u64 cntR = C[rs][b2];
                    if (!cntR)
                        continue;
                    rowsum += cntR * WeightFactor[b1 + b2 + 1];
                }
                rowWeightSum[s][ls][b1] = rowsum;
                bw += cntL * rowsum;
            }
            blockWeight[s][ls] = bw;
        }
    }
}

// ─────────────────────────────────────────────
// DP table for Restricted Growth Strings (RGS).
// An RGS enumerates unique ways to assign variables to leaves.
// ─────────────────────────────────────────────

std::array<std::array<u64, MAX_SIZE + 1>, MAX_SIZE + 1> DP_RGS;

void init_rgs_dp() {
    // Base case: empty RGS is 1 way
    for (int k = 0; k <= MAX_SIZE; ++k)
        DP_RGS[0][k] = 1;

    // Fill RGS DP
    for (int len = 1; len <= MAX_SIZE; ++len) {
        for (int k = 0; k <= MAX_SIZE; ++k) {
            u64 sum = 0;
            for (int v = 0; v <= k + 1; ++v) {
                int nk = std::max(v, k);
                sum += DP_RGS[len - 1][nk];
            }
            DP_RGS[len][k] = sum;
        }
    }
}

// ─────────────────────────────────────────────
// Recursively build the final prefix notation string.
// Uses pre-allocated OUT buffer for performance.
// ─────────────────────────────────────────────

constexpr size_t OUT_BUF_SIZE = 1 << 16;
char OUT[OUT_BUF_SIZE];

void build_expr(int s, u64 idx, const std::vector<int> &ops,
                const std::vector<int> &rgs, int &leafIdx, int &opIdx,
                int &OL) {
    if (s == 1) {
        // It's a leaf: just print the variable.
        OUT[OL++] = char('A' + rgs[leafIdx++]);
        return;
    }

    // Determine if shape is binary-root or unary-root
    u64 binCount = shapeCount[s] - shapeCount[s - 1];
    if (idx < binCount) {
        // Binary-root: figure out which split.
        u64 acc = 0;
        int choice = 1;
        for (int ls = 1; ls <= s - 2; ++ls) {
            u64 bs = shapeCount[ls] * shapeCount[s - 1 - ls];
            if (idx < acc + bs) {
                idx -= acc;
                choice = ls;
                break;
            }
            acc += bs;
        }
        int ls = choice, rs = s - 1 - ls;
        u64 i = idx / shapeCount[rs];
        u64 j = idx % shapeCount[rs];

        // Write operator
        int op = ops[opIdx++];
        const char *lit = op == 0 ? "AND" : op == 1 ? "OR" : "XOR";
        int L = op == 1 ? 2 : 3;
        std::memcpy(OUT + OL, lit, L);
        OL += L;

        OUT[OL++] = '(';
        build_expr(ls, i, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ',';
        build_expr(rs, j, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
    } else {
        // Unary-root: wrap child with NOT.
        std::memcpy(OUT + OL, "NOT(", 4);
        OL += 4;
        build_expr(s - 1, idx - binCount, ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
    }
}

// ─────────────────────────────────────────────
// Given a flat global index, decode the shape,
// then decode which operator sequence and RGS labels it has.
// ─────────────────────────────────────────────

u64 shape_unrank(int s, u64 woff, int &b_shape, u64 &variantOff) {
    if (s == 1) {
        b_shape = 0;
        variantOff = woff;
        return 0;
    }

    u64 binTotal = shapeCount[s] - shapeCount[s - 1];
    u64 acc = 0;

    // Try binary-root blocks
    for (int ls = 1; ls <= s - 2; ++ls) {
        u64 bw = blockWeight[s][ls];
        if (woff < acc + bw) {
            u64 offB = woff - acc;
            int rs = s - 1 - ls;

            // Walk row & column to find left/right subtree shapes
            u64 rowAcc = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                u64 cntL = C[ls][b1];
                if (!cntL)
                    continue;
                u64 rowW = rowWeightSum[s][ls][b1];
                u64 totW = cntL * rowW;
                if (offB < rowAcc + totW) {
                    u64 offG = offB - rowAcc;
                    u64 i = offG / rowW;
                    u64 offR = offG % rowW;

                    u64 colAcc = 0;
                    for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                        u64 cntR = C[rs][b2];
                        if (!cntR)
                            continue;
                        u64 cellW = WeightFactor[b1 + b2 + 1];
                        u64 totC = cntR * cellW;
                        if (offR < colAcc + totC) {
                            u64 off2 = offR - colAcc;
                            u64 j = off2 / cellW;
                            variantOff = off2 % cellW;
                            b_shape = b1 + b2 + 1;

                            // Compute final shape index for binary-root
                            u64 base = 0;
                            for (int x = 1; x < ls; ++x)
                                base += shapeCount[x] * shapeCount[s - 1 - x];
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

    // If not binary-root, it must be unary-root.
    u64 childIdx = shape_unrank(s - 1, woff - acc, b_shape, variantOff);
    return binTotal + childIdx;
}

std::string unrank(u64 N) {
    assert(N >= 1 && N <= cumShapeWeight[MAX_SIZE]);

    // Find which shape size contains N
    int s = 1;
    while (cumShapeWeight[s] < N)
        ++s;
    u64 layerOff = N - (cumShapeWeight[s - 1] + 1);

    // Extract binary op count and variant offset
    int b_shape = 0;
    u64 variantOff = 0;
    u64 shapeIdx = shape_unrank(s, layerOff, b_shape, variantOff);

    // The variantOff is split: operator sequence and RGS leaf labeling
    u64 nVar = Bell[b_shape + 1];
    u64 opIndex = variantOff / nVar;
    u64 varIndex = variantOff % nVar;

    // Decode operators as base-3 digits
    std::vector<int> ops(b_shape);
    for (int i = b_shape - 1; i >= 0; --i) {
        ops[i] = static_cast<int>(opIndex % 3);
        opIndex /= 3;
    }

    // Decode RGS: which variables are reused
    std::vector<int> rgs(b_shape + 1);
    rgs[0] = 0;
    int maxSeen = 0;
    u64 rem = varIndex;
    for (int pos = 1; pos <= b_shape; ++pos) {
        int tail = b_shape - pos;
        for (int v = 0; v <= maxSeen + 1; ++v) {
            int nk = std::max(v, maxSeen);
            u64 cnt = DP_RGS[tail][nk];
            if (rem < cnt) {
                rgs[pos] = v;
                maxSeen = nk;
                break;
            }
            rem -= cnt;
        }
    }

    // Build final string in OUT buffer
    int OL = 0, leafIdx = 0, opIdx = 0;
    build_expr(s, shapeIdx, ops, rgs, leafIdx, opIdx, OL);
    return std::string(OUT, OUT + OL);
}

int main() {
    build_tables();
    init_rgs_dp();
    u64 total = cumShapeWeight[MAX_SIZE];
    u64 toPrint = std::min<u64>(100, total);
    for (u64 i = 1; i <= toPrint; ++i)
        printf("#%llu: %s\n", static_cast<unsigned long long>(i),
               unrank(i).c_str());
    printf("#%llu: %s\n", static_cast<unsigned long long>(total),
           unrank(total).c_str());
    printf("Total expressions: %llu\n", static_cast<unsigned long long>(total));
    return 0;
}
