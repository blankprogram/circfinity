#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

using namespace std;

struct ExprTree {
    int type;
    shared_ptr<ExprTree> left;
    shared_ptr<ExprTree> right;
    int size;
    int num_binary;

    ExprTree() : type(0), size(1), num_binary(0) {}
    ExprTree(shared_ptr<ExprTree> child)
        : type(1), left(std::move(child)), size(1 + left->size),
          num_binary(left->num_binary) {}
    ExprTree(shared_ptr<ExprTree> l, shared_ptr<ExprTree> r)
        : type(2), left(std::move(l)), right(std::move(r)),
          size(1 + left->size + right->size),
          num_binary(1 + left->num_binary + right->num_binary) {}
};

class ExpressionGenerator {
    static constexpr int MAX_SIZE = 15;
    int max_leaves;
    vector<vector<shared_ptr<ExprTree>>> trees_by_size;
    vector<long long> bell;
    vector<vector<long long>> tree_counts;
    vector<long long> cumulative_size_total;
    vector<vector<vector<int>>> all_rgs;
    vector<long long> pow3;

    void precompute_bell() {
        bell = {1, 1, 2, 5, 15, 52, 203, 877, 4140, 21147, 115975};
    }

    void precompute_pow3() {
        pow3.resize(MAX_SIZE);
        pow3[0] = 1;
        for (int i = 1; i < MAX_SIZE; ++i) {
            pow3[i] = pow3[i - 1] * 3;
        }
    }

    void precompute_trees() {
        vector<long long> count_trees(MAX_SIZE, 0);
        count_trees[0] = 1;
        for (int s = 2; s <= MAX_SIZE; ++s) {
            long long cnt = 0;
            cnt += count_trees[s - 2];
            for (int left_size = 1; left_size <= s - 2; ++left_size) {
                int right_size = s - 1 - left_size;
                cnt += count_trees[left_size - 1] * count_trees[right_size - 1];
            }
            count_trees[s - 1] = cnt;
        }

        trees_by_size.resize(MAX_SIZE);
        trees_by_size[0].reserve(count_trees[0]);
        trees_by_size[0].push_back(make_shared<ExprTree>());

        for (int s = 2; s <= MAX_SIZE; ++s) {
            trees_by_size[s - 1].reserve(count_trees[s - 1]);
            for (int left_size = 1; left_size <= s - 2; ++left_size) {
                int right_size = s - 1 - left_size;
                const auto &left_trees = trees_by_size[left_size - 1];
                const auto &right_trees = trees_by_size[right_size - 1];
                for (const auto &left_tree : left_trees) {
                    for (const auto &right_tree : right_trees) {
                        trees_by_size[s - 1].push_back(
                            make_shared<ExprTree>(left_tree, right_tree));
                    }
                }
            }
            for (const auto &child : trees_by_size[s - 2]) {
                trees_by_size[s - 1].push_back(make_shared<ExprTree>(child));
            }
        }
    }

    void precompute_all_rgs() {
        all_rgs.resize(max_leaves + 1);
        for (int k = 1; k <= max_leaves; ++k) {
            if (k == 1) {
                all_rgs[k] = {{0}};
                continue;
            }
            vector<vector<int>> prev = {{0}};
            for (int len = 1; len < k; ++len) {
                vector<vector<int>> next;
                next.reserve(bell[len + 1]);
                for (const auto &seq : prev) {
                    int max_val = *max_element(seq.begin(), seq.end());
                    for (int j = 0; j <= max_val + 1; ++j) {
                        vector<int> new_seq = seq;
                        new_seq.push_back(j);
                        next.push_back(std::move(new_seq));
                    }
                }
                prev = std::move(next);
            }
            all_rgs[k] = std::move(prev);
        }
    }

