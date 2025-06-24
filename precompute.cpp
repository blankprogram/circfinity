// precompute.cpp
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

// compile-time Bell numbers up to MAX_SIZE+1
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

// powers of 3 up to MAX_SIZE
template <std::size_t N> consteval std::array<long long, N + 1> make_pow3() {
    std::array<long long, N + 1> p{};
    p[0] = 1;
    for (int i = 1; i <= N; ++i)
        p[i] = p[i - 1] * 3;
    return p;
}

static constexpr auto Pow3 = make_pow3<MAX_SIZE>();

// weight[b] = (# of var-assignments) * (# of op-selections)
static constexpr std::array<long long, MAX_SIZE + 1> WeightFactor = [] {
    std::array<long long, MAX_SIZE + 1> wf{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        wf[b] = Pow3[b] * Bell[b + 1];
    return wf;
}();

using idx_t = uint16_t;

// on-disk node format: exactly 8 bytes
struct Node {
    uint8_t type;      // 0=leaf,1=unary,2=binary
    uint8_t numBinary; // number of binary ops in subtree
    uint8_t childSize; // size of the single child (if unary) or left subtree
    uint8_t rightSize; // size of right subtree (0 if unary)
    union {
        idx_t child; // for leaf/unary
        struct {     // for binary
            idx_t left, right;
        } br;
    } u;
};
static_assert(sizeof(Node) == 8, "Node on-disk format must be 8 bytes");

int main() {
    std::ofstream out("precomputed_data.bin", std::ios::binary);
    if (!out) {
        std::cerr << "❌ cannot open precomputed_data.bin\n";
        return 1;
    }

    // shapeCount[s] = number of distinct tree‐shapes of total size s
    std::array<size_t, MAX_SIZE + 1> shapeCount{};
    // cum[s] = total weight up through layer s (used for prefix sums)
    std::array<long long, MAX_SIZE + 1> cumulative{};

    // we only ever store one byte per node for every layer
    std::vector<std::vector<uint8_t>> numBinary(MAX_SIZE + 1);

    // ─── Layer 1 (just the single leaf) ───
    shapeCount[1] = 1;
    numBinary[1].push_back(0);

    // write layer‐header
    uint32_t nc = 1;
    out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
    // write the one Node
    {
        Node n;
        n.type = 0;
        n.numBinary = 0;
        n.childSize = 0;
        n.rightSize = 0;
        n.u.child = 0;
        out.write(reinterpret_cast<char *>(&n), sizeof(n));
    }

    // write the “roots” identity map
    out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
    uint32_t zero = 0;
    out.write(reinterpret_cast<char *>(&zero),
              sizeof(zero)); // ✓ correct: the one root is index 0

    // write prefix sums
    long long run = 0;
    run += WeightFactor[0];
    cumulative[1] = run;
    out.write(reinterpret_cast<char *>(&run), sizeof(run));

    for (int s = 2; s <= MAX_SIZE; ++s) {
        // 1) figure out how many shapes there are
        size_t binaryCount = 0;
        for (int ls = 1; ls <= s - 2; ++ls)
            binaryCount += shapeCount[ls] * shapeCount[s - 1 - ls];
        size_t unaryCount = shapeCount[s - 1];
        shapeCount[s] = binaryCount + unaryCount;
        numBinary[s].reserve(shapeCount[s]);

        // 2) write layer header + Node block
        nc = uint32_t(shapeCount[s]);
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));

        // First pass: generate every Node, write it, record its numBinary
        uint32_t idx = 0;
        // ─ binary nodes ─
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (size_t i = 0; i < shapeCount[ls]; ++i) {
                for (size_t j = 0; j < shapeCount[rs]; ++j) {
                    // compute numBinary from children
                    uint8_t nb = 1 + numBinary[ls][i] + numBinary[rs][j];
                    Node n;
                    n.type = 2;
                    n.numBinary = nb;
                    n.childSize = uint8_t(ls);
                    n.rightSize = uint8_t(rs);
                    n.u.br.left = idx_t(i);
                    n.u.br.right = idx_t(j);
                    out.write(reinterpret_cast<char *>(&n), sizeof(n));
                    numBinary[s].push_back(nb);
                    ++idx;
                }
            }
        }
        // ─ unary nodes ─
        for (size_t i = 0; i < shapeCount[s - 1]; ++i) {
            uint8_t nb = numBinary[s - 1][i];
            Node n;
            n.type = 1;
            n.numBinary = nb;
            n.childSize = uint8_t(s - 1);
            n.rightSize = 0;
            n.u.child = idx_t(i);
            out.write(reinterpret_cast<char *>(&n), sizeof(n));
            numBinary[s].push_back(nb);
            ++idx;
        }

        // 3) write roots identity array
        out.write(reinterpret_cast<char *>(&nc), sizeof(nc));
        for (uint32_t i = 0; i < nc; ++i)
            out.write(reinterpret_cast<char *>(&i), sizeof(i));

        // 4) write prefix sums for this layer
        run = cumulative[s - 1];
        for (auto nb : numBinary[s]) {
            run += WeightFactor[nb];
            out.write(reinterpret_cast<char *>(&run), sizeof(run));
        }
        cumulative[s] = run;
    }

    // ─── trailing cumulative totals ───
    out.write(reinterpret_cast<char *>(&cumulative[1]),
              sizeof(long long) * MAX_SIZE);

    return 0;
}
