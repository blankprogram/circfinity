#include "compute_data.h"
#include <array>
#include <cassert>
#include <chrono>
#include <compute.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

/* bigint → decimal string, no heap */
std::string to_string(bigint x) {
    char buf[500];
    char *p = std::end(buf);
    do {
        *--p = char('0' + x % 10);
        x /= 10;
    } while (x);
    return {p, std::end(buf)};
}

/* unrank restricted-growth string of length len */
std::vector<int> unrank_rgs(int len, bigint k) {
    std::vector<int> r(len);
    int cur = 0;
    for (int i = 0; i < len; ++i) {
        for (int v = 0;; ++v) {
            bigint cnt = DP_RGS[len - i - 1][std::max(cur, v)];
            if (k < cnt) {
                r[i] = v;
                if (v == cur + 1)
                    ++cur;
                break;
            }
            k -= cnt;
        }
    }
    return r;
}

/* unrank tree shape with s leaves, u unary nodes  → preorder code {L,U,B}* */
std::string unrank_shape(int s, int u, bigint k) {
    if (s == 1)
        return u ? "U" + unrank_shape(1, u - 1, k) : "L";
    if (u) {
        bigint c = C[s][u - 1];
        if (k < c)
            return "U" + unrank_shape(s, u - 1, k);
        k -= c;
    }
    for (int ls = 1; ls < s; ++ls) {
        int rs = s - ls;
        for (int u1 = 0; u1 <= u; ++u1) {
            bigint block = C[ls][u1] * C[rs][u - u1];
            if (k < block) {
                bigint l = k / C[rs][u - u1], r = k % C[rs][u - u1];
                return "B" + unrank_shape(ls, u1, l) +
                       unrank_shape(rs, u - u1, r);
            }
            k -= block;
        }
    }
    throw;
}

/* emit fully-parenthesised infix, uses fixed array as stack */
std::string emit_expr(const std::string &sig, bigint opIdx,
                      const std::vector<int> &lbl) {
    static constexpr char OPS[] = "AND(OR(XOR(";
    static constexpr uint8_t off[3] = {0, 4, 7}, len[3] = {4, 3, 4};

    std::string out;
    out.reserve(64);

    std::array<char, 2 * MAX_S + MAX_U + 8> st{};
    int top = 0;
    size_t sigPos = 0, lblPos = 0;

    auto nextOp = [&] {
        int v = int(opIdx % 3);
        opIdx /= 3;
        return v;
    };

    while (top >= 0) {
        if (st[top] == 0) {
            char t = sig[sigPos++];
            st[top] = 1;

            if (t == 'L') {
                out += Labels[lbl[lblPos++]];
                --top;
            } else if (t == 'U') {
                out.append("NOT(", 4);
                st[++top] = ')';
                st[++top] = 0;
            } else {
                int o = nextOp();
                out.append(&OPS[off[o]], len[o]);
                st[++top] = ')';
                st[++top] = 0;
                st[++top] = ',';
                st[++top] = 0;
            }
        } else {
            out.push_back(st[top--]);
        }
    }
    return out;
}
/* N ↦ expression, enumeration order = by total size n then lexicographic */
std::string nth_expression(bigint N) {
    int n = 0, hi = MAX_N;
    while (n < hi) {
        int m = (n + hi) / 2;
        (prefixN[m] > N) ? hi = m : n = m + 1;
    }
    bigint rem = N - (n ? prefixN[n - 1] : 0);

    int sSel = 0, uSel = -1, bSel = 0;
    for (int u = n; u >= 0; --u) {
        int s = n - u + 1, b = n - u;
        if (s > MAX_S || u > MAX_U)
            continue;

        bigint blk = C[s][u] * Pow3[b] * Bell[s];
        if (rem < blk) {
            sSel = s;
            uSel = u;
            bSel = b;
            break;
        }
        rem -= blk;
    }

    bigint shapeIdx = rem / (Pow3[bSel] * Bell[sSel]);
    bigint tmp = rem % (Pow3[bSel] * Bell[sSel]);
    bigint opIdx = tmp / Bell[sSel];
    bigint rgsIdx = tmp % Bell[sSel];

    std::string sig = unrank_shape(sSel, uSel, shapeIdx);
    std::vector<int> labels = unrank_rgs(sSel, rgsIdx);

    return emit_expr(sig, opIdx, labels);
}

/* Hamming weight trick to check neighbour rule */
bool neighbour_ok(std::string_view e) {
    uint32_t m = 0;
    auto a = [](char c) { return c >= 'A' && c <= 'Z'; };
    for (size_t i = 0; i < e.size(); ++i)
        if (a(e[i]) && !(i && a(e[i - 1])) &&
            !(i + 1 < e.size() && a(e[i + 1])))
            m |= 1u << (e[i] - 'A');
    for (int i = 1; i < 26; ++i)
        if ((m >> i) & 1u && !(m >> (i - 1) & 1u) &&
            !(i < 25 && (m >> (i + 1) & 1u)))
            return false;
    return true;
}

int bit_length(const bigint &v) noexcept {
    return v == 0 ? 0 : boost::multiprecision::msb(v) + 1;
}

int main() {
    const bigint PRINT = 100;
    std::unordered_set<std::string> seen;

    if (PRINT <= std::numeric_limits<std::size_t>::max())
        seen.reserve(PRINT.convert_to<std::size_t>());

    bool ok = true;
    const auto t0 = std::chrono::high_resolution_clock::now();

    for (bigint i = 0; i < PRINT; ++i) {
        const std::string expr = nth_expression(i);
        std::cout << '#' << to_string(i + 1) << ": " << expr << '\n';

        if (!seen.insert(expr).second)
            std::cerr << "DUPLICATE @ rank " << to_string(i) << '\n';

        if (!neighbour_ok(expr)) {
            ok = false;
            std::cerr << "NEIGHBOUR RULE VIOLATION @ rank " << to_string(i)
                      << '\n';
        }
    }

    const bigint tot = prefixN[MAX_N];
    const bigint last = tot - 1;

    const int bits = bit_length(last);
    const double pct = bits * 100.0 / 128.0;

    std::cout << "\nHighest rank uses " << bits << " /128 bits (" << std::fixed
              << std::setprecision(3) << pct << "%)\n\n";

    std::cout << '#' << to_string(tot) << ": " << nth_expression(last)
              << "\n\n";

    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::high_resolution_clock::now() - t0)
                          .count();

    std::cout << std::fixed << std::setprecision(3) << "Printed "
              << to_string(PRINT) << " expressions in " << ms << " ms\n";

    std::cout << (seen.size() == PRINT ? "No duplicates : )\n"
                                       : "Duplicates found : (\n");
    std::cout << (ok ? "All expressions satisfy neighbour rule : )\n"
                     : "Neighbour-rule violations found : (\n");
}
