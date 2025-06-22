#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

template <std::size_t N>
consteval std::array<uint64_t, N + 2> make_bell_array_plus1() {
    std::array<uint64_t, N + 2> B{};
    std::array<uint64_t, N + 2> prev{};
    std::array<uint64_t, N + 2> row{};
    B[0] = 1;
    prev[0] = 1;

    for (std::size_t n = 1; n <= N + 1; ++n) {
        row[0] = prev[n - 1];

        for (std::size_t k = 1; k <= n; ++k) {
            row[k] = row[k - 1] + prev[k - 1];
        }

        B[n] = row[0];

        for (std::size_t k = 0; k <= n; ++k) {
            prev[k] = row[k];
        }
    }

    return B;
}

static constexpr int MAX_SIZE = 22;
static constexpr auto Bell = make_bell_array_plus1<MAX_SIZE>();

static constexpr std::array<long long, MAX_SIZE + 1> Pow3 = [] {
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
    std::array<std::vector<long long>, MAX_SIZE + 1> prefix;
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
        const Node &n = nodesPool[s][ni];
        if (n.type == 0) {
            return vars[leafi++];
        }
        if (n.type == 1) {
            auto c =
                build_expression(n.childSize, n.child, opd, opi, vars, leafi);
            return "NOT(" + c + ")";
        }
        int d = opd[opi++];
        static constexpr const char *OP_STR[3] = {"AND", "OR", "XOR"};
        auto L = build_expression(n.childSize, n.left, opd, opi, vars, leafi);
        auto R = build_expression(n.rightSize, n.right, opd, opi, vars, leafi);
        return std::string(OP_STR[d]) + "(" + L + "," + R + ")";
    }

  public:
    ExpressionGenerator() {
        std::ifstream in("precomputed_data.bin", std::ios::binary);
        if (!in) {
            std::cerr << "Cannot open data\n";
            std::exit(1);
        }

        for (int s = 1; s <= MAX_SIZE; ++s) {
            uint32_t nodeCount;
            in.read(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));
            nodesPool[s].reserve(nodeCount);
            nodesPool[s].resize(nodeCount);
            in.read(reinterpret_cast<char *>(nodesPool[s].data()),
                    nodeCount * sizeof(Node));

            uint32_t shapeCount;
            in.read(reinterpret_cast<char *>(&shapeCount), sizeof(shapeCount));
            roots[s].reserve(shapeCount);
            roots[s].resize(shapeCount);
            in.read(reinterpret_cast<char *>(roots[s].data()),
                    shapeCount * sizeof(uint32_t));

            prefix[s].reserve(shapeCount);
            prefix[s].resize(shapeCount);
            in.read(reinterpret_cast<char *>(prefix[s].data()),
                    shapeCount * sizeof(long long));
        }

        cumulativeTotal[0] = 0;
        for (int s = 1; s <= MAX_SIZE; ++s) {
            in.read(reinterpret_cast<char *>(&cumulativeTotal[s]),
                    sizeof(long long));
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

        const auto &P = prefix[s];
        auto it = std::lower_bound(P.begin(), P.end(), offset + 1);
        int idxShape = int(it - P.begin());
        long long prev = idxShape ? P[idxShape - 1] : 0;
        long long resid = offset - prev;

        uint32_t rootIdx = roots[s][idxShape];
        int b = nodesPool[s][rootIdx].numBinary;

        long long varCount = Bell[b + 1];
        long long op_index = resid / varCount;
        long long var_index = resid % varCount;

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
    long long num = 1'000'000'000;
    long long max = gen.maxIndex();
    for (int i = 1; i <= n; ++i)
        std::cout << "#" << i << ": " << gen.get_expression(i) << "\n";
    std::cout << "#" << num << ": " << gen.get_expression(num) << "\n";
    std::cout << "#" << max << ": " << gen.get_expression(max) << "\n";
    std::cout << "Max=" << max << "\n";
    return 0;
}
