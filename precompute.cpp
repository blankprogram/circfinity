#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>

template <std::size_t N> consteval std::array<uint64_t, N + 2> make_bell() {
    std::array<uint64_t, N + 2> B{}, prev{}, row{};
    B[0] = prev[0] = 1;
    for (std::size_t n = 1; n <= N + 1; ++n) {
        row[0] = prev[n - 1];
        for (std::size_t k = 1; k <= n; ++k)
            row[k] = row[k - 1] + prev[k - 1];
        B[n] = row[0];
        for (std::size_t k = 0; k <= n; ++k)
            prev[k] = row[k];
    }
    return B;
}

static constexpr int MAX_SIZE = 23;
static constexpr auto Bell = make_bell<MAX_SIZE>();

template <std::size_t N> consteval std::array<long long, N + 1> make_pow3() {
    std::array<long long, N + 1> p{};
    p[0] = 1;
    for (int i = 1; i <= N; ++i)
        p[i] = p[i - 1] * 3;
    return p;
}

static constexpr auto Pow3 = make_pow3<MAX_SIZE>();

static constexpr std::array<long long, MAX_SIZE + 1> WeightFactor = [] {
    std::array<long long, MAX_SIZE + 1> wf{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        wf[b] = Pow3[b] * Bell[b + 1];
    return wf;
}();

struct Node {
    uint8_t type, numBinary, childSize, rightSize;
    uint32_t child, left, right;
};

int main() {
    std::array<Node *, MAX_SIZE + 1> pools{};
    std::array<uint32_t *, MAX_SIZE + 1> roots{};
    std::array<long long *, MAX_SIZE + 1> wts{};
    std::array<size_t, MAX_SIZE + 1> shapeCount{};
    std::array<long long, MAX_SIZE + 1> cumulative{};

    shapeCount[1] = 1;
    pools[1] = new Node[1]{{0, 0, 0, 0, 0, 0, 0}};
    roots[1] = new uint32_t[1]{0};
    wts[1] = new long long[1]{WeightFactor[0]};
    cumulative[1] = WeightFactor[0];

    for (int s = 2; s <= MAX_SIZE; ++s) {
        size_t binaryCount = 0;
        for (int ls = 1; ls <= s - 2; ++ls)
            binaryCount += shapeCount[ls] * shapeCount[s - 1 - ls];
        size_t unaryCount = shapeCount[s - 1];
        size_t totalCount = binaryCount + unaryCount;
        shapeCount[s] = totalCount;

        pools[s] = new Node[totalCount];
        roots[s] = new uint32_t[totalCount];
        wts[s] = new long long[totalCount];

        uint32_t idx = 0;
        long long run = 0;

        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (size_t i = 0; i < shapeCount[ls]; ++i) {
                const Node &L = pools[ls][roots[ls][i]];
                for (size_t j = 0; j < shapeCount[rs]; ++j) {
                    const Node &R = pools[rs][roots[rs][j]];
                    Node n;
                    n.type = 2;
                    n.numBinary = uint8_t(1 + L.numBinary + R.numBinary);
                    n.childSize = uint8_t(ls);
                    n.rightSize = uint8_t(rs);
                    n.left = roots[ls][i];
                    n.right = roots[rs][j];
                    pools[s][idx] = n;
                    roots[s][idx] = idx;
                    run += WeightFactor[n.numBinary];
                    wts[s][idx] = run;
                    ++idx;
                }
            }
        }

        for (size_t i = 0; i < shapeCount[s - 1]; ++i) {
            const Node &C = pools[s - 1][roots[s - 1][i]];
            Node n;
            n.type = 1;
            n.numBinary = C.numBinary;
            n.childSize = uint8_t(s - 1);
            n.rightSize = 0;
            n.child = roots[s - 1][i];
            n.left = n.right = 0;
            pools[s][idx] = n;
            roots[s][idx] = idx;
            run += WeightFactor[n.numBinary];
            wts[s][idx] = run;
            ++idx;
        }

        cumulative[s] = cumulative[s - 1] + run;
    }

    std::ofstream out("precomputed_data.bin", std::ios::binary);
    if (!out) {
        std::cerr << "cannot open precomputed_data.bin\n";
        return 1;
    }

    for (int s = 1; s <= MAX_SIZE; ++s) {
        uint32_t nc = uint32_t(shapeCount[s]);
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
        out.write(reinterpret_cast<char *>(pools[s]), nc * sizeof(Node));
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
        out.write(reinterpret_cast<char *>(roots[s]), nc * sizeof(uint32_t));
        out.write(reinterpret_cast<char *>(wts[s]), nc * sizeof(long long));
    }
    out.write(reinterpret_cast<char *>(&cumulative[1]),
              MAX_SIZE * sizeof(long long));
    return 0;
}
