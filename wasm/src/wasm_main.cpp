#include "compute.h"
#include "compute_data.h"
#include <emscripten/bind.h>
#include <string>

std::string get_expr_full_wrapper(std::string n_str) {
    bigint n(n_str);
    return get_expr_full(n);
}

std::string get_expr_count_wrapper() { return to_string(prefixN[MAX_N]); }

std::string evaluate_expr_full_json_wrapper(std::string n_str,
                                            const std::string &jsonInputs) {
    bigint N(n_str);
    return evaluate_expr_full_json(N, jsonInputs);
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("get_expr_full", &get_expr_full_wrapper);
    emscripten::function("get_expr_count", &get_expr_count_wrapper);
    emscripten::function("evaluate_expr_full_json",
                         &evaluate_expr_full_json_wrapper);
}
