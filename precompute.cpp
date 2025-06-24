// precompute_counts.cpp
// Build only the small DP tables for un‐ranking later.
// Produces counts.bin (~50 KB).

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

// ────────────────────────────────────────────────────────────────────────────
// CONFIGURATION
static constexpr int MAX_SIZE = 24;

// 1) Bell numbers up to MAX_SIZE+1, and 3^b up to MAX_SIZE
static consteval std::array<uint64_t, MAX_SIZE + 2> make_bell() {
    std::array<uint64_t, MAX_SIZE + 2> B{}, p{}, r{};
    B[0] = p[0] = 1;
    for (int n = 1; n <= MAX_SIZE + 1; ++n) {
        r[0] = p[n - 1];
        for (int k = 1; k <= n; ++k)
            r[k] = r[k - 1] + p[k - 1];
        B[n] = r[0];
        for (int k = 0; k <= n; ++k)
            p[k] = r[k];
    }
    return B;
}
static consteval std::array<uint64_t, MAX_SIZE + 1> make_pow3() {
    std::array<uint64_t, MAX_SIZE + 1> P{};
    P[0] = 1;
    for (int i = 1; i <= MAX_SIZE; ++i)
        P[i] = P[i - 1] * 3;
    return P;
}
static constexpr auto Bell = make_bell();
static constexpr auto Pow3 = make_pow3();

// 2) weightFactor[b] = (#variants per shape with b binary ops)
//    = Bell[b+1] * 3^b
static constexpr std::array<uint64_t, MAX_SIZE + 1> WeightFactor = []() {
    std::array<uint64_t, MAX_SIZE + 1> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        W[b] = Bell[b + 1] * Pow3[b];
    return W;
}();

// 3) DP: C[s][b] = # of *shapes* of total size s with exactly b binary ops
static void
compute_C(std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1> &C) {
    C[1].fill(0);
    C[1][0] = 1;
    for (int s = 2; s <= MAX_SIZE; ++s) {
        C[s].fill(0);
        // binary‐root shapes
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            for (int b1 = 0; b1 <= ls; ++b1) {
                auto c1 = C[ls][b1];
                if (!c1)
                    continue;
                for (int b2 = 0; b2 <= rs; ++b2) {
                    auto c2 = C[rs][b2];
                    if (!c2)
                        continue;
                    C[s][b1 + b2 + 1] += c1 * c2;
                }
            }
        }
        // unary‐root shapes
        for (int b = 0; b < s; ++b)
            C[s][b] += C[s - 1][b];
    }
}

// write exactly n bytes to fd
static void full_write(int fd, const void *buf, size_t n) {
    auto p = (const char *)buf;
    while (n) {
        ssize_t w = ::write(fd, p, n);
        if (w < 0) {
            perror("write");
            std::exit(1);
        }
        p += w;
        n -= w;
    }
}

int main() {
    std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1> C;
    compute_C(C);

    std::array<uint64_t, MAX_SIZE + 1> shapeCount{}, shapeWeight{},
        cumShapeWeight{};
    cumShapeWeight[0] = 0;
    for (int s = 1; s <= MAX_SIZE; ++s) {
        uint64_t sc = 0, sw = 0;
        for (int b = 0; b <= MAX_SIZE; ++b) {
            sc += C[s][b];
            sw += C[s][b] * WeightFactor[b];
        }
        shapeCount[s] = sc;
        shapeWeight[s] = sw;
        cumShapeWeight[s] = cumShapeWeight[s - 1] + sw;
    }

    static std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1>
        blockWeight;
    static std::array<
        std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1>,
        MAX_SIZE + 1>
        rowWeightSum;

    for (int s = 2; s <= MAX_SIZE; ++s) {
        for (int ls = 1; ls <= s - 2; ++ls) {
            int rs = s - 1 - ls;
            uint64_t bw = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                if (!C[ls][b1])
                    continue;
                uint64_t rowsum = 0;
                for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                    if (!C[rs][b2])
                        continue;
                    rowsum += C[rs][b2] * WeightFactor[b1 + b2 + 1];
                }
                rowWeightSum[s][ls][b1] = rowsum;
                bw += C[ls][b1] * rowsum;
            }
            blockWeight[s][ls] = bw;
        }
    }

    int fd = ::open("counts.bin", O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // header
    uint32_t magic = 0xB10CB10C, ms = MAX_SIZE;
    full_write(fd, &magic, sizeof(magic));
    full_write(fd, &ms, sizeof(ms));

    // dump C[s][0..MAX_SIZE]
    for (int s = 1; s <= MAX_SIZE; ++s)
        full_write(fd, C[s].data(), sizeof(uint64_t) * (MAX_SIZE + 1));

    // shapeCount, shapeWeight, cumShapeWeight
    full_write(fd, shapeCount.data(), sizeof(uint64_t) * (MAX_SIZE + 1));
    full_write(fd, shapeWeight.data(), sizeof(uint64_t) * (MAX_SIZE + 1));
    full_write(fd, cumShapeWeight.data(), sizeof(uint64_t) * (MAX_SIZE + 1));

    // blockWeight[s][ls]
    for (int s = 1; s <= MAX_SIZE; ++s)
        for (int ls = 0; ls <= MAX_SIZE; ++ls) {
            uint64_t w = (ls >= 1 && ls <= s - 2 ? blockWeight[s][ls] : 0);
            full_write(fd, &w, sizeof(w));
        }

    // rowWeightSum[s][ls][b1]
    for (int s = 1; s <= MAX_SIZE; ++s)
        for (int ls = 0; ls <= MAX_SIZE; ++ls) {
            if (ls >= 1 && ls <= s - 2)
                full_write(fd, rowWeightSum[s][ls].data(),
                           sizeof(uint64_t) * (MAX_SIZE + 1));
            else {
                uint64_t zero[MAX_SIZE + 1] = {};
                full_write(fd, zero, sizeof(zero));
            }
        }

    ::close(fd);
    return 0;
}
