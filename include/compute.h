#pragma once

#include "compute_data.h"
#include <string>
#include <vector>

inline constexpr size_t OUT_BUF_SIZE = 1 << 16;
extern char OUT[OUT_BUF_SIZE];

void build_expr(int s, u128 idx, const std::vector<int> &ops,
                const std::vector<int> &rgs, int &leafIdx, int &opIdx, int &OL);

u128 shape_unrank(int s, u128 woff, int &b_shape, u128 &variantOff);

std::string unrank(u128 N);