    void precompute_counts() {
        tree_counts.resize(MAX_SIZE);
        cumulative_size_total.resize(MAX_SIZE, 0);

        for (int s = 1; s <= MAX_SIZE; ++s) {
            long long total = 0;
            tree_counts[s - 1].resize(trees_by_size[s - 1].size());
            for (size_t i = 0; i < trees_by_size[s - 1].size(); ++i) {
                const int b = trees_by_size[s - 1][i]->num_binary;
                const int k = 1 + b;
                const long long count = pow3[b] * bell[k];
                tree_counts[s - 1][i] = count;
                total += count;
            }
            cumulative_size_total[s - 1] = total;
            if (s > 1) {
                cumulative_size_total[s - 1] += cumulative_size_total[s - 2];
            }
        }
    }

    [[nodiscard]] vector<int> convert_base(long long num, int base,
                                           int digits) const {
        vector<int> res(digits, 0);
        for (int i = digits - 1; i >= 0 && num > 0; --i) {
            res[i] = num % base;
            num /= base;
        }
        return res;
    }

    [[nodiscard]] string build_expression(const ExprTree *T,
                                          vector<int> &op_digits, int &op_idx,
                                          vector<string> &var_labels,
                                          int &leaf_idx) const {
        if (T->type == 0) {
            return var_labels[leaf_idx++];
        }
        if (T->type == 1) {
            string child = build_expression(T->left.get(), op_digits, op_idx,
                                            var_labels, leaf_idx);
            return "NOT(" + child + ")";
        }
        string op_str;
        switch (op_digits[op_idx++]) {
        case 0:
            op_str = "AND";
            break;
        case 1:
            op_str = "OR";
            break;
        case 2:
            op_str = "XOR";
            break;
        }
        string left = build_expression(T->left.get(), op_digits, op_idx,
                                       var_labels, leaf_idx);
        string right = build_expression(T->right.get(), op_digits, op_idx,
                                        var_labels, leaf_idx);
        return op_str + "(" + left + "," + right + ")";
    }

  public:
    [[nodiscard]] long long get_max_valid_index() const {
        return cumulative_size_total[MAX_SIZE - 1];
    }

    ExpressionGenerator() : max_leaves(8) {
        precompute_bell();
        precompute_pow3();
        precompute_trees();
        precompute_all_rgs();
        precompute_counts();
    }

    [[nodiscard]] string get_expression(long long n) const {
        if (n < 1 || n > cumulative_size_total[MAX_SIZE - 1]) {
            return "ERROR: n out of range";
        }

        int s = 1;
        while (s <= MAX_SIZE && cumulative_size_total[s - 1] < n) {
            ++s;
        }

        const long long base = (s > 1) ? cumulative_size_total[s - 2] : 0;
        const long long idx_in_size = n - base;

        const auto &trees = trees_by_size[s - 1];
        const auto &counts = tree_counts[s - 1];
        long long cumulative = 0;
        size_t tree_idx = 0;

        for (; tree_idx < trees.size(); ++tree_idx) {
            if (idx_in_size <= cumulative + counts[tree_idx]) {
                break;
            }
            cumulative += counts[tree_idx];
        }

        const auto &T = trees[tree_idx];
        const long long residual = idx_in_size - cumulative - 1;
        const int b = T->num_binary;
        const int k = 1 + b;

        const long long op_index = residual / bell[k];
        const long long var_index = residual % bell[k];
        vector<int> op_digits =
            (b > 0) ? convert_base(op_index, 3, b) : vector<int>();

        const vector<int> &rgs = all_rgs[k][var_index];
        vector<string> var_labels(k);
        map<int, string> block_map;
        char next_var = 'A';
        for (int i = 0; i < k; ++i) {
            if (!block_map.contains(rgs[i])) {
                block_map[rgs[i]] = string(1, next_var++);
            }
            var_labels[i] = block_map[rgs[i]];
        }

        int op_idx = 0;
        int leaf_idx = 0;
        return build_expression(T.get(), op_digits, op_idx, var_labels,
                                leaf_idx);
    }
};

int main() {
    const ExpressionGenerator gen;
    constexpr int n = 100;
    constexpr long long large = 1000000000;
    for (const auto i : views::iota(1, n + 1)) {
        cout << "Expression #" << i << ": " << gen.get_expression(i) << '\n';
    }

    cout << "Expression #" << large << ": " << gen.get_expression(large)
         << '\n';

    cout << "Max n allowed: " << gen.get_max_valid_index() << '\n';
    return 0;
}
