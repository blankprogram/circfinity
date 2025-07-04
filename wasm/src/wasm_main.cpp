#include "compute.h"
#include "compute_data.h"
#include <emscripten/bind.h>
#include <string>

std::string get_expr_wrapper(std::string n_str) {
    bigint n(n_str);
    return get_expr(n);
}

std::string get_expr_full_wrapper(std::string n_str) {
    bigint n(n_str);
    return get_expr_full(n);
}

std::string get_expr_count_wrapper() { return to_string(prefixN[MAX_N]); }

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("get_expr", &get_expr_wrapper);
    emscripten::function("get_expr_full", &get_expr_full_wrapper);
    emscripten::function("get_expr_count", &get_expr_count_wrapper);
}
