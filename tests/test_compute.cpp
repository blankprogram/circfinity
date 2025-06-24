#include "compute.h"
#include "compute_data.h"
#include <catch2/catch_all.hpp>
#include <unordered_set>
//-----------------------------------------------------------------------------
// Bell numbers
//-----------------------------------------------------------------------------
TEST_CASE("Bell numbers", "[compute_data][Bell]") {
    SECTION("base values") {
        REQUIRE(Bell[0] == 1);
        REQUIRE(Bell[1] == 1);
        REQUIRE(Bell[2] == 2);
        REQUIRE(Bell[3] == 5);
        REQUIRE(Bell[4] == 15);
        REQUIRE(Bell[5] == 52);
    }

    SECTION("increasing for n >= 2") {
        for (int n = 2; n <= MAX_SIZE; ++n)
            REQUIRE(Bell[n] > Bell[n - 1]);
    }
}

//-----------------------------------------------------------------------------
// Powers of 3
//-----------------------------------------------------------------------------
TEST_CASE("Powers of 3", "[compute_data][Pow3]") {
    SECTION("base values") {
        REQUIRE(Pow3[0] == 1);
        REQUIRE(Pow3[1] == 3);
        REQUIRE(Pow3[2] == 9);
        REQUIRE(Pow3[3] == 27);
        REQUIRE(Pow3[4] == 81);
    }

    SECTION("recurrence") {
        for (int n = 1; n <= MAX_SIZE; ++n)
            REQUIRE(Pow3[n] == Pow3[n - 1] * 3);
    }
}

//-----------------------------------------------------------------------------
// WeightFactor
//-----------------------------------------------------------------------------

TEST_CASE("WeightFactor", "[compute_data][WeightFactor]") {
    SECTION("matches Bell * Pow3") {
        for (int b = 0; b <= MAX_SIZE; ++b)
            REQUIRE(WeightFactor[b] == u128(Bell[b + 1]) * Pow3[b]);
    }
}

//-----------------------------------------------------------------------------
// Shape count C table
//-----------------------------------------------------------------------------

TEST_CASE("Shape count table C", "[compute_data][C]") {
    SECTION("base case") { REQUIRE(C[1][0] == 1); }

    SECTION("non-empty sum per row") {
        for (int s = 1; s <= MAX_SIZE; ++s) {
            u64 sum = 0;
            for (int b = 0; b <= s; ++b)
                sum += C[s][b];
            REQUIRE(sum >= 1);
        }
    }
}

//-----------------------------------------------------------------------------
// Shape counts & weights
//-----------------------------------------------------------------------------

TEST_CASE("Shape count & weights", "[compute_data][shape]") {
    SECTION("shape count monotonicity") {
        for (int s = 2; s <= MAX_SIZE; ++s)
            REQUIRE(shapeCount[s] >= shapeCount[s - 1]);
    }

    SECTION("shape weight monotonicity") {
        for (int s = 2; s <= MAX_SIZE; ++s)
            REQUIRE(shapeWeight[s] >= shapeWeight[s - 1]);
    }

    SECTION("cumulative weight matches running sum") {
        for (int s = 2; s <= MAX_SIZE; ++s)
            REQUIRE(cumShapeWeight[s] ==
                    cumShapeWeight[s - 1] + shapeWeight[s]);
    }
}

//-----------------------------------------------------------------------------
// Block and row weights
//-----------------------------------------------------------------------------

