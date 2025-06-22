#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

static constexpr int MAX_SIZE = 20;

static constexpr std::array<long long, MAX_SIZE + 2> Bell = {
    1LL,              // B0
    1LL,              // B1
    2LL,              // B2
    5LL,              // B3
    15LL,             // B4
    52LL,             // B5
    203LL,            // B6
    877LL,            // B7
    4140LL,           // B8
    21147LL,          // B9
    115975LL,         // B10
    678570LL,         // B11
    4213597LL,        // B12
    27644437LL,       // B13
    190899322LL,      // B14
    1382958545LL,     // B15
    10480142147LL,    // B16
    82864869804LL,    // B17
    682076806159LL,   // B18
    5832742205057LL,  // B19
    51724158235372LL, // B20
    474869816156751LL // B21
};
static constexpr std::array<long long, MAX_SIZE + 1> Pow3 = []() {
    std::array<long long, MAX_SIZE + 1> a{};
    a[0] = 1;
    for (int i = 1; i <= MAX_SIZE; ++i)
        a[i] = a[i - 1] * 3;
    return a;
}();

struct Node {
    uint8_t type;
    uint8_t numBinary;
    uint8_t childSize;
    uint8_t rightSize;
    uint32_t child, left, right;
};

class ExpressionGenerator {
    std::array<std::vector<Node>, MAX_SIZE + 1> nodesPool;
    std::array<std::vector<uint32_t>, MAX_SIZE + 1> roots;
    std::array<std::vector<long long>, MAX_SIZE + 1> weights, prefix;
    std::array<long long, MAX_SIZE + 1> cumulativeTotal;

    mutable std::map<int, std::vector<std::vector<int>>> rgs_cache;
    void generate_rgs(int i, int m, int k, std::vector<int> &cur,
                      std::vector<std::vector<int>> &out) const {
        if (i == k) {
            out.push_back(cur);
            return;
        }
        for (int j = 0; j <= m + 1; ++j) {
            cur.push_back(j);
            generate_rgs(i + 1, std::max(m, j), k, cur, out);
            cur.pop_back();
        }
    }
    std::vector<int> unrank_rgs(int k, long long idx) const {
        if (k == 0)
            return {};
        auto &C = rgs_cache[k];
        if (C.empty()) {
            std::vector<std::vector<int>> all;
            std::vector<int> cur{0};
            generate_rgs(1, 0, k, cur, all);
            C = std::move(all);
        }
        if (idx < 0 || idx >= (long long)C.size())
            return std::vector<int>(k, 0);
        return C[idx];
    }

    std::string build_expression(int s, uint32_t ni,
                                 const std::vector<int> &opd, int &opi,
                                 const std::vector<std::string> &vars,
                                 int &leafi) const {
        auto const &n = nodesPool[s][ni];
        if (n.type == 0) {
            return vars[leafi++];
        }
        if (n.type == 1) {
            auto c =
                build_expression(n.childSize, n.child, opd, opi, vars, leafi);
            return "NOT(" + c + ")";
        }
        int d = opd[opi++];
        const char *opn = d == 0 ? "AND" : d == 1 ? "OR" : "XOR";
        auto L = build_expression(n.childSize, n.left, opd, opi, vars, leafi);
        auto R = build_expression(n.rightSize, n.right, opd, opi, vars, leafi);
        return std::string(opn) + "(" + L + "," + R + ")";
    }

  public:
    ExpressionGenerator() {
        std::ifstream in("precomputed_data.bin", std::ios::binary);
        if (!in) {
            std::cerr << "Cannot open data\n";
            std::exit(1);
        }

        prefix[0].clear();
        for (int s = 1; s <= MAX_SIZE; ++s) {
            uint32_t nodeCount;
            in.read((char *)&nodeCount, sizeof(nodeCount));
            nodesPool[s].resize(nodeCount);
            in.read((char *)nodesPool[s].data(), nodeCount * sizeof(Node));

            uint32_t shapeCount;
            in.read((char *)&shapeCount, sizeof(shapeCount));
            roots[s].resize(shapeCount);
            in.read((char *)roots[s].data(), shapeCount * sizeof(uint32_t));

            weights[s].resize(shapeCount);
            in.read((char *)weights[s].data(), shapeCount * sizeof(long long));

            prefix[s].resize(shapeCount);
            std::inclusive_scan(weights[s].begin(), weights[s].end(),
                                prefix[s].begin());
        }

        cumulativeTotal[0] = 0;
        for (int s = 1; s <= MAX_SIZE; ++s) {
            in.read((char *)&cumulativeTotal[s], sizeof(long long));
        }
    }

    long long maxIndex() const { return cumulativeTotal[MAX_SIZE]; }

    std::string get_expression(long long n) const {
        if (n < 1 || n > maxIndex())
            return "ERROR";

        int s = int(std::upper_bound(cumulativeTotal.begin() + 1,
                                     cumulativeTotal.begin() + MAX_SIZE + 1,
                                     n - 1) -
                    cumulativeTotal.begin());
        long long base = cumulativeTotal[s - 1];
        long long offset = n - (base + 1);

        auto const &P = prefix[s];
        auto it = std::lower_bound(P.begin(), P.end(), offset + 1);
        int idxShape = int(it - P.begin());
        long long prev = idxShape ? P[idxShape - 1] : 0;
        long long residual = offset - prev;

        uint32_t rootIdx = roots[s][idxShape];
        auto const &rt = nodesPool[s][rootIdx];
        int b = rt.numBinary;

        long long varCount = Bell[b + 1];
        long long op_index = residual / varCount;
        long long var_index = residual % varCount;

        std::vector<int> opd(b);
        for (int i = b - 1; i >= 0; --i) {
            opd[i] = int(op_index % 3);
            op_index /= 3;
        }

        auto rgs = unrank_rgs(b + 1, var_index);
        std::map<int, char> blk2ch;
        std::vector<std::string> vars(b + 1);
        char next = 'A';
        for (int i = 0; i < b + 1; ++i) {
            if (!blk2ch.count(rgs[i]))
                blk2ch[rgs[i]] = next++;
            vars[i] = std::string(1, blk2ch[rgs[i]]);
        }

        int opi = 0, leafi = 0;
        return build_expression(s, rootIdx, opd, opi, vars, leafi);
    }
};

int main() {
    ExpressionGenerator gen;

    long long n = 100;
    long long num = 1000000000;
    for (int i = 1; i <= n; ++i) {
        std::cout << "#" << i << ": " << gen.get_expression(i) << "\n";
    }

    std::cout << "#" << num << ": " << gen.get_expression(num) << "\n";
    std::cout << "Max=" << gen.maxIndex() << "\n";
    return 0;
}
