# Bully Chess Engine - Developer & Agent Guide

Welcome, Agent! This file contains the complete architectural blueprints, design decisions, and strict coding rules for the **Bully** chess engine. Read this before making modifications or adding new files.

---

## 🛠️ Technological Stack & Build Pipeline
- **C++ Standard**: C++23
- **Compiler**: GCC 16.1.0 (MinGW-w64 / MSYS2)
- **Build System**: CMake 3.20+ with the **Ninja** generator
- **Optimization Flags (Release)**: `-O3`, `-march=native` (enables AVX2/BMI2), `-flto=auto` (Link-Time/Interprocedural Optimization), and `-mbmi2`.
- **Warning Level**: Extremely strict warning flags are enabled:
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wunused -Woverloaded-virtual`
  *Rule: All code must compile with ABSOLUTELY ZERO warnings.*

---

## 📐 Core Architecture & Decisions

### 1. Namespace Isolation
All engine source code, variables, and definitions must be encapsulated inside `namespace Bully` to prevent scope pollution and naming conflicts.

### 2. Strong Types & Casting (`types.h`)
- We use strongly-typed enums (`Square`, `Piece`, `Color`, `Direction`, `File`, `Rank`) for safety.
- **Conversion Casts**: We use the C++23 helper `std::to_underlying(val)` (from `<utility>`) instead of standard `static_cast` for safety and readability.
- **`to_index` Helper**: Because indexing `std::array` requires `size_t` and our enum underlying types are signed, indexing `std::array[std::to_underlying(val)]` triggers sign-conversion warnings. 
  *Rule: Always use the `to_index(val)` helper defined in `types.h` when indexing arrays.*
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
  - `std::popcount` (compiles to assembly instruction **`POPCNT`**)
  - `std::countr_zero` (compiles to assembly instruction **`TZCNT`** or **`BSF`**)
- All lookup tables (`SquareDistance`, `LineBB`, `BetweenBB`, `PseudoAttacks`) are wrapped in `std::array` containers.

### 5. Precomputed Attacks & PEXT (`attacks.h`)
- Slider attacks (Rooks, Bishops, Queens) are computed using **BMI2 PEXT (Parallel Bit Extract)** when hardware support is enabled (`USE_PEXT`). This translates to a single CPU instruction:
  ```cpp
  RookAttacks[idx][_pext_u64(occ, RookMasks[idx])]
  ```
- **Magic Bitboards Fallback**: On non-BMI2 systems, the engine falls back to a Magic Bitboard hash table. The magic numbers are dynamically discovered at startup using a fast sparse-random search.

### 6. Copy-Free Position State (`position.h`)
- Board tracking uses an incremental update system.
- To prevent slow board copying in search, non-reversible board history (castling, en passant target, halfmove clocks, captured pieces) is stored in a caller-allocated, stack-linked list structure: `StateInfo`.
- We use incremental 64-bit Zobrist key hashing for all board updates.

### 7. Transposition Table (`tt.h`)
- Stores searched positions to prevent redundant work.
- **Bucket/Cluster Layout**: Grouped into 32-byte cache-aligned clusters (fits half a standard CPU cache line). Each cluster holds exactly 3 entries (10 bytes each) and 2 bytes padding.
- **Fast Power-of-Two Indexing**: Resizes using power-of-two cluster counts so indexing performs a bitwise `AND` instead of slow modulo division.
- **Lock-Free XOR Tearing Prevention**: Critical fields (`move16`, `score16`, `eval16`) are XORed with the 16-bit key signature. A torn read (occurring during concurrent thread writes in search) fails the key checksum check and is safely discarded without locks.

### 8. Search Engine & Ordering (`search.h`)
- Implements Principal Variation Search (PVS) with Iterative Deepening.
- Searches on a detached worker thread to keep the main UCI thread listening for user inputs (like `stop`).
- **Pruning & Reductions**: Uses Null Move Pruning (NMP) for active branches, and Late Move Reduction (LMR) for quiet moves ordered late in the list.
- **Heuristics**:
  - PV Move (best move from TT).
  - MVV-LVA capture scoring.
  - Killer moves (quiet cutoffs at the same ply).
  - History heuristics (cutoff occurrences multiplied by depth squared).

### 9. Positional Evaluation (`evaluation.h`)
- Uses a **Tapered Static Evaluation** mechanism.
- Interpolates between Middlegame and Endgame Piece-Square Tables (PST) based on the remaining non-pawn material (24 total phase points down to 0).
- **Symmetry Mirroring**: All PSTs are defined for White. Black indexes are mirrored vertically using XOR 56 (`sq ^ 56`) to maintain symmetry and eliminate duplicate tables.

---

## ⚠️ Important MinGW/Windows Print Warnings
MinGW GCC 16 has a known runtime linking bug with C++23 `<print>` (causing undefined references to `std::__open_terminal` and `std::__write_to_terminal` when linking static binaries).
*Rule: Do NOT use `<print>`, `std::print`, or `std::println`. Use standard `std::cout` combined with C++20 `std::format` instead:*
```cpp
// INCORRECT (will fail to link on Windows):
std::println("id name {}", ENGINE_NAME);

// CORRECT:
std::cout << std::format("id name {}\n", ENGINE_NAME);
```

---

## 🎨 Interactive CLI Terminal Styling & Color Convention
When the engine runs in interactive CLI mode (detected via `is_interactive()`), it uses ANSI terminal escape sequences to style output. To keep the visual presentation coherent, all CLI text must strictly follow this semantic color convention:
- **Yellow (`\033[1;33m`)**: Actions, commands, executable title/logo, and primary player pieces (White).
- **Green (`\033[1;32m`)**: Metadata attributes, option labels, coordinate files/ranks, and keyword parameters.
- **Magenta (`\033[1;35m`)**: Dynamic values, variable placeholders, parameters, and evaluated results.
- **Blue (`\033[1;34m`)**: Borders, grids, frame delimiters, and structural syntax brackets (`[ ]`).
- **Cyan (`\033[1;36m`)**: Branding accents and secondary player pieces (Black).
- **Reset (`\033[0m`)**: Used immediately after any style escape to prevent color bleed.

*Rule: Terminal colors must be initialized using `CLIStyle` and must only print color sequences when color support is toggled on (`use_color` is true).*

---

## 📊 Elo Rating Strength Testing Procedure

To measure strength gains for new search or evaluation features, the engine relies on a standardized, automated local tournament pipeline:

### 1. File Organization
*   **`engines/`**: Holds the testing binaries.
    *   `bully-base.exe`: The frozen reference binary.
    *   `bully-<patch_name>.exe`: Binaries of developed features/branches.
*   **`results/`**: Holds the outputs.
    *   `match_<patch_name>.pgn`: Recorded game notation.
    *   `ratings_<patch_name>.txt`: Elo calculation outputs.
*   **`books/`**: Holds standard opening books.
    *   `noob_3moves.epd`: Balanced, depth-3 opening positions used to minimize statistical noise.

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
*   **CuteChess-CLI**: Plays matches using `tc=1+0.05` (1 second per game plus a 0.05-second increment), executing games concurrently across multiple CPU threads.
*   **Ordo**: Resolves rating estimates and 95% confidence intervals from the recorded PGN. It anchors the baseline engine (`Base`) to `2300.0` Elo as a stable rating reference.
*   **Interpreting Results**:
    *   Look at the rating diff in `results/ratings_<patch_name>.txt`.
    *   Look for a high **LOS (Likelihood of Superiority)** (e.g. >95%) to confirm the patch is genuinely stronger.

