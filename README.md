# ♟️ Bully Chess Engine

Bully is a high-performance, command-line chess engine optimized for hardware-level execution speed, deep tactical search, and seamless integration with chess GUIs.

---

## ⚡ Core Features

* **Extreme Search Speed**: Reaches search speeds of **over 1.7 Million Nodes Per Second (NPS)**, allowing it to calculate millions of positions in milliseconds.
* **Hardware-Accelerated Attacks**: Uses native CPU **BMI2 PEXT (Parallel Bit Extract)** hardware instructions to query Rook, Bishop, and Queen attacks instantly in a single instruction. Includes a fast sparse-random Magic Bitboard fallback for older processors.
* **Tapered Positional Evaluation**: Dynamically evaluates positions by blending middlegame positional rules (like king safety and pawn structures) with active endgame positioning based on the remaining pieces on the board.
* **Lock-Free Transposition Cache**: Uses a 32-byte cache-aligned hash table with XOR-obfuscation to safely read and write searched positions across multiple threads without lock overhead.
* **Interactive CLI**: Auto-detects your terminal environment to print clean, high-contrast UTF-8 chess grids with manual toggles for legacy command prompts.
* **UCI Standard Compliant**: Works out of the box with popular chess GUI wrappers (Arena, Cute Chess, Fritz, ChessBase, etc.).

---

## 🛠️ How to Compile

Bully uses **CMake** and the **Ninja** build system.

### 1. Choose Your Architecture Profile
You can compile specialized binaries for different computer configurations:
* **`native`** (Default): Optimizes specifically for your current computer's CPU.
* **`bmi2`**: Targets modern processors supporting AVX2/BMI2 (`bully-bmi2`).
* **`avx2`**: Targets AVX2 processors (`bully-avx2`).
* **`modern`**: Targets SSE4.2/POPCNT processors (`bully-modern`).
* **`generic`**: Highly portable binary that runs on any 64-bit CPU (`bully`).

### 2. Build Commands
Configure CMake with your chosen profile:
```bash
# Example: Compile for your local machine
cmake -B build -DBULLY_ARCH=native -G Ninja
```

Build the executable:
```bash
ninja -C build
```
*(The optimized executable will be located in the `build/` directory).*

---

## 🚀 How to Run

### 1. GUI / Chess Arena Mode (UCI)
When started by a chess GUI, Bully boots silently and complies with the standard UCI protocol. It waits for standard GUI commands (`uci`, `position`, `go`).

### 2. Interactive Console Mode
If you launch the executable directly in your command line, Bully greets you with an interactive developer console:
```bash
.\build\bully-native.exe
```

#### Handy Console Commands:
* **`d` / `display`**: Draw the board using a clean grid.
* **`position startpos`**: Load the standard chess starting layout.
* **`position fen <FEN>`**: Load a custom position (e.g. `position fen r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1`).
* **`go [depth <D>]`**: Search the current position for the best move.
* **`stop`**: Stop a running search thread immediately.
* **`perft <depth>`**: Run a speed test and count leaf nodes recursively.
* **`hash <MB>`**: Dynamically resize the transposition hash table in Megabytes.
* **`utf8 [on | off]`**: Toggle between Unicode chess pieces and standard ASCII.
* **`quit`**: Exit the program.

---

## 🧪 Testing

Bully includes a full GoogleTest suite to validate bitboards, attack generators, board hashing, and transposition tables:

```bash
ctest --test-dir build --output-on-failure
```

---

## 📝 License
This project is licensed under the MIT License - see the [LICENSE](file:///C:/Users/diego/Desktop/Programming/c++/bully/LICENSE) file for details.
