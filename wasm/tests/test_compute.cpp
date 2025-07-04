#include "compute.h"
#include "compute_data.h"
#include <catch2/catch_all.hpp>
#include <unordered_set>
#include <vector>

// helpers ---------------------------------------------------------------
static bigint ipow(bigint b, int e) {
    bigint r = 1;
    while (e) {
        if (e & 1)
            r *= b;
        b *= b;
        e >>= 1;
    }
    return r;
}
constexpr bigint binom(int n, int k) {
    if (k < 0 || k > n)
        return 0;
    if (k > n - k)
        k = n - k;
    bigint r = 1;
    for (int i = 1; i <= k; ++i)
        r = r * bigint(n - i + 1) / bigint(i);
    return r;
}

static std::vector<int> make_labels(std::size_t n) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    return v;
}

static std::string ref_label(std::uint64_t id) {
    std::string s;
    while (true) {
        int d = id % 26;
        s.push_back(char('A' + d));
        if (id < 26)
            break;
        id = id / 26 - 1;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

struct VecHash {
    std::size_t operator()(std::vector<int> const &v) const noexcept {
        std::size_t h = 0;
        for (int x : v) //  FNV-1a-like mix
            h = (h ^ std::size_t(x)) * 0x9E3779B97F4A7C15ULL;
        return h;
    }
};

static int internal_size(std::string_view e) {
    int sz = 0;
    for (size_t i = 0; i < e.size();) {
        if (i + 3 <= e.size() && e.substr(i, 3) == "NOT") {
            ++sz;
            i += 3;
        } else if (i + 3 <= e.size() &&
                   (e.substr(i, 3) == "AND" || e.substr(i, 3) == "XOR")) {
            ++sz;
            i += 3;
        } else if (i + 2 <= e.size() && e.substr(i, 2) == "OR") {
            ++sz;
            i += 2;
        } else {
            ++i;
        }
    }
    return sz;
}

static std::string recover_sig(std::string_view txt) {
    std::string rec;
    for (size_t i = 0, n = txt.size(); i < n;) {
        if (i + 3 <= n && txt.compare(i, 3, "NOT") == 0) {
            rec.push_back('U');
            i += 3;
        } else if (i + 3 <= n && (txt.compare(i, 3, "AND") == 0 ||
                                  txt.compare(i, 3, "XOR") == 0)) {
            rec.push_back('B');
            i += 3;
        } else if (i + 2 <= n && txt.compare(i, 2, "OR") == 0) {
            rec.push_back('B');
            i += 2;
        } else if (std::isupper(static_cast<unsigned char>(txt[i]))) {
            rec.push_back('L');
            ++i;
            while (i < n && std::isupper(static_cast<unsigned char>(txt[i]))) {
                ++i;
            }
        } else {
            ++i;
        }
    }
    return rec;
}

// ─────────────────────────────────────────────────────────────
// Pow3
// ─────────────────────────────────────────────────────────────
TEST_CASE("Pow3 – matches 3^k for all k") {
    for (int k = 0; k <= MAX_S; ++k)
        REQUIRE(Pow3[k] == ipow(3, k));
    for (int k = 0; k < MAX_S; ++k)
        REQUIRE(Pow3[k + 1] / Pow3[k] == bigint(3));
}
// ─────────────────────────────────────────────────────────────
// Bell
// ─────────────────────────────────────────────────────────────
TEST_CASE("Bell – triangle recomputation") {
    std::array<bigint, MAX_S + 1> ref{};
    ref[0] = 1;
    for (int n = 1; n <= MAX_S; ++n)
        for (int k = 0; k <= n - 1; ++k)
            ref[n] += binom(n - 1, k) * ref[k];

    for (int n = 0; n <= MAX_S; ++n)
        REQUIRE(Bell[n] == ref[n]);
}
// ─────────────────────────────────────────────────────────────
// C   (shape-count table)
// ─────────────────────────────────────────────────────────────
TEST_CASE("C – triple recursion check") {
    std::array<std::array<bigint, MAX_U + 1>, MAX_S + 1> ref{};
    ref[1][0] = 1;
    for (int s = 1; s <= MAX_S; ++s)
        for (int u = 1; u <= MAX_U; ++u)
            ref[s][u] = ref[s][u - 1];

    for (int s = 2; s <= MAX_S; ++s)
        for (int u = 0; u <= MAX_U; ++u)
            for (int ls = 1; ls < s; ++ls)
                for (int u1 = 0; u1 <= u; ++u1)
                    ref[s][u] += ref[ls][u1] * ref[s - ls][u - u1];

    for (int s = 1; s <= MAX_S; ++s)
        for (int u = 0; u <= MAX_U; ++u)
            REQUIRE(C[s][u] == ref[s][u]);
}

// ─────────────────────────────────────────────────────────────
// DP_RGS
// ─────────────────────────────────────────────────────────────
TEST_CASE("DP_RGS – recurrence", "[DP_RGS]") {
    for (int m = 0; m <= MAX_S + 1; ++m) {
        REQUIRE(DP_RGS[0][m] == 1);
    }

    for (int len = 1; len <= MAX_S; ++len) {
        for (int m = 0; m <= MAX_S; ++m) {
            bigint rhs = 0;
            for (int v = 0; v <= m + 1; ++v)
                rhs += DP_RGS[len - 1][std::max(m, v)];
            REQUIRE(DP_RGS[len][m] == rhs);
        }
    }
}
// ─────────────────────────────────────────────────────────────
// Wn & prefixN
// ─────────────────────────────────────────────────────────────
TEST_CASE("Wn – closed-form cross-check") {
    for (int n = 0; n <= MAX_N; ++n) {
        bigint w = 0;
        for (int u = 0; u <= n && u <= MAX_U; ++u) {
            int s = n - u + 1, b = n - u;
            if (s < 1 || s > MAX_S)
                continue;
            w += C[s][u] * Pow3[b] * Bell[s];
        }
        REQUIRE(w == Wn[n]);
    }
}
TEST_CASE("prefixN – cumulative property") {
    REQUIRE(Wn[0] == prefixN[0]);
    for (int n = 1; n <= MAX_N; ++n)
        REQUIRE(prefixN[n] == prefixN[n - 1] + Wn[n]);
    REQUIRE(prefixN[MAX_N] > bigint(0));
}
// ─────────────────────────────────────────────────────────────
// Labels
// ─────────────────────────────────────────────────────────────
TEST_CASE("Labels – bijective base-26 table") {
    REQUIRE(Labels[0] == "A");
    REQUIRE(Labels[1] == "B");
    REQUIRE(Labels[25] == "Z");
    REQUIRE(Labels[26] == "AA");
    REQUIRE(Labels[27] == "AB");
    REQUIRE(Labels[51] == "AZ");
    REQUIRE(Labels[52] == "BA");

    for (std::size_t i = 0; i < kMaxLabels; ++i)
        REQUIRE(Labels[i] == ref_label(i));
}
// ─────────────────────────────────────────────────────────────
// to_string
// ─────────────────────────────────────────────────────────────
TEST_CASE("to_string – round-trip small & large") {
    REQUIRE(to_string(bigint(0)) == "0");
    REQUIRE(to_string(bigint(12345678901234567890ULL)) ==
            "12345678901234567890");
    REQUIRE(to_string(bigint(1) << 127) ==
            "170141183460469231731687303715884105728");
}

// ─────────────────────────────────────────────────────────────
// bit_length
// ─────────────────────────────────────────────────────────────
TEST_CASE("bit_length – trivial & power-of-two cases") {
    // zero is the only value whose bit-length is 0
    REQUIRE(bit_length(bigint(0)) == 0);

    // exact powers of two
    for (int k = 0; k <= 512; k += 73) {  // a few sparse exponents
        bigint p2 = bigint(1) << k;       // 2^k
        REQUIRE(bit_length(p2) == k + 1); // leading ‘1’ lives at bit k
        REQUIRE(bit_length(p2 - 1) == k); // one less uses exactly k bits
    }
}

TEST_CASE("bit_length – random composite values") {
    const std::pair<bigint, int> samples[] = {
        {bigint("12345678901234567890"), 64},
        {(bigint(1) << 127) + 1, 128},
        {(bigint(1) << 255) - 123456789, 255},
        {(bigint(1) << 1000) + (bigint(1) << 500) + 7, 1001},
    };

    for (auto [v, expect] : samples)
        REQUIRE(bit_length(v) == expect);
}

// ─────────────────────────────────────────────────────────────
// unrank_rgs
// ─────────────────────────────────────────────────────────────
TEST_CASE("unrank_rgs – first & last for len=3") {
    constexpr int len = 3;
    bigint total = Bell[len];
    REQUIRE(total == bigint(5));

    REQUIRE(unrank_rgs(len, bigint(0)) == std::vector<int>({0, 0, 0}));

    REQUIRE(unrank_rgs(len, total - 1) == std::vector<int>({0, 1, 2}));
}

TEST_CASE("unrank_rgs – bijection for len≤5") {
    for (int len = 1; len <= 5; ++len) {
        std::unordered_set<std::vector<int>, VecHash> seen;
        bigint total = Bell[len];

        for (bigint k = 0; k < total; ++k) {
            auto v = unrank_rgs(len, k);
            REQUIRE(seen.insert(v).second);
        }
        REQUIRE(seen.size() == static_cast<std::size_t>(total));
    }
}

// ─────────────────────────────────────────────────────────────
// unrank_shape
// ─────────────────────────────────────────────────────────────
TEST_CASE("unrank_shape – degenerate cases") {
    REQUIRE(unrank_shape(1, 0, 0) == "L");
    REQUIRE(unrank_shape(1, 2, 0) == "UUL");
    REQUIRE(unrank_shape(2, 0, 0) == "BLL");
}
TEST_CASE("unrank_shape – enumeration matches C for s≤4,u≤2") {
    for (int s = 1; s <= 4; ++s)
        for (int u = 0; u <= 2; ++u) {
            std::unordered_set<std::string> seen;
            bigint total = C[s][u];
            for (bigint k = 0; k < total; ++k)
                REQUIRE(seen.insert(unrank_shape(s, u, k)).second);
            REQUIRE(seen.size() == static_cast<std::size_t>(total));
        }
}

// ─────────────────────────────────────────────────────────────
// emit_expr
// ─────────────────────────────────────────────────────────────
TEST_CASE("emit_expr – simple signatures") {
    REQUIRE(emit_expr("BLL", /*opIdx*/ 0,
                      /*labels*/ std::vector<int>{0, 1}) == "AND(A,B)");

    REQUIRE(emit_expr("UL", 0, std::vector<int>{0}) == "NOT(A)");
}

// ─────────────────────────────────────────────────────────────
// emit_expr
// ─────────────────────────────────────────────────────────────

TEST_CASE("emit_expr – structure recoverable") {
    struct Sample {
        const char *sig;
        bigint op;
    } cases[] = {
        {"BLL", 5},
        {"UL", 0},
        {"BULL", 1},
    };

    for (auto const &c : cases) {
        size_t leaves = std::count(c.sig, c.sig + std::strlen(c.sig), 'L');
        auto txt = emit_expr(c.sig, c.op, make_labels(leaves));

        auto rec = recover_sig(txt);
        REQUIRE(rec == c.sig);
    }
}

// ─────────────────────────────────────────────────────────────
// neighbour_ok
// ─────────────────────────────────────────────────────────────
TEST_CASE("neighbour_ok – positive & negative examples") {
    REQUIRE(neighbour_ok("AND(A,B)"));
    REQUIRE(!neighbour_ok("AND(A,C)"));
    REQUIRE(neighbour_ok("A"));
}
TEST_CASE("neighbour_ok – every lonely letter rejected") {
    for (char c = 'B'; c <= 'Z'; ++c) {
        std::string s(1, c);
        REQUIRE(!neighbour_ok(s));
    }
}
// ─────────────────────────────────────────────────────────────
// nth_expression
// ─────────────────────────────────────────────────────────────
TEST_CASE("nth_expression – uniqueness & rule for first 10") {
    std::unordered_set<std::string> seen;
    for (bigint i = 0; i < 10; ++i) {
        auto e = nth_expression(i);
        REQUIRE(seen.insert(e).second);
        REQUIRE(neighbour_ok(e));
    }
}
TEST_CASE("nth_expression – size matches rank-partition for n≤7") {
    for (int n = 0; n <= 3; ++n)

        for (bigint idx = (n ? prefixN[n - 1] : 0); idx < prefixN[n]; ++idx) {
            auto expr = nth_expression(idx);
            int got = internal_size(expr);
            INFO("n=" << n << " idx=" << idx << " expr=" << expr
                      << " size=" << got);
            REQUIRE(got == n);
        }
}
