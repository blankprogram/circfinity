#include "uint128_t.h"
#include <catch2/catch_all.hpp>
#include <limits>
#include <vector>

using u128 = uint128_t;
static constexpr uint64_t U64MAX = std::numeric_limits<uint64_t>::max();
static const u128 ZERO{0, 0};
static const u128 ONE{0, 1};
static const u128 MAX128{U64MAX, U64MAX};

std::string to_string128(__int128 v) {
    if (v == 0)
        return "0";
    bool neg = v < 0;
    unsigned __int128 u = neg ? __int128(-v) : __int128(v);
    std::string s;
    while (u) {
        int digit = int(u % 10);
        s.push_back('0' + digit);
        u /= 10;
    }
    if (neg)
        s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
}
//-----------------------------------------------------------------------------
// Construction & Conversion
//-----------------------------------------------------------------------------

TEST_CASE("Construction & conversion", "[ctor]") {
    SECTION("default is zero") {
        u128 x;
        REQUIRE(x == ZERO);
    }
    SECTION("from single uint64_t") {
        u128 x(0x12345678ULL);
        REQUIRE(x.low == 0x12345678ULL);
        REQUIRE(x.high == 0);
    }
    SECTION("from hi,lo pair") {
        u128 x{0xDEADBEEFULL, 0xCAFEBABELL};
        REQUIRE(x.high == 0xDEADBEEFULL);
        REQUIRE(x.low == 0xCAFEBABELL);
    }
    SECTION("implicit from integer literal") {
        u128 a = 1;
        REQUIRE(a == ONE);
        u128 b = 42;
        REQUIRE(b.high == 0);
        REQUIRE(b.low == 42);
    }
    SECTION("brace-init from single element") {
        u128 c{7};
        REQUIRE(c == u128{0, 7});
    }
    SECTION("copy constructor") {
        u128 src{5, 6};
        u128 dst = src;
        REQUIRE(dst.high == 5);
        REQUIRE(dst.low == 6);
    }
    SECTION("assignment from uint64_t") {
        u128 x;
        x = 99;
        REQUIRE(x == u128{0, 99});
    }
    SECTION("self-assignment") {
        u128 x{2, 3};
        x = x;
        REQUIRE(x == u128{2, 3});
    }
}

//-----------------------------------------------------------------------------
// Comparison operators
//-----------------------------------------------------------------------------

TEST_CASE("Zero vs One comparisons", "[cmp]") {
    REQUIRE(ZERO < ONE);
    REQUIRE(ONE > ZERO);
    REQUIRE(ZERO != ONE);
    REQUIRE(!(ZERO > ONE));
    REQUIRE(!(ONE < ZERO));
}

TEST_CASE("Equality and inequality", "[cmp]") {
    REQUIRE(ZERO == u128{0, 0});
    REQUIRE(!(ZERO != u128{0, 0}));
    REQUIRE(MAX128 == u128{U64MAX, U64MAX});
    REQUIRE(!(MAX128 != u128{U64MAX, U64MAX}));
}

TEST_CASE("High-part ordering", "[cmp]") {
    REQUIRE(u128{0, 5} < u128{1, 0});
    REQUIRE(u128{1, 0} > u128{0, 5});
    REQUIRE(u128{0, U64MAX} < u128{1, 0});
    REQUIRE(u128{1, 0} > u128{0, U64MAX});
}

TEST_CASE("Low-part ordering with equal highs", "[cmp]") {
    REQUIRE(u128{2, 3} < u128{2, 4});
    REQUIRE(u128{2, 4} > u128{2, 3});
    REQUIRE(u128{7, 0} < u128{7, U64MAX});
    REQUIRE(u128{7, U64MAX} > u128{7, 0});
}

TEST_CASE("Boundary crossing equality", "[cmp]") {
    REQUIRE(u128{3, 5} <= u128{3, 5});
    REQUIRE(u128{3, 5} >= u128{3, 5});
}

