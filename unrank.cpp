// unrank.cpp
// Fully un‐ranks the first 100 Boolean‐formulas of total size ≤ MAX_SIZE,
// then prints the grand total.  Requires counts.bin (from
// precompute_counts.cpp).

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// CONFIGURATION
static constexpr int MAX_SIZE = 24;

// Bell & 3^b tables
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

// WeightFactor[b] = Bell[b+1] * 3^b
static constexpr auto WeightFactor = []() {
    std::array<uint64_t, MAX_SIZE + 1> W{};
    for (int b = 0; b <= MAX_SIZE; ++b)
        W[b] = Bell[b + 1] * Pow3[b];
    return W;
}();

// DP tables loaded from counts.bin
static std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1> C;
static std::array<uint64_t, MAX_SIZE + 1> shapeCount, shapeWeight,
    cumShapeWeight;
static std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1> blockWeight;
static std::array<std::array<std::array<uint64_t, MAX_SIZE + 1>, MAX_SIZE + 1>,
                  MAX_SIZE + 1>
    rowWeightSum;

// ——— RGS DP tables & init ————————————————————————————————————————————
static uint64_t DP_RGS[MAX_SIZE + 1][MAX_SIZE + 1];
static void init_rgs_dp() {
    for (int k = 0; k <= MAX_SIZE; ++k)
        DP_RGS[0][k] = 1;
    for (int len = 1; len <= MAX_SIZE; ++len) {
        for (int k = 0; k <= MAX_SIZE; ++k) {
            uint64_t sum = 0;
            for (int v = 0; v <= k + 1; ++v) {
                int nk = (v > k ? v : k);
                sum += DP_RGS[len - 1][nk];
            }
            DP_RGS[len][k] = sum;
        }
    }
}
// ————————————————————————————————————————————————————————————————————

static void load_counts() {
    int fd = open("counts.bin", O_RDONLY);
    assert(fd >= 0);
    struct stat st;
    fstat(fd, &st);
    auto data = (const uint8_t *)mmap(nullptr, st.st_size, PROT_READ,
                                      MAP_PRIVATE, fd, 0);
    assert(data != MAP_FAILED);
    close(fd);

    assert(*reinterpret_cast<const uint32_t *>(data) == 0xB10CB10C);
    assert(int(*reinterpret_cast<const uint32_t *>(data + 4)) == MAX_SIZE);
    const uint8_t *p = data + 8;

    for (int s = 1; s <= MAX_SIZE; ++s) {
        memcpy(C[s].data(), p, sizeof(uint64_t) * (MAX_SIZE + 1));
        p += sizeof(uint64_t) * (MAX_SIZE + 1);
    }
    memcpy(shapeCount.data(), p, sizeof(uint64_t) * (MAX_SIZE + 1));
    p += sizeof(uint64_t) * (MAX_SIZE + 1);
    memcpy(shapeWeight.data(), p, sizeof(uint64_t) * (MAX_SIZE + 1));
    p += sizeof(uint64_t) * (MAX_SIZE + 1);
    memcpy(cumShapeWeight.data(), p, sizeof(uint64_t) * (MAX_SIZE + 1));
    p += sizeof(uint64_t) * (MAX_SIZE + 1);

    for (int s = 1; s <= MAX_SIZE; ++s)
        for (int ls = 0; ls <= MAX_SIZE; ++ls) {
            blockWeight[s][ls] = *reinterpret_cast<const uint64_t *>(p);
            p += sizeof(uint64_t);
        }
    for (int s = 1; s <= MAX_SIZE; ++s)
        for (int ls = 0; ls <= MAX_SIZE; ++ls) {
            memcpy(rowWeightSum[s][ls].data(), p,
                   sizeof(uint64_t) * (MAX_SIZE + 1));
            p += sizeof(uint64_t) * (MAX_SIZE + 1);
        }

    munmap((void *)data, st.st_size);
}

// scratch buffer for building the prefix string
static char OUT[1 << 16];
static int OL;

// count how many binary ops in the idx-th shape of size s
static int count_binary(int s, uint64_t idx) {
    if (s == 1)
        return 0;
    uint64_t binS = shapeCount[s] - shapeCount[s - 1];
    if (idx < binS) {
        uint64_t acc = 0;
        int choice = 1;
        for (int ls = 1; ls <= s - 2; ++ls) {
            uint64_t bs = shapeCount[ls] * shapeCount[s - 1 - ls];
            if (idx < acc + bs) {
                idx -= acc;
                choice = ls;
                break;
            }
            acc += bs;
        }
        int ls = choice, rs = s - 1 - ls;
        uint64_t i = idx / shapeCount[rs], j = idx % shapeCount[rs];
        return 1 + count_binary(ls, i) + count_binary(rs, j);
    } else {
        return count_binary(s - 1, idx - (shapeCount[s] - shapeCount[s - 1]));
    }
}