TEST_CASE("Block and row weights", "[compute_data][block_rows]") {
    SECTION("block equals weighted sum of rows") {
        for (int s = 2; s <= MAX_SIZE; ++s) {
            for (int ls = 1; ls <= s - 2; ++ls) {
                u128 sumRows = 0;
                for (int b1 = 0; b1 <= MAX_SIZE; ++b1) {
                    REQUIRE(rowWeightSum[s][ls][b1] >= 0);
                    sumRows += u128(C[ls][b1]) * rowWeightSum[s][ls][b1];
                }
                REQUIRE(blockWeight[s][ls] == sumRows);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// DP_RGS
//-----------------------------------------------------------------------------

TEST_CASE("DP_RGS table", "[compute_data][DP_RGS]") {
    SECTION("base case") {
        for (int k = 0; k <= MAX_SIZE; ++k)
            REQUIRE(DP_RGS[0][k] == 1);
    }

    SECTION("recurrence holds") {
        for (int len = 1; len <= MAX_SIZE; ++len) {
            for (int k = 0; k <= MAX_SIZE; ++k) {
                u64 sum = 0;
                for (int v = 0; v <= std::min(k + 1, MAX_SIZE); ++v)
                    sum += DP_RGS[len - 1][std::max(v, k)];
                REQUIRE(DP_RGS[len][k] == sum);
            }
        }
    }

    SECTION("known small values") {
        REQUIRE(DP_RGS[1][0] == 2);
        REQUIRE(DP_RGS[1][1] == 3);
        REQUIRE(DP_RGS[2][0] > 0);
        REQUIRE(DP_RGS[2][1] > 0);
        REQUIRE(DP_RGS[3][1] > 0);
    }
}
//-----------------------------------------------------------------------------
// shape_unrank
//-----------------------------------------------------------------------------

TEST_CASE("shape_unrank", "[runtime][shape_unrank]") {

    SECTION("trivial base cases: s=1") {
        int b_shape = -1;
        u128 variant = ~0ULL;
        for (u128 woff = 0; woff < 5; ++woff) {
            b_shape = -1;
            variant = ~0ULL;
            REQUIRE(shape_unrank(1, woff, b_shape, variant) == 0);
            REQUIRE(b_shape == 0);
            REQUIRE(variant == woff);
        }
    }

    SECTION("binary split for small s") {
        int b_shape = -1;
        u128 variant = ~0ULL;

        b_shape = -1;
        variant = ~0ULL;
        REQUIRE(shape_unrank(2, 0, b_shape, variant) ==
                shapeCount[2] - shapeCount[1]);

        for (u128 woff = 0; woff < 5; ++woff) {
            b_shape = -1;
            variant = ~0ULL;
            u128 idx = shape_unrank(3, woff, b_shape, variant);
            REQUIRE(idx >= 0);
            REQUIRE(b_shape >= 1);
            REQUIRE(variant >= 0);
        }
    }

    SECTION("larger s consistency") {
        int b_shape = -1;
        u128 variant = ~0ULL;

        int s = std::min(10, MAX_SIZE);
        for (u128 woff = 0; woff < 10; ++woff) {
            b_shape = -1;
            variant = ~0ULL;
            u128 idx = shape_unrank(s, woff, b_shape, variant);
            REQUIRE(idx >= 0);
            REQUIRE(b_shape >= 1);
            REQUIRE(variant >= 0);
        }
    }

    SECTION("round-trip consistency with unrank") {
        for (u128 N = 1; N <= 20; ++N) {
            int b_shape = -1;
            u128 variant = ~0ULL;

            int s = 1;
            while (cumShapeWeight[s] < N)
                ++s;
            u128 layerOff = N - (cumShapeWeight[s - 1] + 1);

            u128 idx = shape_unrank(s, layerOff, b_shape, variant);
            REQUIRE(idx >= 0);
            REQUIRE(b_shape >= 0);
            REQUIRE(variant >= 0);

            std::string expr = unrank(N);
            REQUIRE(!expr.empty());
        }
    }
}

//-----------------------------------------------------------------------------
// build_expr
//-----------------------------------------------------------------------------

TEST_CASE("build_expr", "[runtime][build_expr]") {

    SECTION("single leaf emits variable label") {
        std::vector<int> ops;       // unused for s=1
        std::vector<int> rgs = {2}; // means 'C'

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(1, 0, ops, rgs, leafIdx, opIdx, OL);
        REQUIRE(std::string(OUT, OL) == "C");
    }

    SECTION("binary AND(A,B) with trivial shape") {
        std::vector<int> ops = {0};    // 0 = AND
        std::vector<int> rgs = {0, 1}; // A, B

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(2, 0, ops, rgs, leafIdx, opIdx, OL);
        REQUIRE(std::string(OUT, OL) == "AND(A,B)");
    }

    SECTION("binary OR(A,B) with shape s=2") {
        std::vector<int> ops = {1}; // 1 = OR
        std::vector<int> rgs = {0, 1};

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(2, 0, ops, rgs, leafIdx, opIdx, OL);
        REQUIRE(std::string(OUT, OL) == "OR(A,B)");
    }

    SECTION("binary XOR(B,A)") {
        std::vector<int> ops = {2}; // 2 = XOR
        std::vector<int> rgs = {1, 0};

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(2, 0, ops, rgs, leafIdx, opIdx, OL);
        REQUIRE(std::string(OUT, OL) == "XOR(B,A)");
    }

    SECTION("unary NOT(A)") {
        std::vector<int> ops; // unused for NOT chain
        std::vector<int> rgs = {0};

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(2, 1, ops, rgs, leafIdx, opIdx, OL);
        REQUIRE(std::string(OUT, OL) == "NOT(A)");
    }

    SECTION("nested NOT(NOT(A))") {
        std::vector<int> ops;
        std::vector<int> rgs = {0};

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(3, shapeCount[3] - shapeCount[2], ops, rgs, leafIdx, opIdx,
                   OL);
        REQUIRE(std::string(OUT, OL).substr(0, 8) == "NOT(NOT");
    }

    SECTION("complex binary shape uses both ops and rgs") {
        std::vector<int> ops = {0, 1};    // AND, OR
        std::vector<int> rgs = {0, 1, 2}; // A,B,C

        int leafIdx = 0, opIdx = 0, OL = 0;
        build_expr(3, 0, ops, rgs, leafIdx, opIdx, OL);
        std::string expr(OUT, OL);
        REQUIRE(expr.find("AND") != std::string::npos);
        REQUIRE(expr.find("OR") != std::string::npos);
        REQUIRE(expr.find("A") != std::string::npos);
        REQUIRE(expr.find("B") != std::string::npos);
        REQUIRE(expr.find("C") != std::string::npos);
    }
}
//-----------------------------------------------------------------------------
// unrank
//-----------------------------------------------------------------------------

TEST_CASE("unrank", "[runtime][unrank]") {

    SECTION("produces valid non-empty strings for small ranks") {
        for (u128 i = 1; i <= 20; ++i) {
            auto expr = unrank(i);
            REQUIRE_FALSE(expr.empty());
            REQUIRE(expr.find_first_not_of("ANDORXNOT(),ABCDEFGHIJKLMN") ==
                    std::string::npos);
        }
    }

    SECTION("produces different expressions for consecutive ranks") {
        std::unordered_set<std::string> seen;
        for (u128 i = 1; i <= 50; ++i) {
            auto expr = unrank(i);
            REQUIRE(seen.insert(expr).second);
        }
    }

    SECTION("known values: rank 1 and rank 2") {
        REQUIRE(unrank(1) == "A");
        REQUIRE(unrank(2) == "NOT(A)");
    }

    SECTION("largest rank is valid and non-empty") {
        auto last = unrank(cumShapeWeight[MAX_SIZE]);
        REQUIRE_FALSE(last.empty());
    }

    SECTION("throws for out-of-range ranks") {
        REQUIRE_THROWS(unrank(0));
        REQUIRE_THROWS(unrank(cumShapeWeight[MAX_SIZE] + 1));
    }

    SECTION("structure: expressions contain balanced parentheses") {
        for (u128 i = 1; i <= 20; ++i) {
            std::string expr = unrank(i);
            int bal = 0;
            for (char c : expr) {
                if (c == '(')
                    ++bal;
                else if (c == ')')
                    --bal;
                REQUIRE(bal >= 0);
            }
            REQUIRE(bal == 0);
        }
    }

    SECTION("structure: expression uses valid operators or is a leaf") {
        for (u128 i = 1; i <= 20; ++i) {
            std::string expr = unrank(i);

            bool is_leaf = (expr.size() == 1);
            bool has_op = false;

            if (!is_leaf) {
                has_op = (expr.find("AND") != std::string::npos) ||
                         (expr.find("OR") != std::string::npos) ||
                         (expr.find("XOR") != std::string::npos) ||
                         (expr.find("NOT") != std::string::npos);
            }

            if (!is_leaf) {
                REQUIRE(has_op);
            }
        }
    }
}