TEST_CASE("Random example comparisons", "[cmp]") {
    REQUIRE(u128{5, 123} < u128{5, 124});
    REQUIRE(u128{5, 124} > u128{5, 123});
    REQUIRE(u128{U64MAX, 0} < u128{U64MAX, 1});
    REQUIRE(u128{U64MAX, 1} > u128{U64MAX, 0});
}

//-----------------------------------------------------------------------------
// Addition & Subtraction
//-----------------------------------------------------------------------------

TEST_CASE("Addition high-only values", "[add]") {
    u128 a{1, 0}, b{2, 0};
    auto sum = a + b;
    REQUIRE(sum.high == 3);
    REQUIRE(sum.low == 0);
}

TEST_CASE("Addition with mixed high/low and carry", "[add]") {
    u128 a{1, U64MAX}, b{2, 5};
    u128 sum = a + b;
    REQUIRE(sum.high == 4);
    REQUIRE(sum.low == 4);
}

TEST_CASE("Addition arbitrary no carry", "[add]") {
    u128 a{5, 100}, b{5, 200};
    u128 sum = a + b;
    REQUIRE(sum.high == 10);
    REQUIRE(sum.low == 300);
}

TEST_CASE("Subtraction to zero", "[sub]") {
    u128 x{7, 12345};
    REQUIRE(x - x == ZERO);
}

TEST_CASE("Subtraction with simple no borrow", "[sub]") {
    u128 a{5, 300}, b{5, 100};
    u128 diff = a - b;
    REQUIRE(diff.high == 0);
    REQUIRE(diff.low == 200);
}

TEST_CASE("Subtraction with borrow from high", "[sub]") {
    u128 a{2, 0}, b{1, U64MAX};
    u128 diff = a - b;
    REQUIRE(diff.high == 0);
    REQUIRE(diff.low == 1);
}

//-----------------------------------------------------------------------------
// Multiplication
//-----------------------------------------------------------------------------

TEST_CASE("Multiplication identity", "[mul]") {
    u128 x{0x12345678ULL, 0x9ABCDEF0ULL};
    REQUIRE(x * ONE == x);
    REQUIRE(ONE * x == x);
}

TEST_CASE("Multiplication by uint64_t overload", "[mul][small]") {
    u128 x{3, 5};
    uint64_t m = 7;

    uint64_t low = x.low * m;
    uint64_t high =
        uint64_t((__uint128_t(x.high) * m) + ((__uint128_t(x.low) * m) >> 64));

    REQUIRE(x * m == u128{high, low});
    REQUIRE(m * x == x * m);
}
TEST_CASE("Cross-term boundary", "[mul][cross]") {
    u128 a{0xFFFFFFFFULL, 0x00000000ULL};
    u128 b{0x00000000ULL, 0xFFFFFFFFULL};
    u128 prod = a * b;
    __uint128_t bigA = (__uint128_t(a.high) << 64) | a.low;
    __uint128_t bigB = (__uint128_t(b.high) << 64) | b.low;
    __uint128_t bigP = bigA * bigB;
    REQUIRE(prod.high == uint64_t(bigP >> 64));
    REQUIRE(prod.low == uint64_t(bigP));
}

TEST_CASE("Random small multiplication", "[mul][random]") {
    uint64_t a_lo = 0x1234567890ABCDEFULL;
    uint64_t b_lo = 0x0FEDCBA098765432ULL;
    u128 a{0, a_lo}, b{0, b_lo};
    u128 prod = a * b;
    __uint128_t bigA = a_lo, bigB = b_lo;
    __uint128_t bigP = bigA * bigB;
    REQUIRE(prod.high == uint64_t(bigP >> 64));
    REQUIRE(prod.low == uint64_t(bigP));
}

TEST_CASE("Multiplication is commutative", "[mul][commute]") {
    u128 a{7, 0x11111111ULL};
    u128 b{3, 0x22222222ULL};
    REQUIRE(a * b == b * a);
}

//-----------------------------------------------------------------------------
// Bit‐shifts
//-----------------------------------------------------------------------------

