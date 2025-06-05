#include <array>
#include <cctype>
#include <format>
#include <iostream>
#include <memory>
#include <openssl/sha.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using uint128_t = __uint128_t;

static char bitChar(uint8_t b) { return b ? '1' : '0'; }

static std::string varName(int idx) {
    if (idx < 26) {
        return std::string(1, static_cast<char>('A' + idx));
    } else {
        int hi = (idx / 26) - 1;
        int lo = idx % 26;
        return std::string{static_cast<char>('A' + hi),
                           static_cast<char>('A' + lo)};
    }
}

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

std::string toString(uint128_t value) {
    if (value == 0)
        return "0";
    std::string tmp;
    tmp.reserve(40);
    while (value != 0) {
        int digit = static_cast<int>(value % 10);
        tmp.push_back(char('0' + digit));
        value /= 10;
    }
    std::reverse(tmp.begin(), tmp.end());
    return tmp;
}

struct Expr {
    virtual ~Expr() = default;
    virtual bool eval(const std::vector<uint8_t> &inputs) const = 0;
    virtual void collectVars(std::unordered_set<int> &out) const = 0;
    mutable bool isConstant = false;
    mutable bool constValue = false;
};

struct Var : Expr {
    int index;
    explicit Var(int idx) noexcept : index(idx) {}
    bool eval(const std::vector<uint8_t> &inputs) const override {
        return inputs[index] != 0;
    }
    void collectVars(std::unordered_set<int> &out) const override {
        out.insert(index);
    }
};

template <typename Op> struct Binary : Expr {
    std::unique_ptr<Expr> left, right;
    Binary(std::unique_ptr<Expr> l, std::unique_ptr<Expr> r) noexcept
        : left(std::move(l)), right(std::move(r)) {}
    bool eval(const std::vector<uint8_t> &inputs) const override {
        if (isConstant)
            return constValue;
        bool L = left->eval(inputs);
        bool R = right->eval(inputs);
        return Op{}(L, R);
    }
    void collectVars(std::unordered_set<int> &out) const override {
        left->collectVars(out);
        right->collectVars(out);
    }
};

struct Not : Expr {
    std::unique_ptr<Expr> operand;
    explicit Not(std::unique_ptr<Expr> op) noexcept : operand(std::move(op)) {}
    bool eval(const std::vector<uint8_t> &inputs) const override {
        if (isConstant)
            return constValue;
        return !operand->eval(inputs);
    }
    void collectVars(std::unordered_set<int> &out) const override {
        operand->collectVars(out);
    }
};

struct AndOp {
    bool operator()(bool a, bool b) const noexcept { return a && b; }
};
struct OrOp {
    bool operator()(bool a, bool b) const noexcept { return a || b; }
};
struct XorOp {
    bool operator()(bool a, bool b) const noexcept { return a != b; }
};

using And = Binary<AndOp>;
using Or = Binary<OrOp>;
using Xor = Binary<XorOp>;

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
        if (pos >= source.size())
            throw std::runtime_error("Unexpected end of input");

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
                if (pos >= source.size() || source[pos] != ',')
                    throw std::runtime_error(
                        "Expected ',' after first operand");
                ++pos;
                rightNode = parseExpr();
            }

            skipWhitespace();
            if (pos >= source.size() || source[pos] != ')')
                throw std::runtime_error("Expected ')' to close operator");
            ++pos;

            if (id == "AND")
                return std::make_unique<And>(std::move(leftNode),
                                             std::move(rightNode));
            if (id == "OR")
                return std::make_unique<Or>(std::move(leftNode),
                                            std::move(rightNode));
            if (id == "XOR")
                return std::make_unique<Xor>(std::move(leftNode),
                                             std::move(rightNode));
            if (id == "NAND")
                return std::make_unique<Not>(std::make_unique<And>(
                    std::move(leftNode), std::move(rightNode)));
            if (id == "NOR")
                return std::make_unique<Not>(std::make_unique<Or>(
                    std::move(leftNode), std::move(rightNode)));
            if (id == "XNOR")
                return std::make_unique<Not>(std::make_unique<Xor>(
                    std::move(leftNode), std::move(rightNode)));
            if (id == "NOT")
                return std::make_unique<Not>(std::move(leftNode));
            throw std::runtime_error(
                std::format("Unknown operator '{}' at position {}", id, pos));
        }

        pos = startPos;
        skipWhitespace();
        if (pos >= source.size() ||
            !std::isupper(static_cast<unsigned char>(source[pos])))
            throw std::runtime_error("Expected variable (A-Z)");

        char varChar = source[pos++];
        int idx = varChar - 'A';
        if (idx < 0 || idx >= varCount)
            throw std::runtime_error(
                std::format("Variable '{}' out of range", varChar));

        return std::make_unique<Var>(idx);
    }
};

static uint128_t countTable[31][17];
static bool countInit[31][17];

