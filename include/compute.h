#pragma once

#include "compute_data.h"
#include <memory>
#include <string>
#include <vector>

inline constexpr size_t OUT_BUF_SIZE = 1 << 16;
extern char OUT[OUT_BUF_SIZE];
enum class NodeType {
    Leaf,  // A single variable
    Unary, // NOT(subtree)
    Binary // AND/OR/XOR(left,right)
};

//-----------------------------------------------------------------------------
// PlanNode: a node in the *shape* tree (no ops or labels yet)
//   • type     = Leaf, Unary or Binary
//   • children = 0, 1 or 2 subtrees depending on type
//-----------------------------------------------------------------------------
struct PlanNode {
    NodeType type;
    int ls = -1, rs = -1;
    std::vector<std::unique_ptr<PlanNode>> children;
};
void build_expr(int s, u128 idx, const std::vector<int> &ops,
                const std::vector<int> &rgs, int &leafIdx, int &opIdx, int &OL);

u128 shape_unrank(int s, u128 woff, int &b_shape, u128 &variantOff);

std::string unrank(u128 N);