TEST_CASE("Shift by zero", "[shl][shr]") {
    u128 x{0x123, 0x456};
    REQUIRE((x << 0) == x);
    REQUIRE((x >> 0) == x);
}

TEST_CASE("Shift within low 64 bits", "[shl][shr]") {
    u128 one{0, 1};
    REQUIRE((one << 4).low == 16);
    REQUIRE((one << 4).high == 0);
    u128 highbit{1, 0};
    auto r = highbit >> 4;
    REQUIRE(r.high == 0);
    REQUIRE(r.low == (1ULL << 60));
}

TEST_CASE("Shift across 64‐bit boundary", "[shl][shr]") {
    u128 one{0, 1};
    auto y1 = one << 64;
    REQUIRE((y1.high == 1 && y1.low == 0));

    u128 two64{1, 0};
    auto y2 = two64 >> 64;
    REQUIRE((y2.high == 0 && y2.low == 1));
}

TEST_CASE("Full‐width shifts yield zero", "[shl][shr]") {
    REQUIRE((MAX128 << 128) == ZERO);
    REQUIRE((MAX128 >> 128) == ZERO);
}

TEST_CASE("Shift‐and‐restore", "[shl][shr]") {
    u128 F{0xFEDCBA98ULL, 0x76543210ULL};
    for (int s : {0, 1, 7, 31, 32}) {
        REQUIRE(((F << s) >> s) == F);
    }
}

//-----------------------------------------------------------------------------
// Division & Modulus
//-----------------------------------------------------------------------------

TEST_CASE("Divide and mod by power-of-two", "[divmod][pow2]") {
    u128 P96{uint64_t(1) << 32, 0};
    u128 D32{0, uint64_t(1) << 32};
    auto [q1, r1] = P96.divmod(D32);
    REQUIRE(q1 == u128{1, 0});
    REQUIRE(r1 == ZERO);

    u128 N2 = P96 + u128{0, 123};
    auto [q2, r2] = N2.divmod(D32);
    REQUIRE(q2 == q1);
    REQUIRE(r2.low == 123);
    REQUIRE(r2.high == 0);
}

TEST_CASE("Divide MAX128 by MAX128−1", "[divmod][edge]") {
    u128 M1{U64MAX, U64MAX - 1};
    auto [q, r] = MAX128.divmod(M1);
    REQUIRE(q == ONE);
    REQUIRE(r == ONE);
}

TEST_CASE("Quotient*divisor + remainder == numerator",
          "[divmod][reconstruct]") {
    std::vector<std::pair<u128, u128>> cases = {
        {u128{3, 0xF00D1234ULL}, u128{0, 0xABCDEFFF12345678ULL}},
        {u128{U64MAX / 2, U64MAX / 3}, u128{0, 1234567890ULL}},
        {MAX128, u128{0, 999999937ULL}}};
    for (auto [N, D] : cases) {
        if (D == ZERO)
            continue;
        auto [q, r] = N.divmod(D);
        REQUIRE(q * D + r == N);
        REQUIRE(r < D);
    }
}

TEST_CASE("Random 128-bit divisor examples", "[divmod][randomfull]") {
    u128 N1{0x12345678ULL, 0x9ABCDEF012345678ULL};
    u128 D1{0x0FEDCBA9ULL, 0x876543210FEDCBA9ULL};
    {
        auto [q, r] = N1.divmod(D1);
        __uint128_t bigN1 = (__uint128_t(N1.high) << 64) | N1.low;
        __uint128_t bigD1 = (__uint128_t(D1.high) << 64) | D1.low;
        REQUIRE(q.to_string() == to_string128(bigN1 / bigD1));
        REQUIRE(r.to_string() == to_string128(bigN1 % bigD1));
    }
    u128 N2{0xDEADBEEFULL, 0xCAFEBABEC0FFEE00ULL};
    u128 D2{0x11111111ULL, 0x22222222AAAAAAAAULL};
    {
        auto [q, r] = N2.divmod(D2);
        __uint128_t bigN2 = (__uint128_t(N2.high) << 64) | N2.low;
        __uint128_t bigD2 = (__uint128_t(D2.high) << 64) | D2.low;
        REQUIRE(q.to_string() == to_string128(bigN2 / bigD2));
        REQUIRE(r.to_string() == to_string128(bigN2 % bigD2));
    }
}