uint128_t countExprs(int depth, int maxVar) {
    if (maxVar < 1 || maxVar > 16)
        throw std::out_of_range("maxVar must be 1..16");
    if (depth < 0 || depth > 30)
        return 0;

    if (countInit[depth][maxVar]) {
        return countTable[depth][maxVar];
    }

    uint128_t total = 0;
    if (depth == 0) {
        total = static_cast<uint128_t>(maxVar);
    } else {
        for (int d = 0; d < depth; ++d) {
            total += countExprs(d, maxVar);
        }
        for (int d1 = 0; d1 < depth; ++d1) {
            int d2 = depth - 1 - d1;
            uint128_t leftCount = countExprs(d1, maxVar);
            uint128_t rightCount = countExprs(d2, maxVar);
            total += leftCount * rightCount * 3;
        }
    }

    countInit[depth][maxVar] = true;
    countTable[depth][maxVar] = total;
    return total;
}

std::string generateExpr(uint128_t &n, int depth, int maxVar,
                         std::string &buf) {
    if (depth == 0) {
        if (n < static_cast<uint128_t>(maxVar)) {
            buf = varName(static_cast<int>(n));
            return buf;
        }
        throw std::out_of_range("No more variables at depth=0");
    }
    for (int d = 0; d < depth; ++d) {
        uint128_t c = countExprs(d, maxVar);
        if (n < c) {
            generateExpr(n, d, maxVar, buf);
            buf.insert(0, "NOT(");
            buf.push_back(')');
            return buf;
        }
        n -= c;
    }
    static constexpr std::array<std::string_view, 3> ops = {"AND", "OR", "XOR"};
    for (int opIdx = 0; opIdx < 3; ++opIdx) {
        auto opName = ops[opIdx];
        for (int d1 = 0; d1 < depth; ++d1) {
            int d2 = depth - 1 - d1;
            uint128_t leftCount = countExprs(d1, maxVar);
            uint128_t rightCount = countExprs(d2, maxVar);
            uint128_t pairCount = leftCount * rightCount;
            if (n < pairCount) {
                uint128_t li = n / rightCount;
                uint128_t ri = n % rightCount;
                generateExpr(li, d1, maxVar, buf);
                std::string leftSide = buf;
                generateExpr(ri, d2, maxVar, buf);
                std::string rightSide = buf;
                buf.clear();
                buf.reserve(opName.size() + 2 + leftSide.size() + 1 +
                            rightSide.size() + 1);
                buf += opName;
                buf.push_back('(');
                buf += leftSide;
                buf.push_back(',');
                buf += rightSide;
                buf.push_back(')');
                return buf;
            }
            n -= pairCount;
        }
    }
    throw std::out_of_range("Expression index too large");
}

std::string generateNthExpr(uint128_t n, int maxVar) {
    int depth = 0;
    std::string buf;
    while (true) {
        uint128_t c = countExprs(depth, maxVar);
        if (c == 0) {
            throw std::out_of_range("Index out of range at depth = " +
                                    std::to_string(depth));
        }
        if (n < c) {
            return generateExpr(n, depth, maxVar, buf);
        }
        n -= c;
        ++depth;
        if (depth > 30) {
            throw std::out_of_range("Exceeded reasonable depth");
        }
    }
}

int main() {
    constexpr int varCount = 5;
    uint128_t exprIndex = (uint128_t)-1;

    std::string exprStr = generateNthExpr(exprIndex, varCount);
    std::string hash = hashExpr(exprStr);

    std::cout << "Expression:   " << exprStr << "\nSHA256 Hash:  " << hash
              << "\n\n";

    auto rootExpr = Parser(exprStr, varCount).parse();

    std::unordered_set<int> usedSet;
    usedSet.reserve(varCount);
    rootExpr->collectVars(usedSet);

    std::vector<int> usedIndices(usedSet.begin(), usedSet.end());
    std::sort(usedIndices.begin(), usedIndices.end());
    int k = static_cast<int>(usedIndices.size());

    for (int idx : usedIndices) {
        std::cout << varName(idx) << ' ';
    }
    std::cout << "| Out\n";
    std::cout << std::string(k * 2 + 3, '-') << "\n";

    uint64_t totalRows = (uint64_t)1 << k;
    std::vector<uint8_t> fullInputs(varCount, 0);
    std::vector<uint8_t> subInputs(k, 0);

    for (uint64_t mask = 0; mask < totalRows; ++mask) {
        for (int bit = 0; bit < k; ++bit) {
            subInputs[bit] =
                static_cast<uint8_t>((mask >> (k - 1 - bit)) & 1ULL);
        }
        std::fill(fullInputs.begin(), fullInputs.end(), 0);
        for (int i = 0; i < k; ++i) {
            fullInputs[usedIndices[i]] = subInputs[i];
        }
        for (int i = 0; i < k; ++i) {
            std::cout << bitChar(subInputs[i]) << ' ';
        }
        bool out = rootExpr->eval(fullInputs);
        std::cout << "|  " << bitChar(out) << "\n";
    }
    return 0;
}
