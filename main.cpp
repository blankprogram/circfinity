#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace exprgen {

using uint128 = unsigned __int128;

static constexpr int MAX_DEPTH = 30;
static constexpr const char *OP_NAMES[3] = {"AND", "OR", "XOR"};

struct Node {
    enum Kind { LEAF, NOT, OP } kind;
    int left, right;
    int op;
    Node(Kind k = LEAF, int l = -1, int r = -1, int o = -1)
        : kind(k), left(l), right(r), op(o) {}
};

static std::vector<Node> nodes;
static uint128 shapeCountTbl[MAX_DEPTH + 1];
static bool shapeCountInit[MAX_DEPTH + 1];

inline std::string varName(int idx) { return std::string(1, char('A' + idx)); }

uint128 countShapes(int d) {
    if (d < 0)
        return 0;
    if (shapeCountInit[d])
        return shapeCountTbl[d];

    uint128 total = (d == 0 ? 1 : 0);
    if (d > 0) {
        total += countShapes(d - 1);
        for (int d1 = 0; d1 < d; ++d1) {
            int d2 = d - 1 - d1;
            uint128 c1 = countShapes(d1), c2 = countShapes(d2);
            uint128 block = (d1 < d2 ? c1 * c2 : c1 * (c1 + 1) / 2);
            total += block * 3;
        }
    }
    shapeCountInit[d] = true;
    return shapeCountTbl[d] = total;
}

int buildShape(uint128 idx, int d) {
    if (d == 0) {
        nodes.emplace_back(Node::LEAF);
        return int(nodes.size()) - 1;
    }
    uint128 ncount = countShapes(d - 1);
    if (idx < ncount) {
        int c = buildShape(idx, d - 1);
        nodes.emplace_back(Node::NOT, c);
        return int(nodes.size()) - 1;
    }
    idx -= ncount;
    for (int op = 0; op < 3; ++op) {
        for (int d1 = 0; d1 < d; ++d1) {
            int d2 = d - 1 - d1;
            uint128 c1 = countShapes(d1), c2 = countShapes(d2);
            uint128 block = (d1 < d2 ? c1 * c2 : c1 * (c1 + 1) / 2);
            if (idx < block) {
                int i1, i2;
                if (d1 < d2) {
                    i1 = int(idx / c2);
                    i2 = int(idx % c2);
                } else {
                    // combinations i ≤ j
                    uint128 p = idx;
                    int i = 0;
                    while (p >= (c1 - i)) {
                        p -= (c1 - i);
                        ++i;
                    }
                    i1 = i;
                    i2 = i + int(p);
                }
                int L = buildShape(i1, d1);
                int R = buildShape(i2, d2);
                nodes.emplace_back(Node::OP, L, R, op);
                return int(nodes.size()) - 1;
            }
            idx -= block;
        }
    }
    throw std::out_of_range("buildShape: index overflow");
}

int countLeaves(int id) {
    const Node &n = nodes[id];
    if (n.kind == Node::LEAF)
        return 1;
    if (n.kind == Node::NOT)
        return countLeaves(n.left);
    return countLeaves(n.left) + countLeaves(n.right);
}

// 5) Canonical surjection patterns
void generatePatterns(int k, int maxVar, std::vector<std::vector<int>> &out) {
    std::vector<int> pat(k);
    std::function<void(int, int, int)> dfs = [&](int i, int used, int m) {
        if (i == k) {
            if (used == m)
                out.push_back(pat);
            return;
        }
        int lim = (used < m ? used + 1 : m);
        for (int v = 0; v < lim; ++v) {
            pat[i] = v;
            dfs(i + 1, std::max(used, v + 1), m);
        }
    };
    for (int m = 1; m <= std::min(k, maxVar); ++m) {
        dfs(0, 0, m);
    }
}

std::string emitPatterned(int id, const std::vector<int> &pat, int &p) {
    const Node &n = nodes[id];
    if (n.kind == Node::LEAF) {
        return varName(pat[p++]);
    }
    if (n.kind == Node::NOT) {
        return "NOT(" + emitPatterned(n.left, pat, p) + ")";
    }
    std::string L = emitPatterned(n.left, pat, p);
    std::string R = emitPatterned(n.right, pat, p);
    if (L > R)
        std::swap(L, R);
    return std::string(OP_NAMES[n.op]) + "(" + L + "," + R + ")";
}

std::string generateNthCanonicalExpr(uint128 n, int maxVar) {
    std::unordered_set<std::string> seen;
    for (int d = 0; d <= MAX_DEPTH; ++d) {
        uint128 sc = countShapes(d);
        for (uint128 s = 0; s < sc; ++s) {
            nodes.clear();
            buildShape(s, d);
            int leaves = countLeaves(int(nodes.size()) - 1);

            std::vector<std::vector<int>> patterns;
            generatePatterns(leaves, maxVar, patterns);

            for (auto &pat : patterns) {
                int p = 0;
                std::string expr = emitPatterned(int(nodes.size()) - 1, pat, p);
                // skip if we've already emitted this exact string
                if (seen.insert(expr).second) {
                    if (n == 0)
                        return expr;
                    --n;
                }
            }
        }
    }
    throw std::out_of_range("n too large");
}

} // namespace exprgen

int main() {
    constexpr int MAXVAR = 5;
    constexpr unsigned long long N = 200;

    std::unordered_set<std::string> seen;
    for (unsigned long long i = 0; i < N; ++i) {
        auto e = exprgen::generateNthCanonicalExpr((exprgen::uint128)i, MAXVAR);
        if (!seen.insert(e).second) {
            std::cerr << "Duplicate detected: " << e << "\n";
            break;
        }
        std::cout << "Unique Expr #" << (i + 1) << ": " << e << "\n";
    }
}
