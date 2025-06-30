#include "compute.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

char OUT[OUT_BUF_SIZE] = {};
std::unique_ptr<PlanNode> build_plan(int s, u128 shapeIdx) {
    auto node = std::make_unique<PlanNode>();
    if (s == 1) {
        node->type = NodeType::Leaf;
        return node;
    }
    u128 binTotal = u128(shapeCount[s]) - shapeCount[s - 1];
    if (shapeIdx < binTotal) {
        // binary root
        node->type = NodeType::Binary;
        int ls = 1;
        u128 acc = 0;
        for (; ls <= s - 2; ++ls) {
            u128 blockCnt = u128(shapeCount[ls]) * shapeCount[s - 1 - ls];
            if (shapeIdx < acc + blockCnt)
                break;
            acc += blockCnt;
        }
        int rs = s - 1 - ls;
        u128 local = shapeIdx - acc;
        u128 i = local / shapeCount[rs];
        u128 j = local % shapeCount[rs];
        node->ls = ls;
        node->rs = rs;
        node->children.push_back(build_plan(ls, i));
        node->children.push_back(build_plan(rs, j));
    } else {
        // unary NOT
        node->type = NodeType::Unary;
        u128 subIdx = shapeIdx - binTotal;
        node->children.push_back(build_plan(s - 1, subIdx));
    }
    return node;
}

