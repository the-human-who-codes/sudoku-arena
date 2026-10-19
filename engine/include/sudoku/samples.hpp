#pragma once

#include <array>
#include <string_view>

namespace sudoku {

struct Sample {
  std::string_view name;
  std::string_view puzzle;
};

/// Well-known puzzles, from a newspaper classic to some of the hardest ever published.
inline constexpr std::array<Sample, 5> kSamples = {{
    {"Classic", "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79"},
    {"AI Escargot",
     "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3.."},
    {"Inkala 2012",
     "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4.."},
    {"Easter Monster",
     "1.......2.9.4...5...6...7...5.9.3.......7.......85..4.7.....6...3...9.8...2.....1"},
    // 17 clues, built so that the top row solves to 987654321: worst case for naive backtracking.
    {"Anti-Brute-Force",
     "..............3.85..1.2.......5.7.....4...1...9.......5......73..2.1........4...9"},
}};

}  // namespace sudoku
