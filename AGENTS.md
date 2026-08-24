# Bully Chess Engine - Developer & Agent Guide

Welcome, Agent! This file contains the complete architectural blueprints, design decisions, and strict coding rules for the **Bully** chess engine. Read this before making modifications or adding new files.

---

## 🛠️ Technological Stack & Build Pipeline
- **C++ Standard**: C++23
- **Compiler**: GCC 16.1.0+ (MinGW-w64 / MSYS2), Clang 15+ (Linux / macOS / Android NDK), or MSVC 2022+
- **Build System**: CMake 3.20+ with the **Ninja** generator
- **Optimization Flags (Release)**: `-O3`, `-march=native` (enables AVX2/BMI2), `-flto=auto` (Link-Time Optimization), and `-mbmi2`.
- **Warning Level**: Extremely strict warning flags are enabled:
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wunused -Woverloaded-virtual`
  *Rule: All code must compile across all target OSes with ABSOLUTELY ZERO warnings.*

---

## 📐 Core Architecture & Decisions

### 1. Namespace Isolation
All engine source code, variables, and definitions must be encapsulated inside `namespace Bully` to prevent scope pollution and naming conflicts.

### 2. Strong Types & Casting (`types.h`)
- We use strongly-typed enums (`Square`, `Piece`, `Color`, `Direction`, `File`, `Rank`) for safety.
- **Conversion Casts**: We use the C++23 helper `std::to_underlying(val)` (from `<utility>`) instead of standard `static_cast` for safety and readability.
- **`to_index` Helper**: Overloaded with C++20 concepts (`std::is_enum_v` and `std::is_integral_v`) to convert enums and integers safely to `size_t` for `std::array` indexing without triggering sign-conversion warnings:
  ```cpp
  // Correct way:
  SquareDistance[to_index(s1)][to_index(s2)]
  ```
- **[[nodiscard]]**: Apply the `[[nodiscard]]` attribute to all queries, helper functions, and operator overloads to prevent ignored return value bugs.

### 3. Packed Move Representation
- A `Move` is stored as a single `uint16_t` for L1/L2 cache efficiency and compact Transposition Table footprint:
  - **Bits 0-5**: Destination square (0-63)
  - **Bits 6-11**: Origin square (0-63)
  - **Bits 12-13**: Promotion piece type minus Knight (0-3)
  - **Bits 14-15**: Special flags (`NORMAL`, `PROMOTION`, `EN_PASSANT`, `CASTLING`)
- Sentinel moves are:
  - `Move::none()` (value 0)
  - `Move::null()` (value 65, used for Null Move Pruning)

### 4. Bitboard & Intrinsics (`bitboard.h`)
- A `Bitboard` is a raw `uint64_t`.
- We use standard C++20 `<bit>` functions for hardware-accelerated bit manipulation:
  - `std::popcount` (compiles to assembly instruction **`POPCNT`** on x86 or `CNT` on ARM NEON)
  - `std::countr_zero` (compiles to assembly instruction **`TZCNT`** or **`BSF`**)
- All lookup tables (`SquareDistance`, `LineBB`, `BetweenBB`, `PseudoAttacks`) are wrapped in `std::array` containers.

### 5. Precomputed Attacks & PEXT (`attacks.h`)
- Slider attacks (Rooks, Bishops, Queens) are computed using **BMI2 PEXT (Parallel Bit Extract)** when hardware support is enabled (`USE_PEXT`). This translates to a single CPU instruction:
  ```cpp
  RookAttacks[idx][_pext_u64(occ, RookMasks[idx])]
  ```
- **Magic Bitboards Fallback**: On non-BMI2 systems or ARM64 (Android), the engine falls back to a Magic Bitboard hash table. The magic numbers are dynamically discovered at startup using a fast sparse-random search.

### 6. Copy-Free Position State (`position.h`)
- Board tracking uses an incremental update system.
- To prevent slow board copying in search, non-reversible board history (castling, en passant target, halfmove clocks, captured pieces) is stored in a caller-allocated, stack-linked list structure: `StateInfo`.
- We use incremental 64-bit Zobrist key hashing for all board updates.
- **Accumulator Binding**: In `set_fen` and `make_move`, `st->accumulator` must always point to its own instance (`&st->accumulator_data`) to prevent invalid parent pointer aliasing during recursive move generation.

### 7. Transposition Table (`tt.h`)
- Stores searched positions to prevent redundant work.
- **Bucket/Cluster Layout**: Grouped into 32-byte cache-aligned clusters (fits half a standard CPU cache line). Each cluster holds exactly 3 entries (10 bytes each) and 2 bytes padding.
- **Fast Power-of-Two Indexing**: Resizes using power-of-two cluster counts so indexing performs a bitwise `AND` instead of slow modulo division.
- **Lock-Free XOR Tearing Prevention**: Critical fields (`move16`, `score16`, `eval16`) are XORed with the 16-bit key signature. A torn read fails key checksum check and is safely discarded without locks.
- **Commands**: `hash <MB>` resizes the table, `hash clear` flushes all entries. `ucinewgame` stops active search and flushes TT.

### 8. Search Engine & Unified API (`search.h`, `search.cpp`, `threadpool.h`)
- Implements Principal Variation Search (PVS) with Lazy SMP multithreading across persistent `WorkerThread` instances.
- **Zero-Allocation Search Workers**: All per-thread stacks (`history_stack`, `accumulators`, `move_list`, heuristics) are allocated once at thread creation and reused via in-place state resetting.
- **Unified Search API (`namespace Bully::Search`)**:
  - `start(pos, limits, history)`: Prepares workers in-place and launches search asynchronously.
  - `stop()`: Aborts search and joins worker threads.
  - `wait()`: Waits for search to finish naturally without aborting.
  - `is_searching()`: Queries whether search is currently active.
  - `set_threads(count)` / `get_threads()`: Dynamically manages worker pool size.
  - `set_multipv(count)` / `get_multipv()`: Configures multi-PV lines.
  - `get_last_search_nodes()`: Retrieves total nodes searched in the last run.
  - `Search::config`: Centralized struct containing all 12 togglable search heuristics (`config.nmp`, `config.lmr`, `config.rfp`, `config.lmp`, `config.fp`, `config.check_extensions`, `config.singular_extensions`, `config.aspiration_window`, `config.quiescence`, `config.tt`, `config.killers`, `config.history`).
- **Triangular PV Table**: Maintained in `SearchState::pv_table[MAX_PLY][MAX_PLY]` and `SearchState::pv_length[MAX_PLY]`. Reconstructs exact Principal Variation lines dynamically during search tree traversal whenever `alpha` improves, ensuring 100% accurate PV line and `bestmove` rendering regardless of TT state.

### 9. Syzygy Endgame Tablebases (`syzygy.h`, `syzygy.cpp`)
- Integrated via **Pyrrhic** header-only C++20 probing library.
- Bundles 3-piece tablebases (~26 KB total) in `./syzygy` directory.
- Root DTZ probing produces instant perfect move execution at 1 node.
- In-search WDL probing prunes drawn or lost nodes early in `pvs()`.
- Interactive CLI commands: `syzygy [<path>|on|off|clear]` and UCI option `SyzygyPath`.

### 10. Positional & NNUE Evaluation (`evaluation.h`, `nnue.h`)
- Uses a **Tapered Static Evaluation** mechanism as classical fallback, and NNUE (HalfKP architecture) for deep neural evaluation.
- Interpolates between Middlegame and Endgame Piece-Square Tables (PST) based on the remaining non-pawn material (24 total phase points down to 0).
- **Symmetry Mirroring**: All PSTs are defined for White. Black indexes are mirrored vertically using XOR 56 (`sq ^ 56`) to maintain symmetry and eliminate duplicate tables.
- **Zero Heap Allocations**: NNUE accumulator traversal inside `update_accumulator()` must strictly use fixed stack arrays (`std::array<const StateInfo*, MAX_PLY>`) rather than heap `std::vector` to maintain 1.07+ M NPS speed.

### 11. `constexpr` Naming & Feature Flags
- `constexpr` constants must strictly use **`CamelCase`** (e.g. `EngineVersion`, `HasPext`, `TransformerHalfDim`, `HalfKPFeatures`).
- Reserve `ALL_CAPS` strictly for preprocessor macros (e.g., `USE_PEXT`, `USE_VNNI`).
- Use C++23 compile-time branching `if constexpr (HasPext)` instead of scattering `#ifdef` blocks inside engine functions.