void build_from_plan(const PlanNode *node, const std::vector<int> &ops,
                     const std::vector<int> &rgs, int &leafIdx, int &opIdx,
                     int &OL) {
    switch (node->type) {
    case NodeType::Leaf:
        OUT[OL++] = char('A' + rgs[leafIdx++]);
        break;
    case NodeType::Unary:
        std::memcpy(OUT + OL, "NOT(", 4);
        OL += 4;
        build_from_plan(node->children[0].get(), ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
        break;
    case NodeType::Binary: {
        int op = ops[opIdx++];
        std::string_view opStr = (op == 0 ? "AND" : op == 1 ? "OR" : "XOR");
        std::memcpy(OUT + OL, opStr.data(), opStr.size());
        OL += (int)opStr.size();
        OUT[OL++] = '(';
        build_from_plan(node->children[0].get(), ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ',';
        build_from_plan(node->children[1].get(), ops, rgs, leafIdx, opIdx, OL);
        OUT[OL++] = ')';
        break;
    }
    }
}

static int count_leaves(const PlanNode *n) {
    if (n->type == NodeType::Leaf) {
        return 1;
    } else if (n->type == NodeType::Unary) {
        return count_leaves(n->children[0].get());
    } else { // Binary
        return count_leaves(n->children[0].get()) +
               count_leaves(n->children[1].get());
    }
}

inline u128 variant_weight(int leafCount, int b_shape) {
    return u128(Pow3[b_shape]) * Bell[leafCount];
}

static std::vector<std::vector<std::string>> shapeSig;
void build_signature(int s, u128 shapeIdx, std::string &out) {
    if (s == 1) {
        out += 'L';
        return;
    }
    u128 binTotal = u128(shapeCount[s]) - shapeCount[s - 1];
    if (shapeIdx < binTotal) {
        out += 'B';
        int ls = 1;
        u128 acc = 0;
        for (; ls <= s - 2; ++ls) {
            u128 blockCnt = u128(shapeCount[ls]) * shapeCount[s - 1 - ls];
            if (shapeIdx < acc + blockCnt)
                break;
            acc += blockCnt;
        }
        int rs = s - 1 - ls;
        u128 local = shapeIdx - acc;
        u128 i = local / shapeCount[rs];
        u128 j = local % shapeCount[rs];
        build_signature(ls, i, out);
        build_signature(rs, j, out);
    } else {
        out += 'U';
        u128 subIdx = shapeIdx - binTotal;
        build_signature(s - 1, subIdx, out);
    }
}

int count_B(const std::string &sig) {
    return std::count(sig.begin(), sig.end(), 'B');
}
int count_L(const std::string &sig) {
    return std::count(sig.begin(), sig.end(), 'L');
}

static std::vector<std::vector<int>> B_sig;
static std::vector<std::vector<int>> L_sig;
static std::vector<std::vector<u128>> W_sig;
static std::vector<std::vector<u128>> prefixW;

struct ShapeMetadata {
    int b;
    int l;
    u128 w;
};

static std::vector<std::vector<ShapeMetadata>> shapeMeta;

inline const auto W_lookup = [] {
    std::array<std::array<u128, MAX_SIZE + 1>, MAX_SIZE + 1> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        for (int l = 1; l <= MAX_SIZE; ++l)
            W[b][l] = u128(Pow3[b]) * Bell[l];
    return W;
}();

void precompute_all_layers() {
    shapeMeta.resize(MAX_SIZE + 1);
    prefixW.resize(MAX_SIZE + 1);

    shapeMeta[1].push_back({0, 1, W_lookup[0][1]});
    prefixW[1] = {0, W_lookup[0][1]};

    for (int s = 2; s <= MAX_SIZE; ++s) {
        std::vector<ShapeMetadata> meta;

        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (size_t i = 0; i < shapeMeta[ls].size(); ++i) {
                for (size_t j = 0; j < shapeMeta[rs].size(); ++j) {
                    int b = 1 + shapeMeta[ls][i].b + shapeMeta[rs][j].b;
                    int l = shapeMeta[ls][i].l + shapeMeta[rs][j].l;
                    meta.push_back({b, l, W_lookup[b][l]});
                }
            }
        }

        for (const auto &sm : shapeMeta[s - 1]) {
            meta.push_back({sm.b, sm.l, W_lookup[sm.b][sm.l]});
        }

        shapeMeta[s] = std::move(meta);
        prefixW[s].resize(shapeMeta[s].size() + 1);
        for (size_t i = 0; i < shapeMeta[s].size(); ++i)
            prefixW[s][i + 1] = prefixW[s][i] + shapeMeta[s][i].w;
    }
}

u128 shape_unrank(int s, u128 woff, int &b_shape, u128 &variantOff) {
    const auto &P = prefixW[s];
    auto it = std::upper_bound(P.begin(), P.end(), woff);
    int idx = int((it - P.begin()) - 1);

    variantOff = woff - P[idx];
    b_shape = shapeMeta[s][idx].b;
    return idx;
}

std::string unrank(u128 N) {
    assert(N >= 1 && N <= cumShapeWeight[MAX_SIZE]);

    int s = 1;
    while (cumShapeWeight[s] < N)
        ++s;
    u128 layerOff = N - (cumShapeWeight[s - 1] + 1);

    int b_shape = 0;
    u128 variantOff = 0;
    u128 shapeIdx = shape_unrank(s, layerOff, b_shape, variantOff);

    auto plan = build_plan(s, shapeIdx);
    int leafCount = count_leaves(plan.get());

    u128 labelCount = Bell[leafCount];
    u128 opIndex = variantOff / labelCount;
    u128 labIndex = variantOff % labelCount;

    std::vector<int> ops(b_shape);
    for (int i = b_shape - 1; i >= 0; --i) {
        ops[i] = int(opIndex % 3);
        opIndex /= 3;
    }

    std::vector<int> rgs(leafCount);
    rgs[0] = 0;
    int maxSeen = 0;
    u128 rem = labIndex;
    for (int pos = 1; pos < leafCount; ++pos) {
        int tail = leafCount - pos - 1;
        for (int v = 0; v <= maxSeen + 1; ++v) {
            int nk = std::max(v, maxSeen);
            u128 cnt = DP_RGS[tail][nk];
            if (rem < cnt) {
                rgs[pos] = v;
                maxSeen = nk;
                break;
            }
            rem -= cnt;
        }
    }

    int OL = 0, leafIdx = 0, opIdxInner = 0;
    build_from_plan(plan.get(), ops, rgs, leafIdx, opIdxInner, OL);
    return std::string(OUT, OUT + OL);
}
int main() {

    using Clock = std::chrono::high_resolution_clock;
    auto pre_start = Clock::now();
    precompute_all_layers();
    auto pre_end = Clock::now();
    auto run_start = Clock::now();

    std::vector<std::string> exprs;
    const u128 total = cumShapeWeight[MAX_SIZE];

    for (u128 i = 1; i <= 100; ++i) {
        std::string expr = unrank(i);
        exprs.push_back(expr);
        printf("#%s: %s\n", i.to_string().c_str(), expr.c_str());
    }
    u128 firstDeep = cumShapeWeight[MAX_SIZE - 1] + 1;
    u128 lastDeep = cumShapeWeight[MAX_SIZE];
    std::string exprFirstDeep = unrank(firstDeep);
    std::string exprLastDeep = unrank(lastDeep);
    printf("First expression at deepest layer (%d leaves) (#%s): %s\n",
           MAX_SIZE, firstDeep.to_string().c_str(), exprFirstDeep.c_str());

    printf("Last  expression at deepest layer (#%s): %s\n",
           lastDeep.to_string().c_str(), exprLastDeep.c_str());
    // 2) Gather duplicates.
    std::unordered_map<std::string, std::vector<u128>> occurrences;
    for (u128 i = 0; i < exprs.size(); ++i)
        occurrences[exprs[static_cast<size_t>(i.low)]].push_back(i + 1);

    bool has_duplicates = false;
    for (auto &[expr, idx_list] : occurrences) {
        if (idx_list.size() <= 1)
            continue;
        if (!has_duplicates) {
            printf("\nDuplicates found:\n");
            has_duplicates = true;
        }
        printf("  \"%s\" appears %zu times:\n", expr.c_str(), idx_list.size());

        for (u128 idx : idx_list) {
            int s = 1;
            while (cumShapeWeight[s] < idx)
                ++s;
            u128 layerOff = idx - (cumShapeWeight[s - 1] + 1);

            int b_shape = -1;
            u128 variantOff = 0;
            u128 shapeIdx = shape_unrank(s, layerOff, b_shape, variantOff);

            auto plan = build_plan(s, shapeIdx);
            int leafCount = count_leaves(plan.get());

            u128 labelCount = Bell[leafCount];
            u128 opIndex = variantOff / labelCount;
            u128 labIndex = variantOff % labelCount;

            std::vector<int> ops(b_shape);
            for (int i = b_shape - 1; i >= 0; --i) {
                ops[i] = int(opIndex % 3);
                opIndex /= 3;
            }

            std::vector<int> rgs(leafCount);
            rgs[0] = 0;
            int maxSeen = 0;
            u128 rem = labIndex;
            for (int pos = 1; pos < leafCount; ++pos) {
                int tail = leafCount - pos - 1;
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

            printf("    • #%s | s=%d | shapeIdx=%s | b=%d\n",
                   idx.to_string().c_str(), s, shapeIdx.to_string().c_str(),
                   b_shape);

            printf("      ops = ");
            for (int o : ops)
                printf("%d", o);
            printf("\n");

            printf("      rgs = ");
            for (int rv : rgs)
                printf("%d", rv);
            printf("\n");
        }
    }

    if (!has_duplicates) {
        printf("\nDuplicates?: no\n");
    }

    __uint128_t tot128 = ((__uint128_t)total.high << 64) | total.low;
    long double pct = (long double)tot128 / powl(2.0L, 128) * 100.0L;
    printf("\nUsed %.18Lf%% of 128-bit range\n", pct);

    auto run_end = Clock::now();
    auto pre_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      pre_end - pre_start)
                      .count();
    auto run_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      run_end - run_start)
                      .count();
    auto total_us = pre_us + run_us;

    printf("\n--- Timing Breakdown ---\n");
    printf("Precompute: %.3f ms (%.0f µs)\n", pre_us / 1000.0, (double)pre_us);
    printf("Unranking:  %.3f ms (%.0f µs)\n", run_us / 1000.0, (double)run_us);
    printf("Total:      %.3f ms (%.0f µs)\n", total_us / 1000.0,
           (double)total_us);

    return 0;
}
