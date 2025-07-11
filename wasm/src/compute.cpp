#include "compute_data.h"
#include <array>
#include <cassert>
#include <compute.h>
#include <memory>
#include <string>
#include <vector>

/* Converts bigint to decimal string (stack buffer, no heap) */
std::string to_string(bigint x) {
    char buf[500];
    char *p = std::end(buf);
    do {
        *--p = char('0' + x % 10);
        x /= 10;
    } while (x);
    return {p, std::end(buf)};
}

// Minimal JSON parser for flat { "A": true, "B": false }
std::unordered_map<std::string, bool> parse_input_map(const std::string &json) {
    std::unordered_map<std::string, bool> result;
    size_t i = 0;
    auto skip_ws = [&]() {
        while (i < json.size() && std::isspace(json[i]))
            ++i;
    };

    skip_ws();
    if (json[i] != '{')
        throw std::runtime_error("Expected '{'");
    ++i;

    while (true) {
        skip_ws();
        if (json[i] == '}')
            break;

        if (json[i] != '"')
            throw std::runtime_error("Expected key string");
        ++i;
        std::string key;
        while (i < json.size() && json[i] != '"') {
            key += json[i++];
        }
        if (i == json.size() || json[i] != '"')
            throw std::runtime_error("Unterminated key");
        ++i;

        skip_ws();
        if (json[i++] != ':')
            throw std::runtime_error("Expected ':' after key");
        skip_ws();

        if (json.compare(i, 4, "true") == 0) {
            result[key] = true;
            i += 4;
        } else if (json.compare(i, 5, "false") == 0) {
            result[key] = false;
            i += 5;
        } else {
            throw std::runtime_error("Expected true/false value");
        }

        skip_ws();
        if (json[i] == ',') {
            ++i;
        } else if (json[i] == '}') {
            break;
        } else {
            throw std::runtime_error("Expected ',' or '}'");
        }
    }

    return result;
}
// Evaluates expression and computes it
std::string evaluate_expr_full_json(bigint N, const std::string &jsonInputs) {
    auto inputs = parse_input_map(jsonInputs);

    std::string sig;
    bigint opIdx;
    std::vector<int> labels;
    compute_expr_components(N, sig, opIdx, labels);
    auto [_, tree] = emit_expr_both(sig, opIdx, labels);

    std::unordered_map<const ExprTree *, std::string> id_map;
    std::unordered_map<std::string, bool> result_map;
    int id_counter = 0;

    std::function<bool(const ExprTree *)> dfs =
        [&](const ExprTree *node) -> bool {
        if (!node)
            throw std::runtime_error("Null node in evaluation");

        std::string node_id = "n" + std::to_string(id_counter++);
        id_map[node] = node_id;

        bool val;
        if (node->type == "VAR") {
            auto it = inputs.find(node->value);
            if (it == inputs.end())
                throw std::runtime_error("Missing input for variable: " +
                                         node->value);
            val = it->second;
        } else if (node->type == "NOT") {
            val = !dfs(node->left.get());
        } else {
            bool lhs = dfs(node->left.get());
            bool rhs = dfs(node->right.get());

            if (node->type == "AND")
                val = lhs && rhs;
            else if (node->type == "OR")
                val = lhs || rhs;
            else if (node->type == "XOR")
                val = lhs ^ rhs;
            else
                throw std::runtime_error("Unknown node type: " + node->type);
        }

        result_map[node_id] = val;
        return val;
    };

    dfs(tree.get());

    std::string json = "{";
    bool first = true;
    for (const auto &[ptr, name] : id_map) {
        if (!first)
            json += ",";
        first = false;
        json += "\"" + name + "\":" + (result_map[name] ? "true" : "false");
    }
    json += "}";
    return json;
}