### 12. Evaluation Score Representation (`using Value = int`)
- `Value` is defined as 32-bit `int` (`using Value = int;`) for all search and evaluation logic.
- Operating natively on 32-bit `int` eliminates C++ unary minus integer promotion warnings (`-Wimplicit-int-conversion-on-negation` on Clang) and prevents overflow when combining evaluation bonuses.
- Transposition Table entries pack scores into `int16_t` (`score16`, `eval16`) to maintain the 32-byte cluster memory footprint.

---

## 🚨 Compiler Pitfalls & Strict Rules

### 1. MSVC Compiler Heap Exhaustion (`fatal error C1060`)
- **Never** use inline default aggregate initializers `{}` for large multi-megabyte array structures inside header files (`search.h`, `nnue.h`, etc.), e.g.:
  ```cpp
  // WRONG: Triggers C1060 out of memory in MSVC template parser:
  int cont_history_1[1024][16][64]{};

  // CORRECT: Declare uninitialized and clear inside a constructor:
  int cont_history_1[1024][16][64];
  SharedHeuristics() { clear(); }
  ```

### 2. MSVC Alignment Warnings (`C4324`)
- Aligned SIMD structures (`alignas(64)`) like `Accumulator` and `NetworkWeights` emit MSVC warning `C4324`.
- Wrap aligned structures in MSVC `#pragma warning(push)` / `#pragma warning(disable: 4324)` blocks.

