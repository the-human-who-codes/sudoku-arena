#include "sudoku/generator.hpp"

#include <cstdlib>

#include "search_base.hpp"

namespace sudoku {
namespace {

struct LevelSpec {
  int minClues;     // Stop removing clues at this count.
  int maxAttempts;  // Fresh grids to try before settling for the closest level.
};

constexpr LevelSpec specFor(Level level) {
  switch (level) {
    case Level::Easy: return {38, 20};
    case Level::Medium: return {28, 50};
    case Level::Hard: return {24, 200};
    case Level::Expert: return {22, 300};
    case Level::Master: return {17, 200};
  }
  return {17, 1};
}

bool fill(Grid& grid, detail::Occupancy& occupancy, Rng& rng) {
  int best = -1;
  Mask bestMask = 0;
  for (int cell = 0; cell < kCells; ++cell) {
    if (grid[cell] != 0) continue;
    const Mask mask = occupancy.allowed(cell);
    if (mask == 0) return false;
    if (best < 0 || countDigits(mask) < countDigits(bestMask)) {
      best = cell;
      bestMask = mask;
    }
  }
  if (best < 0) return true;

  std::array<std::uint8_t, 9> digits{};
  std::size_t count = 0;
  for (Mask m = bestMask; m != 0; m &= m - 1) digits[count++] = static_cast<std::uint8_t>(firstDigit(m));
  rng.shuffle(digits, count);
  for (std::size_t i = 0; i < count; ++i) {
    grid[best] = digits[i];
    occupancy.set(best, digits[i]);
    if (fill(grid, occupancy, rng)) return true;
    occupancy.clear(best, digits[i]);
    grid[best] = 0;
  }
  return false;
}

}  // namespace

Grid randomSolution(Rng& rng) {
  Grid grid{};
  detail::Occupancy occupancy;
  fill(grid, occupancy, rng);
  return grid;
}

GeneratedPuzzle generate(Level level, std::uint64_t seed) {
  const LevelSpec spec = specFor(level);
  Rng rng(seed);
  GeneratedPuzzle best;
  int bestDistance = -1;

  for (int attempt = 1; attempt <= spec.maxAttempts; ++attempt) {
    const Grid solution = randomSolution(rng);
    Grid puzzle = solution;
    int clues = kCells;

    // Remove clues in symmetric pairs (cell and its 180-degree rotation), keeping a
    // removal only if the solution stays unique and the puzzle no harder than asked.
    std::array<std::uint8_t, 41> order{};
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<std::uint8_t>(i);
    rng.shuffle(order);
    for (const int cell : order) {
      const int mirror = kCells - 1 - cell;
      const int removed = cell == mirror ? 1 : 2;
      if (clues - removed < spec.minClues) continue;
      const auto saved = puzzle[cell];
      const auto savedMirror = puzzle[mirror];
      puzzle[cell] = puzzle[mirror] = 0;
      bool keep = countSolutions(puzzle, 2) == 1;
      if (keep && level != Level::Master) {
        const auto rating = rate(puzzle);
        keep = rating && rating->level <= level;
      }
      if (keep) {
        clues -= removed;
      } else {
        puzzle[cell] = saved;
        puzzle[mirror] = savedMirror;
      }
    }

    const auto rating = rate(puzzle);
    if (!rating) continue;
    const int distance = std::abs(static_cast<int>(rating->level) - static_cast<int>(level));
    if (bestDistance < 0 || distance < bestDistance) {
      best = {puzzle, solution, *rating, attempt};
      bestDistance = distance;
    }
    if (distance == 0) break;
  }
  return best;
}

std::uint64_t seedFromString(std::string_view text) {
  if (!text.empty() && text.size() <= 19 &&
      text.find_first_not_of("0123456789") == std::string_view::npos) {
    std::uint64_t value = 0;
    for (const char ch : text) value = value * 10 + static_cast<std::uint64_t>(ch - '0');
    return value;
  }
  std::uint64_t hash = 0xCBF29CE484222325ULL;
  for (const char ch : text) {
    hash ^= static_cast<unsigned char>(ch);
    hash *= 0x100000001B3ULL;
  }
  return hash;
}

}  // namespace sudoku
