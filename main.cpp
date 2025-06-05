// circuit_truth_table.cpp
// C++23 implementation to canonicalize a Boolean expression, compute its
// SHA-256 hash, and generate its truth table on the fly.

#include <cctype>
#include <format>
#include <iostream>
#include <memory>
#include <openssl/sha.h>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// Expression System

struct Expr {
    virtual ~Expr() = default;
    virtual bool eval(const std::vector<bool> &inputs) const = 0;
};

struct Var : Expr {
    int index;
    explicit Var(int idx) noexcept : index(idx) {}
    bool eval(const std::vector<bool> &inputs) const override {
        return inputs[index];
    }
};

template <typename Op> struct Binary : Expr {
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    Binary(std::unique_ptr<Expr> l, std::unique_ptr<Expr> r) noexcept
        : left(std::move(l)), right(std::move(r)) {}

    bool eval(const std::vector<bool> &inputs) const override {
        return Op{}(left->eval(inputs), right->eval(inputs));
    }
};

struct Not : Expr {
    std::unique_ptr<Expr> operand;
    explicit Not(std::unique_ptr<Expr> op) noexcept : operand(std::move(op)) {}
    bool eval(const std::vector<bool> &inputs) const override {
        return !operand->eval(inputs);
    }
};

struct AndOp {
    static constexpr std::string_view name = "AND";
    bool operator()(bool a, bool b) const noexcept { return a && b; }
};
struct OrOp {
    static constexpr std::string_view name = "OR";
    bool operator()(bool a, bool b) const noexcept { return a || b; }
};
struct XorOp {
    static constexpr std::string_view name = "XOR";
    bool operator()(bool a, bool b) const noexcept { return a != b; }
};

using And = Binary<AndOp>;
using Or = Binary<OrOp>;
using Xor = Binary<XorOp>;

// Canonicalization + Serialization

std::string serializeExpr(const Expr *expr);

std::unique_ptr<Expr> canonicalize(const Expr *expr) {
    if (auto const *v = dynamic_cast<const Var *>(expr)) {
        return std::make_unique<Var>(v->index);
    }
    if (auto const *n = dynamic_cast<const Not *>(expr)) {
        return std::make_unique<Not>(canonicalize(n->operand.get()));
    }

    auto canonicalPair = [&](auto const *node, auto makeNode) {
        auto l = canonicalize(node->left.get());
        auto r = canonicalize(node->right.get());
        if (serializeExpr(l.get()) > serializeExpr(r.get())) {
            std::swap(l, r);
        }
        return makeNode(std::move(l), std::move(r));
    };

    if (auto const *a = dynamic_cast<const And *>(expr)) {
        return canonicalPair(a, [](auto l, auto r) {
            return std::make_unique<And>(std::move(l), std::move(r));
        });
    }
    if (auto const *o = dynamic_cast<const Or *>(expr)) {
        return canonicalPair(o, [](auto l, auto r) {
            return std::make_unique<Or>(std::move(l), std::move(r));
        });
    }
    if (auto const *x = dynamic_cast<const Xor *>(expr)) {
        return canonicalPair(x, [](auto l, auto r) {
            return std::make_unique<Xor>(std::move(l), std::move(r));
        });
    }

    throw std::runtime_error("Unknown expression node in canonicalize");
}

std::string serializeExpr(const Expr *expr) {
    if (auto const *v = dynamic_cast<const Var *>(expr)) {
        return std::string(1, static_cast<char>('A' + v->index));
    }
    if (auto const *n = dynamic_cast<const Not *>(expr)) {
        return std::format("NOT({})", serializeExpr(n->operand.get()));
    }
    if (auto const *a = dynamic_cast<const And *>(expr)) {
        return std::format("AND({},{})", serializeExpr(a->left.get()),
                           serializeExpr(a->right.get()));
    }
    if (auto const *o = dynamic_cast<const Or *>(expr)) {
        return std::format("OR({},{})", serializeExpr(o->left.get()),
                           serializeExpr(o->right.get()));
    }
    if (auto const *x = dynamic_cast<const Xor *>(expr)) {
        return std::format("XOR({},{})", serializeExpr(x->left.get()),
                           serializeExpr(x->right.get()));
    }
    throw std::runtime_error("Unknown expression node in serializeExpr");
}

// Hashing

std::string hashExpr(std::string_view expr) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(expr.data()), expr.size(),
           digest);

    std::string hex;
    hex.reserve(2 * SHA256_DIGEST_LENGTH);
    for (auto byte : digest) {
        hex += std::format("{:02x}", byte);
    }
    return hex;
}

// Parser

class Parser {
  public:
    Parser(std::string_view str, int vars) noexcept
        : source(str), varCount(vars), pos(0) {}