// build the actual prefix-notation string
static void build_expr(int s, uint64_t idx, const std::vector<int> &ops,
                       const std::vector<int> &rgs, int &leafIdx, int &opIdx) {
    if (s == 1) {
        OUT[OL++] = char('A' + rgs[leafIdx++]);
        return;
    }
    uint64_t binS = shapeCount[s] - shapeCount[s - 1];
    if (idx < binS) {
        // binary root
        uint64_t acc = 0;
        int choice = 1;
        for (int ls = 1; ls <= s - 2; ++ls) {
            uint64_t bs = shapeCount[ls] * shapeCount[s - 1 - ls];
            if (idx < acc + bs) {
                idx -= acc;
                choice = ls;
                break;
            }
            acc += bs;
        }
        int ls = choice, rs = s - 1 - ls;
        uint64_t i = idx / shapeCount[rs], j = idx % shapeCount[rs];
        int op = ops[opIdx++];
        const char *lit = (op == 0 ? "AND" : op == 1 ? "OR" : "XOR");
        int L = (op == 1 ? 2 : 3);
        memcpy(OUT + OL, lit, L);
        OL += L;
        OUT[OL++] = '(';
        build_expr(ls, i, ops, rgs, leafIdx, opIdx);
        OUT[OL++] = ',';
        build_expr(rs, j, ops, rgs, leafIdx, opIdx);
        OUT[OL++] = ')';
    } else {
        // unary root
        uint64_t uidx = idx - binS;
        memcpy(OUT + OL, "NOT(", 4);
        OL += 4;
        build_expr(s - 1, uidx, ops, rgs, leafIdx, opIdx);
        OUT[OL++] = ')';
    }
}

// shape_unrank: binary splits (ls=1..s-2) first, then the unary split (ls=0)
static uint64_t shape_unrank(int s, uint64_t woff, int &b_shape,
                             uint64_t &variantOff) {
    if (s == 1) {
        b_shape = 0;
        variantOff = woff; // must be zero
        return 0;
    }

    uint64_t binCount = shapeCount[s] - shapeCount[s - 1];
    uint64_t acc = 0;

    // 1) binary-root blocks (ls=1..s-2)
    for (int ls = 1; ls <= s - 2; ++ls) {
        uint64_t bw = blockWeight[s][ls];
        if (woff < acc + bw) {
            uint64_t offB = woff - acc;
            int rs = s - 1 - ls;
            uint64_t rowAcc = 0;
            for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                uint64_t cntL = C[ls][b1];
                if (!cntL)
                    continue;
                uint64_t rowW = rowWeightSum[s][ls][b1];
                uint64_t totW = cntL * rowW;
                if (offB < rowAcc + totW) {
                    uint64_t offG = offB - rowAcc;
                    uint64_t i = offG / rowW;
                    uint64_t offR = offG % rowW;
                    uint64_t colAcc = 0;
                    for (int b2 = 0; b2 <= MAX_SIZE; ++b2) {
                        uint64_t cntR = C[rs][b2];
                        if (!cntR)
                            continue;
                        uint64_t cellW = WeightFactor[b1 + b2 + 1];
                        uint64_t totC = cntR * cellW;
                        if (offR < colAcc + totC) {
                            uint64_t off2 = offR - colAcc;
                            uint64_t j = off2 / cellW;
                            variantOff = off2 % cellW;
                            b_shape = b1 + b2 + 1;
                            // compute shapeIdx among *binary* shapes
                            uint64_t base = 0;
                            for (int x = 1; x < ls; ++x)
                                base += shapeCount[x] * shapeCount[s - 1 - x];
                            return base + i * shapeCount[rs] + j;
                        }
                        colAcc += totC;
                    }
                }
                rowAcc += totW;
            }
        }
        acc += bw;
    }

    // 2) unary-root block (ls=0) last:
    uint64_t offU = woff - acc;
    uint64_t childIdx = shape_unrank(s - 1, offU, b_shape, variantOff);
    return binCount + childIdx;
}

// top-level unrank
static std::string_view unrank(uint64_t N) {
    assert(N >= 1 && N <= cumShapeWeight[MAX_SIZE]);
    int s = 1;
    while (cumShapeWeight[s] < N)
        ++s;
    uint64_t layerOff = N - (cumShapeWeight[s - 1] + 1);

    int b_shape;
    uint64_t variantOff;
    uint64_t shapeIdx = shape_unrank(s, layerOff, b_shape, variantOff);

    // split variantOff into opIndex vs varIndex
    uint64_t nVar = Bell[b_shape + 1];
    uint64_t opIndex = variantOff / nVar;
    uint64_t varIndex = variantOff % nVar;

    // base-3 decode ops
    std::vector<int> ops(b_shape);
    for (int i = b_shape - 1; i >= 0; --i) {
        ops[i] = int(opIndex % 3);
        opIndex /= 3;
    }

    // restricted-growth unrank for rgs[]
    int M = b_shape + 1;
    std::vector<int> rgs(M);
    rgs[0] = 0;
    {
        int maxSeen = 0;
        uint64_t rem = varIndex;
        for (int pos = 1; pos < M; ++pos) {
            int tail = M - pos - 1;
            for (int v = 0; v <= maxSeen + 1; ++v) {
                int nk = (v > maxSeen ? v : maxSeen);
                uint64_t cnt = DP_RGS[tail][nk];
                if (rem < cnt) {
                    rgs[pos] = v;
                    maxSeen = nk;
                    break;
                }
                rem -= cnt;
            }
        }
    }

    // build and return the prefix-notation string
    OL = 0;
    int leafIdx = 0, opIdxLoc = 0;
    build_expr(s, shapeIdx, ops, rgs, leafIdx, opIdxLoc);
    return std::string_view(OUT, OL);
}

int main() {
    init_rgs_dp();
    load_counts();

    uint64_t total = cumShapeWeight[MAX_SIZE];
    uint64_t toPrint = std::min<uint64_t>(100, total);

    for (uint64_t i = 1; i <= toPrint; ++i) {
        auto e = unrank(i);
        printf("#%llu: %.*s\n", (unsigned long long)i, int(e.size()), e.data());
    }
    auto last = unrank(total);
    printf("#%llu: %.*s\n", (unsigned long long)total, int(last.size()),
           last.data());
    printf("Total expressions: %llu\n", (unsigned long long)total);
    return 0;
}
