#ifndef COMPUTE_H
#define COMPUTE_H

#include <string>
#include <string_view>
#include <vector>

using u128 = unsigned __int128;

inline constexpr int ALPHA = 26;

std::string to_string(u128 x);

int bit_length(u128 x) noexcept;

std::vector<int> unrank_rgs(int len, u128 k);

std::string unrank_shape(int s, int u, u128 k);

std::string emit_expr(const std::string &sig, u128 opIdx, u128 lblIdx);

std::string nth_expression(u128 n);

bool neighbour_ok(std::string_view expr);

#endif // COMPUTE_H
