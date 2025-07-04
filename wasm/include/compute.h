#ifndef COMPUTE_H
#define COMPUTE_H

#include <boost/multiprecision/cpp_int.hpp>
#include <memory>
#include <string>
#include <vector>

using bigint = boost::multiprecision::cpp_int;

struct ExprTree {
    std::string type;
    std::string value;
    std::unique_ptr<ExprTree> left;
    std::unique_ptr<ExprTree> right;
};

std::string to_string(bigint x);
std::vector<int> unrank_rgs(int len, bigint k);
std::string unrank_shape(int s, int u, bigint k);
std::string emit_expr(const std::string &sig, bigint opIdx,
                      const std::vector<int> &lbl);

void compute_expr_components(bigint n, std::string &sig, bigint &opIdx,
                             std::vector<int> &labels);
std::pair<std::string, std::unique_ptr<ExprTree>>
emit_expr_both(const std::string &sig, bigint opIdx,
               const std::vector<int> &labels);
std::string serialise_tree(const ExprTree *node);

std::string get_expr(bigint n);
std::string get_expr_full(bigint n);
#endif // COMPUTE_H
