#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

int maxVar;
std::vector<uint64_t> P;
std::vector<std::vector<uint64_t>> B;
std::vector<std::vector<std::vector<uint64_t>>> C;
std::vector<std::vector<std::vector<uint64_t>>> patternDP;
std::vector<std::vector<std::vector<std::vector<int>>>> trees;
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
    auto t_start = std::chrono::steady_clock::now();

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

    nodes.resize(trees[d][sIdx].size());
    for (size_t i = 0; i < nodes.size(); ++i)
        nodes[i] = {trees[d][sIdx][i][0], trees[d][sIdx][i][1],
                    trees[d][sIdx][i][2], trees[d][sIdx][i][3]};
    int p = 0;

    auto t_end = std::chrono::steady_clock::now();
    std::cerr << "[Time] generateNthCanonicalExpr: "
              << std::chrono::duration<double>(t_end - t_start).count()
              << " sec\n";

    return emitPatterned(nodes.size() - 1, pat, p);
}

int main(int argc, char **argv) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <n>\n";
        return 1;
    }

    auto t1 = clock::now();
    std::ifstream in("tables.bin", std::ios::binary);
    if (!in) {
        std::cerr << "Error: cannot open tables.bin\n";
        return 1;
    }

    auto t2 = clock::now();
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

    auto t3 = clock::now();

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

    auto t4 = clock::now();

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

    auto t5 = clock::now();

    trees.resize(Psize - 1);
    for (auto &td : trees) {
        size_t S;
        in.read((char *)&S, sizeof(size_t));
        td.resize(S);
        for (auto &ts : td) {
            size_t N;
            in.read((char *)&N, sizeof(size_t));
            ts.resize(N);
            for (auto &node : ts) {
                node.resize(4);
                in.read((char *)node.data(), sizeof(int) * 4);
            }
        }
    }

    auto t6 = clock::now();

    leafCount.resize(Psize - 1);
    for (auto &row : leafCount) {
        size_t R;
        in.read((char *)&R, sizeof(size_t));
        row.resize(R);
        in.read((char *)row.data(), sizeof(int) * R);
    }

    in.close();
    auto t7 = clock::now();

    maxVar = int(C[0][0].size()) - 1;

    uint64_t n = std::stoull(argv[1]);

    auto t8 = clock::now();
    std::string result = generateNthCanonicalExpr(n);
    auto t9 = clock::now();

    std::cout << "Expr #" << n << ": " << result << "\n";

    std::cerr << "\n=== Timings ===\n";
    std::cerr << "Open file:   "
              << std::chrono::duration<double>(t2 - t1).count() << " s\n";
    std::cerr << "Read P+B:    "
              << std::chrono::duration<double>(t3 - t2).count() << " s\n";
    std::cerr << "Read C:      "
              << std::chrono::duration<double>(t4 - t3).count() << " s\n";
    std::cerr << "Read patternDP: "
              << std::chrono::duration<double>(t5 - t4).count() << " s\n";
    std::cerr << "Read trees:  "
              << std::chrono::duration<double>(t6 - t5).count() << " s\n";
    std::cerr << "Read leafCount: "
              << std::chrono::duration<double>(t7 - t6).count() << " s\n";
    std::cerr << "Unrank expr: "
              << std::chrono::duration<double>(t9 - t8).count() << " s\n";
    std::cerr << "TOTAL:       "
              << std::chrono::duration<double>(t9 - t0).count() << " s\n";

    return 0;
}