//-----------------------------------------------------------------------------
// Increment & Boolean
//-----------------------------------------------------------------------------

TEST_CASE("Prefix and postfix increment edge cases", "[inc]") {
    u128 x{0, U64MAX - 1};
    u128 &pref = ++x;
    REQUIRE(&pref == &x);
    REQUIRE(x == u128{0, U64MAX});

    ++x;
    REQUIRE(x == u128{1, 0});

    u128 y{0, U64MAX};
    u128 old = y++;
    REQUIRE(old == u128{0, U64MAX});
    REQUIRE(y == u128{1, 0});

    y = u128{U64MAX, U64MAX};
    ++y;
    REQUIRE(y == u128{0, 0});
}

TEST_CASE("Prefix and postfix decrement edge cases", "[dec]") {
    u128 z{0, 0};
    u128 &pref = --z;
    REQUIRE(&pref == &z);
    REQUIRE(z == u128{U64MAX, U64MAX});

    u128 w{1, 0};
    u128 oldw = w--;
    REQUIRE(oldw == u128{1, 0});
    REQUIRE(w == u128{0, U64MAX});

    u128 a{5, 10};
    --a;
    REQUIRE(a == u128{5, 9});

    u128 b{0, 0};
    b--;
    b--;
    REQUIRE(b == u128{U64MAX, U64MAX - 1});
}

TEST_CASE("Compound add and mixed boundary adds", "[add]") {
    u128 x{0, U64MAX};
    x += u128{0, 1};
    REQUIRE(x == u128{1, 0});

    x = u128{U64MAX - 1, U64MAX - 5};
    x += u128{2, 10};
    REQUIRE(x == u128{1, 4});

    x = u128{123, 456};
    x += u128{0, 0};
    REQUIRE(x == u128{123, 456});
}

TEST_CASE("Boolean conversion & negation", "[bool]") {
    u128 z{0, 0}, o1{0, 1}, o2{1, 0}, o3{1, 1};
    REQUIRE(!z);
    REQUIRE(z == u128{0, 0});
    REQUIRE(!static_cast<bool>(z));

    REQUIRE(static_cast<bool>(o1));
    REQUIRE(static_cast<bool>(o2));
    REQUIRE(static_cast<bool>(o3));
    REQUIRE(!!o1);
    REQUIRE(!!o2);
    REQUIRE(!!o3);
}

//-----------------------------------------------------------------------------
// to_string
//-----------------------------------------------------------------------------

TEST_CASE("to_string power-of-two values", "[to_string][pow2]") {
    u128 v64{1, 0};
    REQUIRE(v64.to_string() == "18446744073709551616");

    u128 v127{1ULL << 63, 0};
    REQUIRE(v127.to_string() == "170141183460469231731687303715884105728");
}

TEST_CASE("to_string around decimal boundary", "[to_string][seq]") {
    for (uint64_t i = 95; i <= 105; ++i) {
        u128 v{i};
        REQUIRE(v.to_string() == std::to_string(i));
    }
}

TEST_CASE("to_string mid-range combination", "[to_string][pattern]") {
    u128 v{0x12345678ULL, 0x9ABCDEF0ULL};
    auto s = v.to_string();
    REQUIRE(s == "5634002656530987591361421040");
    REQUIRE(s.size() == 28);
    REQUIRE(s.substr(0, 3) == "563");
    REQUIRE(s.substr(s.size() - 10) == "1361421040");
}

TEST_CASE("to_string after decrementing MAX128", "[to_string][dec]") {
    u128 m = MAX128;
    --m;
    auto s = m.to_string();
    REQUIRE(s == "340282366920938463463374607431768211454");
}
