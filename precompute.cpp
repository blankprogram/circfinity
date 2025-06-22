#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
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

int main() {
    std::array<std::vector<Node>, MAX_SIZE + 1> nodesPool;
    std::array<std::vector<uint32_t>, MAX_SIZE + 1> roots;
    std::array<std::vector<long long>, MAX_SIZE + 1> weights;
    std::array<long long, MAX_SIZE + 1> cumulativeTotal{};

    nodesPool[1].push_back({0, 0, 0, 0, 0, 0, 0});
    roots[1].push_back(0);
    weights[1].push_back(1);
    cumulativeTotal[1] = 1;

    for (int s = 2; s <= MAX_SIZE; ++s) {
        auto &pool = nodesPool[s];
        auto &rts = roots[s];
        auto &w = weights[s];

        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (auto li : roots[ls]) {
                for (auto ri : roots[rs]) {
                    auto const &L = nodesPool[ls][li];
                    auto const &R = nodesPool[rs][ri];
                    Node n;
                    n.type = 2;
                    n.numBinary = uint8_t(1 + L.numBinary + R.numBinary);
                    n.childSize = uint8_t(ls);
                    n.rightSize = uint8_t(rs);
                    n.left = li;
                    n.right = ri;
                    uint32_t idx = uint32_t(pool.size());
                    pool.push_back(n);
                    rts.push_back(idx);
                    w.push_back(Pow3[n.numBinary] * Bell[n.numBinary + 1]);
                }
            }
        }

        for (auto ci : roots[s - 1]) {
            auto const &C = nodesPool[s - 1][ci];
            Node n;
            n.type = 1;
            n.numBinary = C.numBinary;
            n.childSize = uint8_t(s - 1);
            n.rightSize = 0;
            n.child = ci;
            n.left = n.right = 0;
            uint32_t idx = uint32_t(pool.size());
            pool.push_back(n);
            rts.push_back(idx);
            w.push_back(Pow3[n.numBinary] * Bell[n.numBinary + 1]);
        }

        std::vector<long long> prefix(w.size());
        std::inclusive_scan(w.begin(), w.end(), prefix.begin());
        cumulativeTotal[s] = cumulativeTotal[s - 1] + prefix.back();
    }

    std::ofstream out("precomputed_data.bin", std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open precomputed_data.bin\n";
        return 1;
    }

    for (int s = 1; s <= MAX_SIZE; ++s) {
        uint32_t nodeCount = uint32_t(nodesPool[s].size());
        uint32_t shapeCount = uint32_t(roots[s].size());
        out.write(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));
        out.write(reinterpret_cast<char *>(nodesPool[s].data()),
                  nodeCount * sizeof(Node));
        out.write(reinterpret_cast<char *>(&shapeCount), sizeof(shapeCount));
        out.write(reinterpret_cast<char *>(roots[s].data()),
                  shapeCount * sizeof(uint32_t));
        out.write(reinterpret_cast<char *>(weights[s].data()),
                  shapeCount * sizeof(long long));
    }

    out.write(reinterpret_cast<char *>(cumulativeTotal.data() + 1),
              MAX_SIZE * sizeof(long long));
    return 0;
}
