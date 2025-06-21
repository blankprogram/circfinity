#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

using BigInt = boost::multiprecision::cpp_int;

constexpr int MAX_DEPTH = 8;
constexpr int MAX_VAR = 5;

std::vector<BigInt> shapes(MAX_DEPTH + 1);
std::vector<std::vector<int>> leafCount;
std::vector<std::vector<std::vector<uint64_t>>> C;
std::vector<std::vector<uint64_t>> B;
std::vector<uint64_t> P(MAX_DEPTH + 2);
std::vector<std::vector<BigInt>> stirling;
std::vector<std::vector<std::vector<uint64_t>>> patternDP;

std::vector<int> flatTrees;
std::vector<std::tuple<int, int, int>> treeIndex;

struct Node {
    int kind, l, r, op;
};
std::vector<Node> nodes;

BigInt countShapes(int d) {
    static std::vector<BigInt> cache(MAX_DEPTH + 1, -1);
    if (d < 0)
        return 0;
    if (cache[d] != -1)
        return cache[d];
    BigInt total = (d == 0 ? 1 : 0);
    if (d > 0) {
        total += countShapes(d - 1);
        for (int d1 = 0; d1 <= d - 1; ++d1) {
            int d2 = d - 1 - d1;
            if (d1 > d2)
                continue;
            BigInt c1 = countShapes(d1);
            BigInt c2 = countShapes(d2);
            BigInt block;
            if (d1 < d2) {
                block = (c1 * c2 + 1) / 2;
            } else {
                block = (c1 * (c1 + 1)) / 2;
            }
            total += block * 3;
        }
    }
    return cache[d] = total;
}

int buildShape(const BigInt &idx, int d) {
    if (d == 0) {
        nodes.push_back({0, 0, 0, 0});
        return nodes.size() - 1;
    }
    BigInt rem = idx;
    BigInt notCount = countShapes(d - 1);
    if (rem < notCount) {
        int c = buildShape(rem, d - 1);
        nodes.push_back({1, c, 0, 0});
        return nodes.size() - 1;
    }
    rem -= notCount;

    for (int op = 0; op < 3; ++op) {
        for (int d1 = 0; d1 <= d - 1; ++d1) {
            int d2 = d - 1 - d1;
            if (d1 > d2)
                continue;

            BigInt c1 = countShapes(d1);
            BigInt c2 = countShapes(d2);
            BigInt block;
            if (d1 < d2) {
                block = (c1 * c2 + 1) / 2;
                if (rem < block) {
                    BigInt p = rem;
                    int i1 = 0;
                    while (p >= (c2 - i1)) {
                        p -= (c2 - i1);
                        ++i1;
                    }
                    int i2 = i1 + p.convert_to<int>();
                    int L = buildShape(i1, d1);
                    int R = buildShape(i2, d2);
                    if (L > R)
                        std::swap(L, R);
                    nodes.push_back({2, L, R, op});
                    return nodes.size() - 1;
                }
                rem -= block;
            } else {
                block = (c1 * (c1 + 1)) / 2;
                if (rem < block) {
                    BigInt p = rem;
                    int i = 0;
                    while (p >= (c1 - i)) {
                        p -= (c1 - i);
                        ++i;
                    }
                    int i1 = i;
                    int i2 = i + p.convert_to<int>();
                    int L = buildShape(i1, d1);
                    int R = buildShape(i2, d2);
                    if (L > R)
                        std::swap(L, R);
                    nodes.push_back({2, L, R, op});
                    return nodes.size() - 1;
                }
                rem -= block;
            }
        }
    }
    throw std::out_of_range("buildShape overflow");
}

int countLeaves(int id) {
    const auto &n = nodes[id];
    if (n.kind == 0)
        return 1;
    if (n.kind == 1)
        return countLeaves(n.l);
    return countLeaves(n.l) + countLeaves(n.r);
}

void precomputeStirling(int ML, int MV) {
    stirling.assign(ML + 1, std::vector<BigInt>(MV + 1, 0));
    stirling[0][0] = 1;
    for (int k = 1; k <= ML; ++k)
        for (int m = 1; m <= MV; ++m)
            stirling[k][m] =
                BigInt(m) * stirling[k - 1][m] + stirling[k - 1][m - 1];
}

