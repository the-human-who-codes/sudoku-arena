#include <array>

#include "search_base.hpp"

namespace sudoku {
namespace {

class SmartSearch : detail::SearchBase {
 public:
  using SearchBase::SearchBase;

  SearchResult run() {
    if (!occupancy_.load(grid_)) return invalid();
    search();
    return finish();
  }

 private:
  using Forced = std::array<std::uint8_t, kCells>;

  // Returns true when the search should stop.
  bool search() {
    Forced forced;
    int forcedCount = 0;
    const bool consistent = propagate(forced, forcedCount);
    if (aborted_) return true;

    if (consistent) {
      // Branch on the most constrained cell (minimum remaining values).
      int best = -1;
      Mask bestMask = 0;
      for (int cell = 0; cell < kCells; ++cell) {
        if (grid_[cell] != 0) continue;
        const Mask mask = occupancy_.allowed(cell);
        if (best < 0 || countDigits(mask) < countDigits(bestMask)) {
          best = cell;
          bestMask = mask;
          if (countDigits(mask) <= 2) break;
        }
      }
      if (best < 0) {
        if (foundSolution()) return true;
      } else {
        const auto kind =
            countDigits(bestMask) > 1 ? SearchTrace::Kind::Guess : SearchTrace::Kind::Forced;
        for (Mask m = bestMask; m != 0; m &= m - 1) {
          if (!place(best, firstDigit(m), kind)) return true;
          if (search()) return true;
          undo(best);
        }
      }
    }

    while (forcedCount > 0) undo(forced[--forcedCount]);
    return false;
  }

  // Fills naked and hidden singles until none remain. Returns false on a
  // contradiction (a cell or a digit with nowhere to go) or when the budget runs out.
  bool propagate(Forced& forced, int& count) {
    bool progress = true;
    while (progress) {
      progress = false;
      for (int cell = 0; cell < kCells; ++cell) {
        if (grid_[cell] != 0) continue;
        const Mask mask = occupancy_.allowed(cell);
        if (mask == 0) return false;
        if (countDigits(mask) == 1) {
          if (!place(cell, firstDigit(mask), SearchTrace::Kind::Forced)) return false;
          forced[count++] = static_cast<std::uint8_t>(cell);
          progress = true;
        }
      }
      for (const auto& unit : kTopology.unitCells) {
        Mask once = 0;
        Mask twice = 0;
        Mask placed = 0;
        for (const int cell : unit) {
          if (grid_[cell] != 0) {
            placed |= digitBit(grid_[cell]);
          } else {
            const Mask mask = occupancy_.allowed(cell);
            twice |= once & mask;
            once |= mask;
          }
        }
        if ((once | placed) != kAllDigits) return false;
        for (Mask singles = once & ~twice; singles != 0; singles &= singles - 1) {
          const int digit = firstDigit(singles);
          for (const int cell : unit) {
            if (grid_[cell] == 0 && (occupancy_.allowed(cell) & digitBit(digit)) != 0) {
              if (!place(cell, digit, SearchTrace::Kind::Forced)) return false;
              forced[count++] = static_cast<std::uint8_t>(cell);
              progress = true;
              break;
            }
          }
          // If no cell was found, an earlier single took the only spot; the next
          // pass reports that digit as missing.
        }
      }
    }
    return true;
  }
};

}  // namespace

SearchResult solveSmart(const Grid& puzzle, const SearchOptions& options) {
  return SmartSearch(puzzle, options).run();
}

int countSolutions(const Grid& puzzle, int limit) {
  return solveSmart(puzzle, {.solutionLimit = limit}).solutions;
}

std::string_view toString(SolveStatus status) {
  switch (status) {
    case SolveStatus::Solved: return "solved";
    case SolveStatus::Unsolvable: return "unsolvable";
    case SolveStatus::Invalid: return "invalid";
    case SolveStatus::Stuck: return "stuck";
    case SolveStatus::LimitReached: return "limit";
  }
  return "unknown";
}

}  // namespace sudoku
