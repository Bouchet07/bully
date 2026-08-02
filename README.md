# ♟️ Bully Chess Engine

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android-lightgrey.svg?style=flat-square)](#)
[![UCI Protocol](https://img.shields.io/badge/protocol-UCI%20Compliant-orange.svg?style=flat-square)](#)
[![Tests Status](https://img.shields.io/badge/tests-83%20%2F%2083%20passed-brightgreen.svg?style=flat-square)](#)

**Bully** is a high-performance, modern C++23 chess engine designed for hardware-level calculation speed, deep tactical evaluation, and full compatibility with popular chess GUIs and tournament platforms.

---

## ✨ Features at a Glance

* ⚡ **Extreme Multi-Threaded Speed**: Scales seamlessly across CPU cores, achieving over **74 Million Nodes Per Second (NPS)** on perft tests and millions of NPS during deep search traversal.
* 🧠 **NNUE & Tapered Evaluation**: Supports both efficient HalfKP Neural Network (NNUE) evaluation and a hand-crafted tapered static evaluation, seamlessly togglable via UCI.
* 🧩 **Syzygy Endgame Tablebases**: Native integration with 3-4-5 piece Syzygy tablebases for instant, flawless endgame execution (WDL in search & DTZ at root).
* 🖥️ **Universal Chess Interface (UCI)**: 100% compliant with standard UCI protocol for instant setup in Arena, CuteChess, Fritz, ChessBase, Banksia GUI, or Lichess bots.
* 🎨 **Rich Interactive CLI**: Features a full-featured terminal interface with 3D UTF-8 board rendering, ANSI color highlights, real-time streaming move analysis (`divide`), and benchmark diagnostics (`bench`).
* 🌍 **Universal Cross-Platform Architecture**: Pre-compiled binaries tuned specifically for every modern hardware platform (x86-64 AVX-512/VNNI, AVX2/BMI2, Apple Silicon ARM64 DotProd, and Android).

---

## 📦 Binary Downloads & Architecture Selection

Pre-built binaries for Windows, Linux, macOS, and Android are published under [GitHub Releases](https://github.com/Bouchet07/bully/releases). 

Choose the binary that best matches your system's processor for maximum playing speed:

| Executable Suffix | Architecture & Instruction Sets | Supported Hardware / CPUs | Performance Advantage |
| :--- | :--- | :--- | :--- |
| **`bully-avx512-vnni`** | x86-64-v4 (`AVX-512`, `VNNI`, `BMI2`, `PEXT`) | Intel 12th+ Gen (Alder/Raptor Lake), AMD Zen 4/5 | **Fastest x86 Speed**; 512-bit vector registers + hardware VNNI NNUE math |
| **`bully-avx512`** | x86-64-v4 (`AVX-512F/BW/DQ/VL`, `BMI2`, `PEXT`) | Intel Skylake-X / Ice Lake, AMD Zen 4 | Ultra-wide 512-bit SIMD vector accumulators |
| **`bully-bmi2`** | x86-64-v3 (`AVX2`, `BMI2`, `PEXT`) | Intel Haswell+ (~2013+), AMD Zen 3+ | Hardware CPU slider lookups (`PEXT`) in 1 cycle + AVX2 evaluation |
| **`bully-avx2`** | x86-64-v3 (`AVX2`, Magic Bitboards) | AMD Zen 1/Zen 2, legacy AVX2 systems | 256-bit SIMD evaluation with sparse-random magic lookup fallback |
| **`bully-modern`** | x86-64-v2 (`SSE4.2`, `POPCNT`) | Older 64-bit Intel/AMD CPUs (~2008+) | Hardware bit-count acceleration (`POPCNT`) |
| **`bully`** *(generic)* | x86-64 (v1 baseline) | Any standard 64-bit CPU | Maximum compatibility across older machines |
| **`bully-macos-universal`** | Apple Silicon (`arm64`) + Intel (`x86_64`) | All Mac computers (M1/M2/M3/M4 & Intel Macs) | Native macOS universal binary execution |
| **`bully-armv8-dotprod`** | ARM64 (`armv8.2-a+dotprod`) | Apple Silicon, Snapdragon 855+, Tensor | Hardware 8-bit Dot Product instructions (`vdotq_s32`), **2x–3x faster ARM NNUE** |
| **`bully-android-arm64`** | ARM64 (`arm64-v8a`) | Android smartphones & tablets | Native 64-bit mobile ARM execution |

> **Tip**: If you're unsure which binary to download, try `bully-bmi2` (for modern x86 PCs) or `bully-macos-universal` (for Mac). If the engine starts without error, you are good to go!

---

## 🎮 Connecting Bully to a Chess GUI

Bully communicates using standard UCI protocol.

### Popular Chess GUIs:
1. **CuteChess**: `Tools` ➡️ `Options` ➡️ `Engines` ➡️ `Add...` ➡️ Select binary.
2. **Arena**: `Engine` ➡️ `Install New Engine...` ➡️ Select binary ➡️ Choose `UCI`.
3. **ChessBase / Fritz**: `Engine` ➡️ `Create UCI Engine` ➡️ Select binary.
4. **Banksia GUI**: `Engines` ➡️ `Add Engine` ➡️ Select binary.

### Configurable Engine Options (UCI):

You can tune options directly in your GUI or via `setoption name <Name> value <Value>`:

| UCI Option Name | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| **`Hash`** | Spin (1 - 65536) | `16` | Transposition Table memory size in Megabytes (MB). |
| **`Threads`** | Spin (1 - 512) | `1` | Number of CPU search threads for parallel execution. |
| **`SyzygyPath`** | String | `syzygy` | Directory path containing `.rtbw` and `.rtbz` Endgame Tablebase files. |
| **`nnue`** | Check | `false` | Toggles NNUE neural network evaluation (`true`) vs Classical static evaluation (`false`). |
| **`nmp`** | Check | `true` | Null Move Pruning search heuristic. |
| **`lmr`** | Check | `true` | Late Move Reduction search heuristic. |
| **`rfp`** | Check | `true` | Reverse Futility Pruning. |
| **`lmp`** | Check | `true` | Late Move Pruning. |
| **`fp`** | Check | `true` | Futility Pruning. |

---

## 💻 Interactive Terminal CLI Mode

Running the executable directly in your command line launches Bully's interactive terminal console.

```bash
./build/bully-native
```

### Essential CLI Commands:

```text
  d                   Renders board position in 3D UTF-8 block art
  position startpos   Loads default starting chess board
  position fen <FEN>  Loads board position from a custom FEN string
  go depth <D>        Starts search to specific depth limit
  go movetime <ms>    Starts search for specific time duration
  perft <depth>       Executes recursive node counting perft test
  divide <depth>      Executes perft test streaming root moves in real-time
  bench [depth]       Runs standard 8-position performance benchmark suite
  options             Lists all engine heuristic options and current states
  syzygy <path>       Loads Syzygy tablebases from specified directory path
  quit                Exits the engine
```

---

## 🛠️ Compiling From Source

### Prerequisites:
- **C++ Compiler**: GCC 11+ or Clang 13+ (or MSVC 2022+) with C++23 support.
- **Build Tools**: CMake 3.20+ and Ninja (recommended).

### Quick Build:

```bash
# Clone repository with submodules
git clone --recursive https://github.com/Bouchet07/bully.git
cd bully

# Configure build directory (Native host optimization)
cmake -B build -DBULLY_ARCH=native -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile binary & tests
ninja -C build

# Run unit tests
./build/tests
```

### Custom Architecture Builds:

Pass `-DBULLY_ARCH=<target>` to CMake to target specific hardware:

```bash
cmake -B build -DBULLY_ARCH=bmi2 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -B build -DBULLY_ARCH=avx512 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -B build -DBULLY_ARCH=armv8-dotprod -G Ninja -DCMAKE_BUILD_TYPE=Release
```

---

## 📖 Developer Guide & Architecture

For full architectural blueprints, coding conventions, namespace rules, and Elo tournament testing procedures, please refer to [AGENTS.md](AGENTS.md).

---

## 📝 License

Bully is released under the open-source **[MIT License](LICENSE)**.
