#include "compute.h"
#include "compute_data.h"
#include <catch2/catch_all.hpp>
#include <unordered_set>
#include <vector>

// helpers ---------------------------------------------------------------
static u128 ipow(u128 b, int e) {
    u128 r = 1;
    while (e) {
        if (e & 1)
            r *= b;
        b *= b;
        e >>= 1;
    }
    return r;
}
constexpr u128 binom(int n, int k) {
    if (k < 0 || k > n)
        return 0;
    if (k > n - k)
        k = n - k;
    u128 r = 1;
    for (int i = 1; i <= k; ++i)
        r = r * u128(n - i + 1) / u128(i);
    return r;
}
static std::string strip(std::string_view s) {
    std::string r;
    for (char c : s)
        if (c != '(' && c != ')' && c != ',')
            r.push_back(c);
    return r;
}

struct VecHash {
    std::size_t operator()(std::vector<int> const &v) const noexcept {
        std::size_t h = 0;
        for (int x : v) //  FNV-1a-like mix
            h = (h ^ std::size_t(x)) * 0x9E3779B97F4A7C15ULL;
        return h;
    }
};
// ─────────────────────────────────────────────────────────────
// Pow3
// ─────────────────────────────────────────────────────────────
TEST_CASE("Pow3 – matches 3^k for all k") {
    for (int k = 0; k <= MAX_S; ++k)
        REQUIRE(Pow3[k] == ipow(3, k));
    for (int k = 0; k < MAX_S; ++k)
        REQUIRE(Pow3[k + 1] / Pow3[k] == u128(3));
}
// ─────────────────────────────────────────────────────────────
// Bell
// ─────────────────────────────────────────────────────────────
TEST_CASE("Bell – triangle recomputation") {
    std::array<u128, MAX_S + 1> ref{};
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
    std::array<std::array<u128, MAX_U + 1>, MAX_S + 1> ref{};
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
TEST_CASE("DP_RGS – recurrence & Bell link") {
    for (int len = 1; len <= MAX_S; ++len)
        for (int m = 0; m <= MAX_S; ++m) {
            u128 rhs = 0;
            for (int v = 0; v <= m + 1; ++v)
                rhs += DP_RGS[len - 1][std::max(m, v)];
            REQUIRE(DP_RGS[len][m] == rhs);
        }
    for (int n = 0; n <= MAX_S; ++n)
        REQUIRE(DP_RGS[n][0] == Bell[n]);
}
// ─────────────────────────────────────────────────────────────
// Wn & prefixN
// ─────────────────────────────────────────────────────────────
TEST_CASE("Wn – closed-form cross-check") {
    for (int n = 0; n <= MAX_N; ++n) {
        u128 w = 0;
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
    REQUIRE(prefixN[MAX_N] > u128(0));
}
// ─────────────────────────────────────────────────────────────
// to_string
// ─────────────────────────────────────────────────────────────
TEST_CASE("to_string – round-trip small & large") {
    REQUIRE(to_string(u128(0)) == "0");
    REQUIRE(to_string(u128(12345678901234567890ULL)) == "12345678901234567890");
    REQUIRE(to_string(u128(1) << 127) ==
            "170141183460469231731687303715884105728");
}
// ─────────────────────────────────────────────────────────────
// bit_length
// ─────────────────────────────────────────────────────────────
TEST_CASE("bit_length – edge cases") {
    REQUIRE(bit_length(u128(0)) == 0);
    REQUIRE(bit_length(u128(1)) == 1);
    REQUIRE(bit_length(u128(2)) == 2);
    REQUIRE(bit_length((u128(1) << 127) - 1) == 127);
    REQUIRE(bit_length(u128(1) << 127) == 128);
}
// ─────────────────────────────────────────────────────────────
// to_string & bit_length (fuzz)
// ─────────────────────────────────────────────────────────────
TEST_CASE("to_string/bit_length – 1 000 pseudo-random values") {
    u128 x = 17;
    for (int i = 0; i < 1000; ++i) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        x ^= x << 37;
        auto s = to_string(x);
        u128 y = 0;
        for (char c : s)
            y = y * 10 + (c - '0');
        REQUIRE(y == x);
        REQUIRE(bit_length(y) == bit_length(x));
    }
}

// ─────────────────────────────────────────────────────────────
// unrank_rgs
// ─────────────────────────────────────────────────────────────
TEST_CASE("unrank_rgs – first & last for len=3") {
    REQUIRE(DP_RGS[3][0] == 5);
    REQUIRE(unrank_rgs(3, 0) == std::vector<int>({0, 0, 0}));
    REQUIRE(unrank_rgs(3, 4) == std::vector<int>({0, 1, 2}));
}

TEST_CASE("unrank_rgs – bijection for len≤5") {
    for (int len = 1; len <= 5; ++len) {
        std::unordered_set<std::vector<int>, VecHash> seen;
        u128 total = DP_RGS[len][0];
        for (u128 k = 0; k < total; ++k)
            REQUIRE(seen.insert(unrank_rgs(len, k)).second);
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
            u128 total = C[s][u];
            for (u128 k = 0; k < total; ++k)
                REQUIRE(seen.insert(unrank_shape(s, u, k)).second);
            REQUIRE(seen.size() == static_cast<std::size_t>(total));
        }
}
// ─────────────────────────────────────────────────────────────
// emit_expr
// ─────────────────────────────────────────────────────────────
TEST_CASE("emit_expr – simple signatures") {
    REQUIRE(emit_expr("BLL", 0, 1 * 26 + 0) == "AND(A,B)");
    REQUIRE(emit_expr("UL", 0, 0) == "NOT(A)");
}
TEST_CASE("emit_expr – structure recoverable") {
    struct T {
        std::string sig;
        u128 op;
        u128 lbl;
    };
    const T cases[] = {{"BLL", 5, 12}, {"UL", 0, 0}, {"BUL", 1, 7}};
    for (auto &d : cases) {
        auto txt = emit_expr(d.sig, d.op, d.lbl);
        auto rec = strip(txt);
        for (char &c : rec) {
            if (c >= 'A' && c <= 'Z')
                c = 'L';
            else if (c == 'N')
                c = 'U';
            else
                c = 'B';
        }
        REQUIRE(rec.substr(0, d.sig.size()) == d.sig);
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
static int internal_size(std::string_view e) {
    int bin = 0, un = 0;
    for (char c : e)
        if (c == '(')
            ++bin;
        else if (c == 'N')
            ++un;
    return bin + un;
}
TEST_CASE("nth_expression – uniqueness & rule for first 10") {
    std::unordered_set<std::string> seen;
    for (u128 i = 0; i < 10; ++i) {
        auto e = nth_expression(i);
        REQUIRE(seen.insert(e).second);
        REQUIRE(neighbour_ok(e));
    }
}
TEST_CASE("nth_expression – size matches rank-partition for n≤7") {
    for (int n = 0; n <= 7; ++n)
        for (u128 idx = (n ? prefixN[n - 1] : 0); idx < prefixN[n]; ++idx)
            REQUIRE(internal_size(nth_expression(idx)) == n);
}
