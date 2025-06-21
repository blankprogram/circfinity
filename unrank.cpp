#include <algorithm>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

int maxVar;
std::vector<uint64_t> P;
std::vector<std::vector<uint64_t>> B;
std::vector<std::vector<std::vector<uint64_t>>> C;
std::vector<std::vector<std::vector<uint64_t>>> patternDP;

std::vector<int> flatTrees;
std::vector<std::tuple<int, int, int>> treeIndex;
std::vector<std::vector<int>> leafCount;

struct Node {
    int kind, l, r, op;
};
std::vector<Node> nodes;

constexpr const char *OP_NAMES[3] = {"AND", "OR", "XOR"};

std::string emitPatterned(int id, const std::vector<int> &pat, int &p) {
    const auto &n = nodes[id];
    if (n.kind == 0)
        return std::string(1, 'A' + pat[p++]);
    if (n.kind == 1)
        return "NOT(" + emitPatterned(n.l, pat, p) + ")";
    std::string L = emitPatterned(n.l, pat, p);
    std::string R = emitPatterned(n.r, pat, p);
    if (L > R)
        std::swap(L, R);
    return std::string(OP_NAMES[n.op]) + "(" + L + "," + R + ")";
}

std::string generateNthCanonicalExpr(uint64_t n) {
    auto it = std::upper_bound(P.begin(), P.end(), n);
    int d = int(it - P.begin()) - 1;
    n -= P[d];

    auto bIt = std::upper_bound(B[d].begin(), B[d].end(), n);
    size_t sIdx = std::distance(B[d].begin(), bIt) - 1;
    n -= B[d][sIdx];

    int k = leafCount[d][sIdx];
    int m = 1;
    uint64_t acc = 0;
    for (; m <= maxVar; ++m) {
        uint64_t c = C[d][sIdx][m];
        if (n < acc + c)
            break;
        acc += c;
    }
    n -= acc;

    const uint64_t *DP = patternDP[k][m].data();
    auto Pdp = [&](int i, int u) { return DP[i * (maxVar + 1) + u]; };

    static thread_local std::vector<int> pat;
    pat.resize(k);
    int used = 0;
    for (int i = 0; i < k; ++i) {
        int lim = (used < m) ? (used + 1) : used;
        for (int v = 0; v < lim; ++v) {
            uint64_t cnt = (v < used) ? Pdp(i + 1, used) : Pdp(i + 1, used + 1);
            if (n < cnt) {
                pat[i] = v;
                if (v == used)
                    ++used;
                break;
            }
            n -= cnt;
        }
    }

    int offset = -1;
    for (const auto &[td, ts, off] : treeIndex) {
        if (td == d && ts == int(sIdx)) {
            offset = off;
            break;
        }
    }
    if (offset < 0)
        throw std::runtime_error("Tree index not found!");

    int size = flatTrees[offset];
    nodes.resize(size);
    for (int i = 0; i < size; ++i) {
        nodes[i] = {flatTrees[offset + 1 + 4 * i + 0],
                    flatTrees[offset + 1 + 4 * i + 1],
                    flatTrees[offset + 1 + 4 * i + 2],
                    flatTrees[offset + 1 + 4 * i + 3]};
    }

    int p = 0;
    return emitPatterned(size - 1, pat, p);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <n>\n";
        return 1;
    }

    std::ifstream in("tables.bin", std::ios::binary);
    if (!in) {
        std::cerr << "Error: cannot open tables.bin\n";
        return 1;
    }

    size_t Psize;
    in.read((char *)&Psize, sizeof(size_t));
    P.resize(Psize);
    in.read((char *)P.data(), sizeof(uint64_t) * Psize);

    B.resize(Psize - 1);
    for (auto &row : B) {
        size_t R;
        in.read((char *)&R, sizeof(size_t));
        row.resize(R);
        in.read((char *)row.data(), sizeof(uint64_t) * R);
    }

    C.resize(Psize - 1);
    for (auto &mat : C) {
        size_t M;
        in.read((char *)&M, sizeof(size_t));
        mat.resize(M);
        for (auto &row : mat) {
            size_t R;
            in.read((char *)&R, sizeof(size_t));
            row.resize(R);
            in.read((char *)row.data(), sizeof(uint64_t) * R);
        }
    }

    size_t pdpSize;
    in.read((char *)&pdpSize, sizeof(size_t));
    patternDP.resize(pdpSize);
    for (auto &pk : patternDP) {
        size_t M;
        in.read((char *)&M, sizeof(size_t));
        pk.resize(M);
        for (auto &pm : pk) {
            size_t R;
            in.read((char *)&R, sizeof(size_t));
            pm.resize(R);
            in.read((char *)pm.data(), sizeof(uint64_t) * R);
        }
    }

    size_t flatSize;
    in.read((char *)&flatSize, sizeof(size_t));
    flatTrees.resize(flatSize);
    in.read((char *)flatTrees.data(), sizeof(int) * flatSize);

    size_t indexSize;
    in.read((char *)&indexSize, sizeof(size_t));
    treeIndex.resize(indexSize);
    for (auto &[d, s, offset] : treeIndex) {
        in.read((char *)&d, sizeof(int));
        in.read((char *)&s, sizeof(int));
        in.read((char *)&offset, sizeof(int));
    }

    leafCount.resize(Psize - 1);
    for (auto &row : leafCount) {
        size_t R;
        in.read((char *)&R, sizeof(size_t));
        row.resize(R);
        in.read((char *)row.data(), sizeof(int) * R);
    }

    in.close();
    maxVar = int(C[0][0].size()) - 1;

    uint64_t n = std::stoull(argv[1]);
    std::cout << "Expr #" << n << ": " << generateNthCanonicalExpr(n) << "\n";
    return 0;
}