### 3. MinGW / GCC Print Linker Bug
- MinGW GCC 16 has a known runtime linking bug with C++23 `<print>` (`undefined reference to std::__open_terminal`).
- **Rule**: Do NOT use `<print>`, `std::print`, or `std::println`. Use standard `std::cout` combined with C++20 `std::format` instead:
  ```cpp
  // INCORRECT (will fail to link on Windows):
  std::println("id name {}", EngineName);

  // CORRECT:
  std::cout << std::format("id name {}\n", EngineName);
  ```

---

## 🎨 Interactive CLI Terminal Styling & Alignment
When the engine runs in interactive CLI mode (detected via `is_interactive()`):
- **ANSI Color Scheme**: Yellow (Commands/Actions/White), Green (Metadata/Files/Ranks), Magenta (Values/Results), Blue (Borders/Brackets), Cyan (Accents/Black).
- **ANSI Visible Alignment Rule**: When formatting CLI menus (`options`, `help`, `position`), padding spaces must be calculated based on **visible text characters** (excluding ANSI escape sequences `\033[...]`) so colons `: ` line up at identical terminal columns.
- **Unicode Fallback**: Detects UTF-8 via `detect_utf8()` (CodePage 65001 on Windows, `LANG` env var on POSIX). Automatically uses 3D UTF-8 block art for `use_utf8 == true` and clean 5-line ASCII art for `use_utf8 == false`.
- **Platform Binary Name**: `BinaryName` resolves to `bully.exe` on Windows (`#ifdef _WIN32`) and `bully` on POSIX/macOS/Android.

---

## 📱 Android Cross-Compilation
Bully is 100% compatible with Android NDK (ARM64 / `arm64-v8a`):
- Cross-platform POSIX headers (`<unistd.h>`, `isatty`) handle non-Windows terminal checks.
- Build command:
  ```bash
  cmake .. -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -GNinja
  ninja
  ```

---

## 🧪 Unit Testing & Perft Verification
- Run the unit test executable after modifying move generation, evaluation, or search logic:
  ```powershell
  build\tests.exe
  ```
- **Perft Legal Move Rule**: In perft test loops, `pos.unmake_move(m)` must strictly reside *inside* `if (pos.make_move(m, next_si))` to avoid corrupting `Position` state when moves are illegal.

---

## 📊 Elo Rating Strength Testing Procedure

To measure strength gains for new search or evaluation features, the engine relies on a standardized, automated local tournament pipeline:

### 1. File Organization
*   **`engines/`**: Holds testing binaries (`bully-base.exe`, `bully-<patch_name>.exe`).
*   **`results/`**: Holds outputs (`match_<patch_name>.pgn`, `ratings_<patch_name>.txt`).
*   **`books/`**: Holds opening books (`noob_3moves.epd`).

### 2. The Automation Script (`run_test.ps1`)
The testing script automatically manages compiling, backing up binaries, running the tournament, and computing final Elo.

**How to run a test:**
1. Code your changes.
2. Compile the binary: `ninja -C build`
3. Execute the script in PowerShell:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\run_test.ps1 -PatchName <my_patch_name> -Rounds 150
   ```

### 3. Under the Hood
*   **CuteChess-CLI**: Plays matches using `tc=1+0.05` across multiple CPU threads.
*   **Ordo**: Resolves rating estimates and 95% confidence intervals from recorded PGNs, anchoring `Base` to `2300.0` Elo.
*   **Interpreting Results**: Look for positive Elo diff in `results/ratings_<patch_name>.txt` and high **LOS (Likelihood of Superiority)** (>95%).