void precomputePatternDP(int ML, int MV) {
    patternDP.resize(ML + 1);
    for (int k = 0; k <= ML; ++k) {
        patternDP[k].resize(MV + 1);
        for (int m = 1; m <= MV; ++m) {
            std::vector<uint64_t> dp((k + 1) * (MV + 1), 0);
            auto Pdp = [&](int i, int u) -> uint64_t & {
                return dp[i * (MV + 1) + u];
            };
            for (int u = 0; u <= MV; ++u)
                Pdp(k, u) = (u == m ? 1 : 0);
            for (int i = k - 1; i >= 0; --i)
                for (int u = 0; u <= m; ++u)
                    Pdp(i, u) =
                        u * Pdp(i + 1, u) + (u < m ? Pdp(i + 1, u + 1) : 0);
            patternDP[k][m] = dp;
        }
    }
}

int main() {
    for (int d = 0; d <= MAX_DEPTH; ++d)
        shapes[d] = countShapes(d);

    leafCount.resize(MAX_DEPTH + 1);
    B.resize(MAX_DEPTH + 1);
    C.resize(MAX_DEPTH + 1);

    int ML = 0;
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        size_t S = shapes[d].convert_to<size_t>();
        leafCount[d].resize(S);
        B[d].resize(S + 1);
        B[d][0] = 0;
        for (size_t s = 0; s < S; ++s) {
            nodes.clear();
            int root = buildShape(BigInt(s), d);
            int k = countLeaves(root);
            leafCount[d][s] = k;
            ML = std::max(ML, k);

            int offset = flatTrees.size();
            flatTrees.push_back(nodes.size());
            for (const auto &n : nodes)
                flatTrees.push_back(n.kind), flatTrees.push_back(n.l),
                    flatTrees.push_back(n.r), flatTrees.push_back(n.op);
            treeIndex.emplace_back(d, int(s), offset);
        }
    }

    precomputeStirling(ML, MAX_VAR);
    precomputePatternDP(ML, MAX_VAR);

    P[0] = 0;
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        size_t S = shapes[d].convert_to<size_t>();
        C[d].resize(S, std::vector<uint64_t>(MAX_VAR + 1, 0));
        for (size_t s = 0; s < S; ++s) {
            int k = leafCount[d][s];
            uint64_t blk = 0;
            for (int m = 1; m <= MAX_VAR; ++m) {
                uint64_t val = stirling[k][m].convert_to<uint64_t>();
                C[d][s][m] = val;
                blk += val;
            }
            B[d][s + 1] = B[d][s] + blk;
        }
        P[d + 1] = P[d] + B[d][S];
    }

    std::ofstream out("tables.bin", std::ios::binary);
    size_t Psize = P.size();
    out.write((char *)&Psize, sizeof(size_t));
    out.write((char *)P.data(), sizeof(uint64_t) * Psize);

    for (const auto &row : B) {
        size_t R = row.size();
        out.write((char *)&R, sizeof(size_t));
        out.write((char *)row.data(), sizeof(uint64_t) * R);
    }

    for (const auto &mat : C) {
        size_t M = mat.size();
        out.write((char *)&M, sizeof(size_t));
        for (const auto &row : mat) {
            size_t R = row.size();
            out.write((char *)&R, sizeof(size_t));
            out.write((char *)row.data(), sizeof(uint64_t) * R);
        }
    }

    size_t pdpSize = patternDP.size();
    out.write((char *)&pdpSize, sizeof(size_t));
    for (const auto &pk : patternDP) {
        size_t M = pk.size();
        out.write((char *)&M, sizeof(size_t));
        for (const auto &pm : pk) {
            size_t R = pm.size();
            out.write((char *)&R, sizeof(size_t));
            out.write((char *)pm.data(), sizeof(uint64_t) * R);
        }
    }

    size_t flatSize = flatTrees.size();
    out.write((char *)&flatSize, sizeof(size_t));
    out.write((char *)flatTrees.data(), sizeof(int) * flatSize);

    size_t indexSize = treeIndex.size();
    out.write((char *)&indexSize, sizeof(size_t));
    for (const auto &[d, s, offset] : treeIndex) {
        out.write((char *)&d, sizeof(int));
        out.write((char *)&s, sizeof(int));
        out.write((char *)&offset, sizeof(int));
    }

    for (const auto &row : leafCount) {
        size_t R = row.size();
        out.write((char *)&R, sizeof(size_t));
        out.write((char *)row.data(), sizeof(int) * R);
    }

    out.close();
    std::cout << "✅ Precompute done → tables.bin\n";
    return 0;
}
