#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "sudoku/grid.hpp"
#include "sudoku/logic.hpp"

namespace sudoku {

/// SplitMix64. Unlike the std distributions, its output is identical on every
/// platform, so a seed names the same puzzle in the browser, the CLI and a server.
class Rng {
 public:
  explicit Rng(std::uint64_t seed) : state_(seed) {}

  std::uint64_t next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  /// Uniform integer in [0, bound), without modulo bias.
  std::uint32_t below(std::uint32_t bound) {
    const std::uint32_t threshold = (0u - bound) % bound;
    while (true) {
      const auto value = static_cast<std::uint32_t>(next() >> 32);
      if (value >= threshold) return value % bound;
    }
  }

  /// Fisher-Yates shuffle of the first `count` items.
  template <typename T, std::size_t N>
  void shuffle(std::array<T, N>& items, std::size_t count = N) {
    for (std::size_t i = count; i > 1; --i) {
      std::swap(items[i - 1], items[below(static_cast<std::uint32_t>(i))]);
    }
  }

 private:
  std::uint64_t state_;
};

struct GeneratedPuzzle {
  Grid puzzle{};
  Grid solution{};
  Rating rating;     ///< May differ from the requested level if every attempt missed it.
  int attempts = 0;  ///< Candidate puzzles built before one matched.
};

/// Builds a symmetric puzzle with a unique solution at `level`. Deterministic:
/// the same level and seed always give the same puzzle.
GeneratedPuzzle generate(Level level, std::uint64_t seed);

/// A random complete grid.
Grid randomSolution(Rng& rng);

/// Decimal strings map to their value; any other text is hashed (FNV-1a).
std::uint64_t seedFromString(std::string_view text);

}  // namespace sudoku
