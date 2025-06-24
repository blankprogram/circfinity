// unrank.cpp
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

template <std::size_t N> consteval std::array<uint64_t, N + 2> make_bell() {
    std::array<uint64_t, N + 2> B{}, prev{}, row{};
    B[0] = prev[0] = 1;
    for (std::size_t n = 1; n <= N + 1; ++n) {
        row[0] = prev[n - 1];
        for (std::size_t k = 1; k <= n; ++k)
            row[k] = row[k - 1] + prev[k - 1];
        B[n] = row[0];
        for (std::size_t k = 0; k <= n; ++k)
            prev[k] = row[k];
    }
    return B;
}

static constexpr int MAX_SIZE = 23;
static constexpr auto Bell = make_bell<MAX_SIZE>();

using idx_t = uint16_t;

// on-disk node format: exactly 8 bytes
struct Node {
    uint8_t type;      // 0=leaf,1=unary,2=binary
    uint8_t numBinary; // number of binary ops in subtree
    uint8_t childSize; // size of the single child (if unary) or left subtree
    uint8_t rightSize; // size of right subtree (0 if unary)
    union {
        idx_t child; // for leaf/unary
        struct {     // for binary
            idx_t left, right;
        } br;
    } u;
};
static_assert(sizeof(Node) == 8, "Node on-disk format must be 8 bytes");

class ExpressionGenerator {
    int _fd{};
    size_t _fileSize{};
    const char *_data{};

    std::array<const Node *, MAX_SIZE + 1> _nodesPool;
    std::array<const uint32_t *, MAX_SIZE + 1> _roots;
    std::array<const long long *, MAX_SIZE + 1> _prefix;
    std::array<size_t, MAX_SIZE + 1> _shapeCount;
    std::array<long long, MAX_SIZE + 1> _cumTotal;

    mutable std::array<std::vector<std::vector<int>>, MAX_SIZE + 2> _rgsCache;
    mutable std::array<int, MAX_SIZE> _tmpOp{};
    mutable std::array<int, 64> _stamp{};
    mutable std::array<char, 64> _chMap{};
    mutable int _curStamp{1};
    mutable std::array<char, 4096> _outBuf;
    mutable size_t _outLen{0};

    void generateRgs(int i, int m, int k, std::vector<int> &cur,
                     std::vector<std::vector<int>> &out) const {
        if (i == k) {
            out.push_back(cur);
            return;
        }
        for (int j = 0; j <= m + 1; ++j) {
            cur.push_back(j);
            generateRgs(i + 1, std::max(m, j), k, cur, out);
            cur.pop_back();
        }
    }

  public:
    ExpressionGenerator(const char *path = "precomputed_data.bin") {
        _fd = open(path, O_RDONLY);
        if (_fd < 0) {
            perror("open");
            exit(1);
        }
        struct stat st;
        fstat(_fd, &st);
        _fileSize = st.st_size;
        _data = static_cast<const char *>(
            mmap(nullptr, _fileSize, PROT_READ, MAP_PRIVATE, _fd, 0));
        if (_data == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }

        const char *ptr = _data;
        for (int s = 1; s <= MAX_SIZE; ++s) {
            // read node count
            uint32_t nc = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += sizeof(uint32_t);

            // nodes
            _nodesPool[s] = reinterpret_cast<const Node *>(ptr);
            ptr += nc * sizeof(Node);

            uint32_t sc = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += sizeof(uint32_t);
            _roots[s] = reinterpret_cast<const uint32_t *>(ptr);
            ptr += sc * sizeof(uint32_t);

            // prefix sums
            _prefix[s] = reinterpret_cast<const long long *>(ptr);
            ptr += sc * sizeof(long long);

            _shapeCount[s] = sc;
        }
        // final cumulative totals
        _cumTotal[0] = 0;
        for (int s = 1; s <= MAX_SIZE; ++s) {
            _cumTotal[s] = *reinterpret_cast<const long long *>(ptr);
            ptr += sizeof(long long);
        }
    }