/* serialises a logic expression tree to minimal JSON */
std::string serialise_tree(const ExprTree *node) {
    if (!node)
        return "null";
    if (node->type == "VAR") {
        return "\"" + node->value + "\"";
    } else if (node->type == "NOT") {
        return "{\"type\":\"NOT\",\"child\":" +
               serialise_tree(node->left.get()) + "}";
    } else {
        return "{\"type\":\"" + node->type +
               "\",\"left\":" + serialise_tree(node->left.get()) +
               ",\"right\":" + serialise_tree(node->right.get()) + "}";
    }
}

/* Unranks a restricted growth string (used for variable partitioning) */
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

/* Unranks a shape (preorder string of L/U/B) given leaf/unary count */
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

/* Builds both expression string and tree from signature + operator/label
 * indices */
std::pair<std::string, std::unique_ptr<ExprTree>>
emit_expr_both(const std::string &sig, bigint opIdx,
               const std::vector<int> &lbl) {
    static constexpr const char *OPSTR[3] = {"AND", "OR", "XOR"};
    size_t sigPos = 0, lblPos = 0;
    std::string out;
    out.reserve(sig.size() * 4);

    std::function<std::unique_ptr<ExprTree>()> dfs =
        [&]() -> std::unique_ptr<ExprTree> {
        char t = sig[sigPos++];
        if (t == 'L') {
            std::string v = Labels[lbl[lblPos++]];
            out += v;
            return std::make_unique<ExprTree>(ExprTree{"VAR", v});
        } else if (t == 'U') {
            out += "NOT(";
            auto child = dfs();
            out += ')';
            return std::make_unique<ExprTree>(
                ExprTree{"NOT", "", std::move(child)});
        } else {
            int o = int(opIdx % 3);
            opIdx /= 3;
            std::string op = OPSTR[o];
            out += op + "(";
            auto l = dfs();
            out += ',';
            auto r = dfs();
            out += ')';
            return std::make_unique<ExprTree>(
                ExprTree{op, "", std::move(l), std::move(r)});
        }
    };

    auto tree = dfs();
    return {out, std::move(tree)};
}

/* builds the expression string */
std::string emit_expr(const std::string &sig, bigint opIdx,
                      const std::vector<int> &lbl) {
    static constexpr const char *OPSTR[3] = {"AND", "OR", "XOR"};
    size_t sigPos = 0, lblPos = 0;
    std::string out;
    out.reserve(sig.size() * 4);

    std::function<void()> dfs = [&]() {
        char t = sig[sigPos++];
        if (t == 'L') {
            out += Labels[lbl[lblPos++]];
        } else if (t == 'U') {
            out += "NOT(";
            dfs();
            out += ")";
        } else {
            int o = int(opIdx % 3);
            opIdx /= 3;
            out += OPSTR[o];
            out += "(";
            dfs();
            out += ",";
            dfs();
            out += ")";
        }
    };

    dfs();
    return out;
}

/* Computes shape, opIdx, and labels for nth expression */
void compute_expr_components(bigint N, std::string &sig, bigint &opIdx,
                             std::vector<int> &labels) {
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
    opIdx = tmp / Bell[sSel];
    bigint rgsIdx = tmp % Bell[sSel];

    sig = unrank_shape(sSel, uSel, shapeIdx);
    labels = unrank_rgs(sSel, rgsIdx);
}

/* Returns expression string for index N */
std::string get_expr(bigint N) {
    std::string sig;
    bigint opIdx;
    std::vector<int> labels;
    compute_expr_components(N, sig, opIdx, labels);

    return emit_expr(sig, opIdx, labels);
}

/* Returns expression string + serialised tree JSON for index N */
std::string get_expr_full(bigint N) {
    std::string sig;
    bigint opIdx;
    std::vector<int> labels;
    compute_expr_components(N, sig, opIdx, labels);
    auto [exprStr, tree] = emit_expr_both(sig, opIdx, labels);
    return "{\"expr\":\"" + exprStr +
           "\",\"tree\":" + serialise_tree(tree.get()) + "}";
}
