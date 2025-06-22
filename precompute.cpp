#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>

template <std::size_t N>
consteval std::array<uint64_t, N + 2> make_bell_array_plus1() {
    std::array<uint64_t, N + 2> B{};
    std::array<uint64_t, N + 2> prev{};
    std::array<uint64_t, N + 2> row{};

    // B0 = 1
    B[0] = 1;
    prev[0] = 1;

    for (std::size_t n = 1; n <= N + 1; ++n) {
        row[0] = prev[n - 1];
        for (std::size_t k = 1; k <= n; ++k) {
            row[k] = row[k - 1] + prev[k - 1];
        }
        B[n] = row[n];

        for (std::size_t k = 0; k <= n; ++k)
            prev[k] = row[k];
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

static const std::array<long long, MAX_SIZE + 1> weightFactor = [] {
    std::array<long long, MAX_SIZE + 1> wf{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        wf[b] = Pow3[b] * Bell[b + 1];
    return wf;
}();
static const long long *const wf = weightFactor.data();

struct Node {
    uint8_t type;
    uint8_t numBinary;
    uint8_t childSize;
    uint8_t rightSize;
    uint32_t child;
    uint32_t left;
    uint32_t right;
};

int main() {
    std::array<Node *, MAX_SIZE + 1> poolBufs{};
    std::array<uint32_t *, MAX_SIZE + 1> rootsBufs{};
    std::array<long long *, MAX_SIZE + 1> wtsBufs{};
    std::array<size_t, MAX_SIZE + 1> shapeCount{};
    std::array<long long, MAX_SIZE + 1> cumulativeTotal{};

    shapeCount[1] = 1;
    poolBufs[1] = new Node[1];
    rootsBufs[1] = new uint32_t[1];
    wtsBufs[1] = new long long[1];
    poolBufs[1][0] = Node{0, 0, 0, 0, 0, 0, 0};
    rootsBufs[1][0] = 0;
    wtsBufs[1][0] = wf[0];
    cumulativeTotal[1] = wf[0];

    for (int s = 2; s <= MAX_SIZE; ++s) {
        size_t est = shapeCount[s - 1];
        for (int ls = 1; ls <= s - 2; ++ls)
            est += shapeCount[ls] * shapeCount[s - 1 - ls];
        shapeCount[s] = est;

        poolBufs[s] = new Node[est];
        rootsBufs[s] = new uint32_t[est];
        wtsBufs[s] = new long long[est];

        uint32_t idx = 0;
        long long running = 0;

        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            Node *Lpool = poolBufs[ls];
            uint32_t *Lroots = rootsBufs[ls];
            Node *Rpool = poolBufs[rs];
            for (size_t i = 0; i < shapeCount[ls]; ++i) {
                uint32_t li = Lroots[i];
                const Node &L = Lpool[li];
                for (size_t j = 0; j < shapeCount[rs]; ++j) {
                    uint32_t ri = rootsBufs[rs][j];
                    const Node &R = Rpool[ri];
                    Node n;
                    n.type = 2;
                    n.numBinary = uint8_t(1 + L.numBinary + R.numBinary);
                    n.childSize = uint8_t(ls);
                    n.rightSize = uint8_t(rs);
                    n.left = li;
                    n.right = ri;
                    poolBufs[s][idx] = n;
                    rootsBufs[s][idx] = idx;
                    running += wf[n.numBinary];
                    wtsBufs[s][idx] = running;
                    ++idx;
                }
            }
        }

        Node *Cpool = poolBufs[s - 1];
        uint32_t *Croots = rootsBufs[s - 1];
        for (size_t i = 0; i < shapeCount[s - 1]; ++i) {
            uint32_t ci = Croots[i];
            const Node &C = Cpool[ci];
            Node n;
            n.type = 1;
            n.numBinary = C.numBinary;
            n.childSize = uint8_t(s - 1);
            n.rightSize = 0;
            n.child = ci;
            n.left = n.right = 0;
            poolBufs[s][idx] = n;
            rootsBufs[s][idx] = idx;
            running += wf[n.numBinary];
            wtsBufs[s][idx] = running;
            ++idx;
        }

        cumulativeTotal[s] = cumulativeTotal[s - 1] + running;
    }

    std::ofstream out("precomputed_data.bin", std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open precomputed_data.bin\n";
        return 1;
    }
    for (int s = 1; s <= MAX_SIZE; ++s) {
        uint32_t nc = uint32_t(shapeCount[s]);
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
        out.write(reinterpret_cast<char *>(poolBufs[s]), nc * sizeof(Node));
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
        out.write(reinterpret_cast<char *>(rootsBufs[s]),
                  nc * sizeof(uint32_t));
        out.write(reinterpret_cast<char *>(wtsBufs[s]), nc * sizeof(long long));
    }
    out.write(reinterpret_cast<char *>(cumulativeTotal.data() + 1),
              MAX_SIZE * sizeof(long long));
    return 0;
}