    [[nodiscard]] std::unique_ptr<Expr> parse() {
        pos = 0;
        return parseExpr();
    }

  private:
    const std::string_view source;
    size_t pos;
    int varCount;

    static constexpr std::string_view opNames[] = {"AND",  "OR",  "XOR", "NOT",
                                                   "NAND", "NOR", "XNOR"};
    static inline const std::unordered_set<std::string_view> ops{
        opNames[0], opNames[1], opNames[2], opNames[3],
        opNames[4], opNames[5], opNames[6]};

    void skipWhitespace() noexcept {
        while (pos < source.size() &&
               std::isspace(static_cast<unsigned char>(source[pos]))) {
            ++pos;
        }
    }

    [[nodiscard]] std::string_view parseIdentifier() {
        skipWhitespace();
        size_t start = pos;
        while (pos < source.size() &&
               std::isalpha(static_cast<unsigned char>(source[pos]))) {
            ++pos;
        }
        return source.substr(start, pos - start);
    }

    [[nodiscard]] std::unique_ptr<Expr> parseExpr() {
        skipWhitespace();
        if (pos >= source.size()) {
            throw std::runtime_error("Unexpected end of input");
        }

        size_t startPos = pos;
        auto id = parseIdentifier();
        skipWhitespace();

        if (!id.empty() && ops.contains(id) && pos < source.size() &&
            source[pos] == '(') {
            ++pos;
            auto leftNode = parseExpr();
            skipWhitespace();

            std::unique_ptr<Expr> rightNode = nullptr;
            if (id != "NOT") {
                if (pos >= source.size() || source[pos] != ',') {
                    throw std::runtime_error(
                        "Expected ',' after first operand");
                }
                ++pos;
                rightNode = parseExpr();
            }

            skipWhitespace();
            if (pos >= source.size() || source[pos] != ')') {
                throw std::runtime_error("Expected ')' to close operator");
            }
            ++pos;

            if (id == "AND") {
                return std::make_unique<And>(std::move(leftNode),
                                             std::move(rightNode));
            }
            if (id == "OR") {
                return std::make_unique<Or>(std::move(leftNode),
                                            std::move(rightNode));
            }
            if (id == "XOR") {
                return std::make_unique<Xor>(std::move(leftNode),
                                             std::move(rightNode));
            }
            if (id == "NAND") {
                return std::make_unique<Not>(std::make_unique<And>(
                    std::move(leftNode), std::move(rightNode)));
            }
            if (id == "NOR") {
                return std::make_unique<Not>(std::make_unique<Or>(
                    std::move(leftNode), std::move(rightNode)));
            }
            if (id == "XNOR") {
                return std::make_unique<Not>(std::make_unique<Xor>(
                    std::move(leftNode), std::move(rightNode)));
            }
            if (id == "NOT") {
                return std::make_unique<Not>(std::move(leftNode));
            }

            throw std::runtime_error(
                std::format("Unknown operator: {}", std::string(id)));
        }

        pos = startPos;
        skipWhitespace();
        if (pos >= source.size() ||
            !std::isupper(static_cast<unsigned char>(source[pos]))) {
            throw std::runtime_error("Expected variable (A-Z)");
        }
        char varChar = source[pos++];
        int idx = varChar - 'A';
        if (idx < 0 || idx >= varCount) {
            throw std::runtime_error(
                std::format("Variable '{}' out of range", varChar));
        }
        return std::make_unique<Var>(idx);
    }
};

// Main

int main() {
    constexpr std::string_view exprStr = "OR(AND(NOT(A),B), XNOR(AND(C,D),E))";
    constexpr int varCount = 5;

    auto parsedExpr = Parser(exprStr, varCount).parse();
    auto canonicalExpr = canonicalize(parsedExpr.get());
    std::string serialized = serializeExpr(canonicalExpr.get());
    std::string hash = hashExpr(serialized);

    std::cout << std::format("Expression:   {}\n", exprStr);
    std::cout << std::format("Canonical:    {}\n", serialized);
    std::cout << std::format("SHA256 Hash:  {}\n\n", hash);

    for (int i = 0; i < varCount; ++i) {
        std::cout << static_cast<char>('A' + i) << ' ';
    }
    std::cout << "| Out\n";
    std::cout << std::string(varCount * 2 + 3, '-') << "\n";

    const int totalRows = 1 << varCount;
    std::vector<bool> inputs(varCount);

    for (int row : std::ranges::views::iota(0, totalRows)) {
        for (int bit = 0; bit < varCount; ++bit) {
            inputs[bit] = static_cast<bool>((row >> (varCount - 1 - bit)) & 1);
        }
        for (bool b : inputs) {
            std::cout << b << ' ';
        }
        bool result = canonicalExpr->eval(inputs);
        std::cout << "|  " << result << '\n';
    }

    return 0;
}
