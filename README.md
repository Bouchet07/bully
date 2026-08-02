# ♟️ Bully Chess Engine

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android-lightgrey.svg?style=flat-square)](#)
[![UCI Protocol](https://img.shields.io/badge/protocol-UCI%20Compliant-orange.svg?style=flat-square)](#)
[![Tests Status](https://img.shields.io/badge/tests-83%20%2F%2083%20passed-brightgreen.svg?style=flat-square)](#)

**Bully** is a high-performance, modern C++23 chess engine designed for hardware-level calculation speed, deep tactical evaluation, and seamless UCI compatibility.

---

## ⚡ Key Highlights

* 🚀 **Extreme Multi-Threaded Speed**: Scales across CPU cores, achieving over **74 Million Nodes Per Second (NPS)** on perft tests and millions of NPS during deep search.
* 🧠 **NNUE & Tapered Evaluation**: Supports both efficient HalfKP Neural Network (NNUE) evaluation and a hand-crafted tapered static evaluation, dynamically togglable.
* 🧩 **Syzygy Endgame Tablebases**: Native C++20 probing for 3-4-5 piece Syzygy tablebases (WDL in search & DTZ at root).
* 🎨 **Interactive Console**: Includes a built-in terminal interface with 3D UTF-8 board rendering, real-time streaming move analysis (`divide`), and performance diagnostics.
* 🌍 **Hardware-Tuned Builds**: Optimized pre-compiled binaries for x86-64 (AVX-512 VNNI, AVX2/BMI2), Apple Silicon (ARM64 DotProd), and Android.

---

## 📦 Binary Releases & Target Architectures

Download pre-compiled binaries from [GitHub Releases](https://github.com/Bouchet07/bully/releases):

| Executable | Architecture & Feature Set | Recommended CPU / System | Performance Advantage |
| :--- | :--- | :--- | :--- |
| **`bully-avx512-vnni`** | x86-64-v4 (`AVX-512`, `VNNI`, `BMI2`, `PEXT`) | Intel 12th+ Gen (Alder/Raptor Lake), AMD Zen 4/5 | **Fastest x86 Speed**: 512-bit SIMD registers + hardware VNNI NNUE math |
| **`bully-avx512`** | x86-64-v4 (`AVX-512`, `BMI2`, `PEXT`) | Intel Skylake-X / Ice Lake, AMD Zen 4 | Wide 512-bit vector accumulators (updates 32 features in half the cycles) |
| **`bully-bmi2`** | x86-64-v3 (`AVX2`, `BMI2`, `PEXT`) | Intel Haswell+ (~2013+), AMD Zen 3+ | Hardware CPU slider lookups (`PEXT`) in 1 cycle + 256-bit AVX2 evaluation |
| **`bully-avx2`** | x86-64-v3 (`AVX2`, Magic Bitboards) | AMD Zen 1 / Zen 2, legacy AVX2 systems | 256-bit SIMD evaluation with sparse-random magic lookup fallback |
| **`bully-modern`** | x86-64-v2 (`SSE4.2`, `POPCNT`) | Older 64-bit Intel/AMD CPUs (~2008+) | Hardware bit-count acceleration (`POPCNT`) |
| **`bully`** *(generic)* | x86-64 (v1 baseline) | Standard 64-bit baseline CPUs | Universal compatibility across all 64-bit machines |
| **`bully-macos-universal`** | Apple Silicon (`arm64`) + Intel (`x86_64`) | All Mac computers (M1/M2/M3/M4 & Intel Macs) | Native macOS universal binary execution |
| **`bully-armv8-dotprod`** | ARM64 (`armv8.2-a+dotprod`) | Apple Silicon, Snapdragon 855+, Tensor | Hardware 8-bit Dot Product instructions, **2x–3x faster ARM NNUE** |
| **`bully-android-arm64`** | ARM64 (`arm64-v8a`) | Android smartphones & tablets | Native 64-bit mobile ARM execution |

---

## ⚡ What Each Hardware Feature Brings to the Table

* **`PEXT` (Parallel Bit Extract - BMI2)**: Calculates Rook, Bishop, and Queen slider attack bitboards in a single CPU clock cycle using native hardware bit extraction, completely eliminating cache misses and lookup latency.
* **`VNNI` (Vector Neural Network Instructions)**: Executes integer dot products in a single instruction (`_mm512_dpbusd_epi32`), boosting NNUE neural network matrix evaluation speed by **1.5x to 2x**.
* **`AVX-512` (512-bit Vector Extensions)**: Doubles the vector register width to 512 bits, allowing Bully to update 32 NNUE piece accumulators in half the SIMD instruction cycles compared to 256-bit AVX2.
* **`DotProd` (ARM64 Dot Product)**: Native ARM hardware dot-product instruction (`vdotq_s32`), accelerating NNUE matrix multiplication on Apple Silicon (M1/M2/M3/M4) and modern mobile devices by **2x to 3x**.
* **`AVX2` (256-bit Vector Arithmetic)**: 256-bit SIMD vector operations processing 16 16-bit integer calculations simultaneously per cycle.
* **`POPCNT` (Population Count)**: Single CPU instruction for instantaneous bit counting on 64-bit bitboards, accelerating mobility and piece counting.

---

## 🛠️ Compiling From Source

### Prerequisites:
- GCC 11+ or Clang 13+ (or MSVC 2022+) with C++23 support
- CMake 3.20+ and Ninja generator

```bash
# Clone repository with submodules
git clone --recursive https://github.com/Bouchet07/bully.git
cd bully

# Configure build (Native host optimization)
cmake -B build -DBULLY_ARCH=native -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile binary & tests
ninja -C build

# Run unit test suite
./build/tests
```

To compile for a specific target architecture, pass `-DBULLY_ARCH=<target>` to CMake (`bmi2`, `avx2`, `avx512`, `avx512-vnni`, `armv8-dotprod`, `modern`, `generic`).

---

## 📖 Developer Guide

For architectural blueprints, C++23 standards, namespace design, and local Elo tournament testing procedures, see [AGENTS.md](AGENTS.md).

---

## 📝 License

Bully is released under the **[MIT License](LICENSE)**.
