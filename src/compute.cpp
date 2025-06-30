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

/* u128 → decimal string, no heap */
std::string to_string(u128 x) {
    char buf[40];
    char *p = std::end(buf);
    do {
        *--p = char('0' + x % 10);
        x /= 10;
    } while (x);
    return {p, std::end(buf)};
}

/* unrank restricted-growth string of length len */
std::vector<int> unrank_rgs(int len, u128 k) {
    std::vector<int> r(len);
    int cur = 0;
    for (int i = 0; i < len; ++i) {
        for (int v = 0;; ++v) {
            u128 cnt = DP_RGS[len - i - 1][std::max(cur, v)];
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
std::string unrank_shape(int s, int u, u128 k) {
    if (s == 1)
        return u ? "U" + unrank_shape(1, u - 1, k) : "L";
    if (u) {
        u128 c = C[s][u - 1];
        if (k < c)
            return "U" + unrank_shape(s, u - 1, k);
        k -= c;
    }
    for (int ls = 1; ls < s; ++ls) {
        int rs = s - ls;
        for (int u1 = 0; u1 <= u; ++u1) {
            u128 block = C[ls][u1] * C[rs][u - u1];
            if (k < block) {
                u128 l = k / C[rs][u - u1], r = k % C[rs][u - u1];
                return "B" + unrank_shape(ls, u1, l) +
                       unrank_shape(rs, u - u1, r);
            }
            k -= block;
        }
    }
    throw;
}

/* emit fully-parenthesised infix, uses fixed array as stack */
std::string emit_expr(const std::string &sig, u128 op, u128 lbl) {
    static constexpr char OPS[] = "AND(OR(XOR(";
    static constexpr uint8_t off[3] = {0, 4, 7}, len[3] = {4, 3, 4};
    std::string out;
    out.reserve(64);
    std::array<char, 2 * MAX_S + MAX_U + 8> st{};
    int top = 0;
    size_t i = 0;

    auto nextOp = [&] {
        int v = int(op % 3);
        op /= 3;
        return v;
    };
    auto nextLb = [&] {
        int v = int(lbl % ALPHA);
        lbl /= ALPHA;
        return v;
    };

    while (top >= 0) {
        if (st[top] == 0) {
            char t = sig[i++];
            st[top] = 1;
            if (t == 'L') {
                out.push_back('A' + nextLb());
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
        } else
            out.push_back(st[top--]);
    }
    return out;
}

/* N ↦ expression, enumeration order = by total size n then lexicographic */
std::string nth_expression(u128 N) {
    int n = 0, hi = MAX_N;
    while (n < hi) {
        int m = (n + hi) / 2;
        (prefixN[m] > N) ? hi = m : n = m + 1;
    }
    u128 rem = N - (n ? prefixN[n - 1] : 0);

    int sSel = 0, uSel = -1, bSel = 0;
    for (int u = n; u >= 0; --u) {
        int s = n - u + 1, b = n - u;
        if (s > MAX_S || u > MAX_U)
            continue;
        u128 blk = C[s][u] * Pow3[b] * Bell[s];
        if (rem < blk) {
            sSel = s;
            uSel = u;
            bSel = b;
            break;
        }
        rem -= blk;
    }
    u128 shapeIdx = rem / (Pow3[bSel] * Bell[sSel]);
    u128 tmp = rem % (Pow3[bSel] * Bell[sSel]);
    u128 opIdx = tmp / Bell[sSel], rgsIdx = tmp % Bell[sSel];

    std::string sig = unrank_shape(sSel, uSel, shapeIdx);
    std::vector<int> rgs = unrank_rgs(sSel, rgsIdx);

    u128 packed = 0;
    for (int i = sSel - 1; i >= 0; --i)
        packed = packed * ALPHA + rgs[i];
    return emit_expr(sig, opIdx, packed);
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

int bit_length(u128 v) noexcept {
    if (!v)
        return 0;
    std::uint64_t hi = static_cast<std::uint64_t>(v >> 64);
    if (hi)
        return 64 + (64 - std::countl_zero(hi));
    std::uint64_t lo = static_cast<std::uint64_t>(v);
    return 64 - std::countl_zero(lo);
}

int main() {
    constexpr u128 PRINT = 100;
    std::unordered_set<std::string> seen;
    seen.reserve(PRINT);
    bool ok = true;
    auto t0 = std::chrono::high_resolution_clock::now();

    for (u128 i = 0; i < PRINT; ++i) {
        std::string e = nth_expression(i);
        std::cout << "#" << to_string(i + 1) << ": " << e << '\n';
        if (!seen.insert(e).second)
            std::cerr << "DUPLICATE @ " << to_string(i) << '\n';
        if (!neighbour_ok(e)) {
            ok = false;
            std::cerr << "GAP @ " << to_string(i) << '\n';
        }
    }

    u128 tot = prefixN[MAX_N];
    u128 last = tot - 1;

    int bits = bit_length(last);
    double pct = bits * 100.0 / 128.0;

    std::cout << "\nHighest rank uses " << bits << " /128 bits (" << std::fixed
              << std::setprecision(3) << pct << "%)\n";

    std::cout << "\n#" << to_string(tot) << ": " << nth_expression(tot - 1)
              << '\n';

    double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - t0)
                    .count();
    std::cout << std::fixed << std::setprecision(3) << "\nPrinted "
              << to_string(PRINT) << " expressions in " << ms << " ms\n";
    std::cout << (seen.size() == PRINT ? "No duplicates : )\n"
                                       : "Duplicates : (\n");
    std::cout << (ok ? "Neighbour rule OK : )\n"
                     : "Neighbour rule failed : (\n");
}
