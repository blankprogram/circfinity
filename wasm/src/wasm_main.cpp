#include "compute.h"
#include "compute_data.h"
#include <emscripten/bind.h>
#include <string>

std::string get_expr(const std::string &n_str) {
    bigint n(n_str);
    return nth_expression(n);
}

std::string get_expr_count() { return to_string(prefixN[MAX_N]); }

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("get_expr", &get_expr);
    emscripten::function("get_expr_count", &get_expr_count);
}