    ~ExpressionGenerator() {
        munmap(const_cast<char *>(_data), _fileSize);
        close(_fd);
    }

    long long totalExpressions() const { return _cumTotal[MAX_SIZE]; }

    std::string_view get_expression(long long n) const {
        if (n < 1 || n > _cumTotal[MAX_SIZE])
            return "ERROR";

        // find size layer s
        int s = 1;
        while (_cumTotal[s] < n)
            ++s;
        long long base = _cumTotal[s - 1];
        long long offset = n - (base + 1);

        // pick which shape index
        const long long *Pfx = _prefix[s];
        int sc = int(_shapeCount[s]);
        int idxSh = 0;
        while (Pfx[idxSh] < offset + 1)
            ++idxSh;
        long long prev = idxSh ? Pfx[idxSh - 1] : 0;
        long long resid = offset - prev;

        const Node *Np = _nodesPool[s];
        const uint32_t *Rts = _roots[s];
        uint32_t rootIdx = Rts[idxSh];
        int b = Np[rootIdx].numBinary;

        // decode op choices
        long long varCount = Bell[b + 1];
        long long opv = resid / varCount;
        long long varIdx = resid % varCount;
        for (int i = b - 1; i >= 0; --i) {
            _tmpOp[i] = int(opv % 3);
            opv /= 3;
        }

        // generate RGS if needed
        auto &rgsList = _rgsCache[b + 1];
        if (rgsList.empty()) {
            std::vector<int> cur{0};
            rgsList.reserve(Bell[b + 1]);
            generateRgs(1, 0, b + 1, cur, rgsList);
        }
        const auto &rgs = rgsList[varIdx];

        // assign variable names
        char next = 'A';
        ++_curStamp;
        for (int i = 0; i < b + 1; ++i) {
            int blk = rgs[i];
            if (_stamp[blk] != _curStamp) {
                _stamp[blk] = _curStamp;
                _chMap[blk] = next++;
            }
        }

        // build string
        _outLen = 0;
        int leafIdx = 0;
        build(Np, rootIdx, b, rgs.data(), 0, leafIdx);
        return {_outBuf.data(), _outLen};
    }

  private:
    void build(const Node *Np, uint32_t ni, int b, const int *rgs, int opi,
               int &leafIdx) const {
        const Node &n = Np[ni];
        if (n.type == 0) {
            _outBuf[_outLen++] = _chMap[rgs[leafIdx++]];
            return;
        }
        if (n.type == 1) {
            memcpy(_outBuf.data() + _outLen, "NOT(", 4);
            _outLen += 4;
            build(_nodesPool[n.childSize], n.u.child, b, rgs, opi, leafIdx);
            _outBuf[_outLen++] = ')';
            return;
        }
        // binary
        int op = _tmpOp[opi];
        const char *lit = (op == 0 ? "AND" : op == 1 ? "OR" : "XOR");
        size_t len = (op == 1 ? 2 : 3);
        memcpy(_outBuf.data() + _outLen, lit, len);
        _outLen += len;
        _outBuf[_outLen++] = '(';
        build(_nodesPool[n.childSize], n.u.br.left, b, rgs, opi + 1, leafIdx);
        _outBuf[_outLen++] = ',';
        build(_nodesPool[n.rightSize], n.u.br.right, b, rgs, opi + 1, leafIdx);
        _outBuf[_outLen++] = ')';
    }
};

int main() {
    ExpressionGenerator gen;
    long long n = 100;
    long long max = gen.totalExpressions();
    for (long long i = 1; i <= n; ++i)
        std::cout << "#" << i << ": " << gen.get_expression(i) << "\n";
    std::cout << "#" << max << ": " << gen.get_expression(max) << "\n";
    std::cout << "Total expressions: " << max << "\n";
    return 0;
}
