# CircFinity

**CircFinity** is a browser-based visualizer and enumerator for Boolean logic expressions, combining a C++/WASM backend with a minimal React + Vite frontend. It generates, evaluates, and visualizes canonical logic circuits.

**Deployed**: [https://blankprogram.github.io/circfinity/](https://blankprogram.github.io/circfinity/)

## Features

- **Deterministic Enumeration** of all Boolean expressions of a given size using:
  - Preorder shape signatures over `{L, U, B}`
  - Operator sequences (`{AND, OR, XOR}` via ternary indices)
  - Variable partitions via **Restricted Growth Strings (RGS)**
- **Canonical Variable Labels** using a base-26 encoding
- **Efficient Evaluation** via structured DAGs and flat JSON input
- **Massive Index Support** with `cpp_int` (up to ~10¹⁵³ expressions)
- **Fast Unranking** via precomputed tables for:
  - Shape counts `C[s][u]`
  - Operator counts `3^k`
  - Set partitions (Bell numbers, `RGS`)
- **WASM Integration** using Emscripten

## Theory

- Sawada, Williams & Wong (2017): [_Practical Algorithms to Rank Necklaces, Lyndon Words, and de Bruijn Sequences_](https://www.socs.uoguelph.ca/~sawada/papers/ranking.pdf)
- Orlov (2002): [_Efficient Generation of Set Partitions_](https://noexec.org/public/papers/partitions.pdf)

## Implementation

Each index `N` maps to:

```text
N ↦ (shape, operator sequence, variable partition)
```

Where:

- **Shape**: preorder string over `{L, U, B}`
- **Operators**: ternary index over binary nodes
- **Partition**: RGS assigning variables to leaves

This guarantees **uniqueness** and **reproducibility** for any expression index.

Example:

```cpp
std::string out = get_expr_full(large_number!);
```

## Build & Run

```bash
# Build C++ → WASM
cd wasm
./build.sh

# Start frontend
cd frontend
npm ci
npm run dev
```
