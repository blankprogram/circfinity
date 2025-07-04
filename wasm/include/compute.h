#ifndef COMPUTE_H
#define COMPUTE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <string>
#include <string_view>
#include <vector>
using bigint = boost::multiprecision::cpp_int;

std::string to_string(bigint x);

std::vector<int> unrank_rgs(int len, bigint k);

int bit_length(const bigint &v) noexcept;

std::string unrank_shape(int s, int u, bigint k);

std::string emit_expr(const std::string &signature, bigint opIndex,
                      const std::vector<int> &labels);

std::string nth_expression(bigint n);

bool neighbour_ok(std::string_view expr);

#endif // COMPUTE_H
