# ♟️ Bully Chess Engine

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android-lightgrey.svg?style=flat-square)](#)
[![Tests Status](https://img.shields.io/badge/tests-79%20%2F%2079%20passed-brightgreen.svg?style=flat-square)](#)

Bully is a professional, high-performance C++23 chess engine optimized for hardware-level execution speed, deep tactical search, and seamless integration with standard chess GUIs. 

---

## ⚡ Key Highlights

- **Extreme Calculation Speed**: Capable of processing **over 1.7 Million Nodes Per Second (NPS)**.
- **Hardware-Accelerated Sliders**: Leverages native x86-64 **BMI2 PEXT** instructions to query Rook, Bishop, and Queen attacks in a single CPU cycle. Automatically falls back to a fast, sparse-random Magic Bitboard generator on non-BMI2/ARM systems.
- **Tapered Static Evaluation**: Interpolates between Middlegame and Endgame Piece-Square Tables (PST) dynamically based on the remaining non-pawn material (24 total phase points).
- **Lock-Free Cache-Aligned TT**: Uses a 32-byte cache-aligned Transposition Table (TT) cluster layout with lock-free XOR-signature tearing prevention for maximum CPU efficiency.
- **Triangular PV Table**: Maintains a precise triangular Principal Variation table to reconstruct exact PV lines dynamically under any transposition table conditions.
- **Endgame Tablebase Probing**: Integrates Pyrrhic C++20 endgame tablebases (up to 3 pieces bundled natively) with sorted root DTZ probing for instant, perfect move execution.

---

## 🛠️ Compilation & Installation

Bully utilizes **CMake** and **Ninja** for professional, cross-platform build pipelines.

### 1. Build Profile Options
Configure your build by selecting the target CPU architecture option (`-DBULLY_ARCH`):

| Build Profile | CPU Instructions & Architecture Targets | Executable Output |
|:---|:---|:---|
| **`native`** (Default) | Optimizes specifically for the compiling host CPU features. | `bully-native` |
| **`bmi2`** | Targets AVX2 and BMI2 hardware (Intel Haswell+, AMD Zen3+). | `bully-bmi2` |
| **`avx2`** | Targets AVX2 hardware but falls back to Magic Bitboards. | `bully-avx2` |
| **`modern`** | Targets SSE4.2 and hardware POPCNT. | `bully-modern` |
| **`generic`** | Base x86-64 compatibility (runs on any 64-bit CPU). | `bully` |

### 2. Compilation Commands

```bash
# Configure the build directory (Example: native profile)
cmake -B build -DBULLY_ARCH=native -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile the engine
ninja -C build
```

The optimized executable binary will be created inside the `./build/` directory.

---

## 🎮 Interface & Integration

### 1. Connecting to a Chess GUI
Bully complies fully with the standard **UCI (Universal Chess Interface)** protocol. You can load it directly into Arena, Cute Chess, Fritz, ChessBase, or any other compatible GUI wrapper by registering the compiled binary executable.

### 2. Interactive CLI Mode
If you launch the engine binary directly in a terminal console, it automatically enters interactive developer mode:

```bash
./build/bully-native
```

#### Supported Developer Commands:

| Command | Action |
|:---|:---|
| `d` / `display` | Renders a high-contrast UTF-8 chess board representation. |
| `position startpos` | Loads the standard chess starting layout. |
| `position fen <FEN>` | Loads a custom board position using FEN representation. |
| `go [depth <D> \| infinite]` | Launches PVS search from the current position. |
| `stop` | Safe, immediate termination of background search threads. |
| `perft <depth>` | Runs recursive node generation speed tests. |
| `hash <MB>` | Resizes the transposition table (TT) memory bounds. |
| `syzygy <path>` | Loads/reloads endgame tablebases from path. |
| `options` | Inspects and toggles engine heuristics search options. |
| `quit` | Gracefully closes the engine. |

---

## 🧪 Testing Suite

Bully comes with a comprehensive GoogleTest suite containing 79 individual test cases to verify core components.

```bash
# Rebuild and run all tests under CTest
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 📝 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
