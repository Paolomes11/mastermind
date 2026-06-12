# Mastermind Entropy Solver

An optimal Mastermind solver implemented in C++17, using information-theoretic strategies to minimise the expected number of guesses. Comes with a CLI, a dedicated benchmark tool, and an interactive GUI with real-time entropy visualisation.

## Game rules

Mastermind is a code-breaking game. One player sets a secret code — a sequence of *P* colours chosen from *C* available colours (repetitions allowed). The solver guesses codes and receives feedback after each guess:

- **Black peg** — correct colour in the correct position
- **White peg** — correct colour in the wrong position

The goal is to deduce the secret code in as few guesses as possible.

## Build

Requires CMake ≥ 3.16 and a C++17-capable compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

This produces the following binaries inside `build/`:

| Binary | Purpose |
|---|---|
| `mastermind` | Interactive play and quick benchmark (CLI) |
| `mastermind_bench` | Dedicated benchmark with full statistics |
| `gui/mastermind_gui` | Interactive GUI with real-time charts |

### GUI dependencies

The GUI requires SDL2 and OpenGL (ImGui and ImPlot are fetched automatically by CMake):

```bash
sudo apt-get install -y libsdl2-dev   # Ubuntu / Debian
```

To build without the GUI:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF
```

## Interactive GUI

Launch the GUI:

```bash
./build/gui/mastermind_gui
```

The window is divided into two panels:

**Left — Game board**
- Coloured circles represent each guess; black and white dots show the feedback.
- The current (pending) guess is highlighted in gold.

**Right — Analysis charts**
- **Entropy (bits):** bar chart of the top-10 candidate guesses ranked by Shannon entropy. The solver's chosen move is highlighted in yellow.
- **Minimax (worst-case partition):** bar chart of the same top-10 guesses ranked by worst-case remaining candidates. Lower is better.
- **Candidates remaining:** line chart showing how the candidate set shrinks turn-by-turn (log scale).

Charts update in real time after each turn. Strategy scoring runs on a background thread so the UI stays responsive even for large configurations.

### Guided mode

Think of a secret code. The solver proposes guesses; you enter the feedback (black/white pegs) via sliders and click **Submit Feedback**. The solver uses your feedback to narrow down the candidates.

### Auto-Solve mode

Type in the secret code digit-by-digit, then press **Start Solving**. The solver animates the solution step by step. Use **Play / Pause / Step** to control the speed.

### Configuration

All parameters can be changed from the top bar **before** starting a game:

| Control | Range | Default |
|---|---|---|
| Colors | 2–8 | 6 |
| Positions | 2–6 | 4 |
| Strategy | entropy / minimax / random | entropy |

Click **New Game** to apply new settings and restart.

## CLI usage

### Interactive mode

The solver proposes guesses; you provide feedback after each one.

```bash
./build/mastermind -i
./build/mastermind -c 6 -p 4 -s entropy -i
```

Example session (6 colours, 4 positions):

```
Mastermind Solver
6 colors, 4 positions (1296 codes)
Building feedback table...

Interactive mode — entropy strategy
After each guess, enter feedback as: <blacks> <whites>

Guess 1: 1122
Feedback (blacks whites): 0 1
Guess 2: 2344
Feedback (blacks whites): 1 2
Guess 3: 3245
Feedback (blacks whites): 4 0

Solved in 3 guesses!
```

Enter feedback as two space-separated integers: `<blacks> <whites>`.
The game ends when you enter `<P> 0` (all pegs black).

### Benchmark mode

Runs every strategy against every possible secret code and reports aggregate statistics.

```bash
# Classic Mastermind (6 colours, 4 positions)
./build/mastermind -b

# Custom configuration
./build/mastermind -c 8 -p 4 -b

# Dedicated benchmark binary — more detailed output, strategy filtering
./build/mastermind_bench -c 6 -p 4
./build/mastermind_bench -c 6 -p 4 -s entropy   # single strategy
./build/mastermind_bench -c 6 -p 4 -v            # verbose progress
```

Example output:

```
=== Benchmark: 6 colors, 4 positions (1296 codes) ===

Strategy      Mean  StdDev   Min   Max  Time(ms)
------------------------------------------------
random       4.671   0.923     1     7         2
entropy      4.415   0.631     1     6      1730
minimax      4.476   0.618     1     5      1459

--- Turn distribution ---
Turns      random   entropy   minimax
--------------------------------------
1               3         1         1
2              15         4         6
3             100        71        62
4             395       612       533
5             579       596       694
6             183        12         0
7              21         0         0
```

## Options

### `mastermind`

| Flag | Default | Description |
|---|---|---|
| `-c, --colors N` | 6 | Number of colours (2–8) |
| `-p, --positions N` | 4 | Code length (2–6) |
| `-s, --strategy S` | `entropy` | Strategy: `random`, `entropy`, `minimax` |
| `-b, --benchmark` | — | Benchmark all three strategies |
| `-i, --interactive` | — | Interactive play |
| `-v, --verbose` | — | Progress output during benchmark |
| `-h, --help` | — | Show help |

### `mastermind_bench`

Same flags as above except `-i` is not available and `-s` accepts `all` (default) to run all strategies.

## Strategies

### Entropy (recommended)
Selects the guess that maximises Shannon entropy over the remaining candidate set:

```
H = -Σ p_i · log₂(p_i)
```

where each *p_i* is the fraction of candidates that would produce a given feedback outcome. This minimises the expected number of candidates remaining after the guess.

**6c4p results:** mean 4.415 guesses, worst case 6.

### Minimax (Knuth-style)
Selects the guess that minimises the worst-case number of remaining candidates after feedback. Guarantees a bounded worst case at the cost of a slightly higher average.

**6c4p results:** mean 4.476 guesses, worst case 5.

### Random
Picks a uniformly random remaining candidate. Useful as a performance baseline.

## Performance notes

| Config | Codes | Table | Benchmark time (entropy) |
|---|---|---|---|
| 4c4p | 256 | precomputed | < 1 s |
| 6c4p | 1 296 | precomputed | ~2 s |
| 5c5p | 3 125 | precomputed | ~20 s |
| 6c5p | 7 776 | on-the-fly | minutes |
| 8c5p | 32 768 | on-the-fly | ~30 min |

For configurations with ≤ 5 000 codes the full feedback table (N × N bytes) is precomputed at startup, making inner loops a single memory lookup. For larger configurations feedback is computed on-the-fly and the strategy search is restricted to the current candidate set.

The GUI runs strategy scoring on a background thread and always displays both entropy and minimax charts regardless of which strategy is active for solving, so the UI stays responsive even for slow configurations.
